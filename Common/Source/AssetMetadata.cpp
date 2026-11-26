/*=============================================================================

 File   : AssetMetadata.cpp
 Desc   : Assetメタデータを検証・生成し、索引に必要な値を返す。

------------------------------------------------------------------------------

 Date   : 2026/11/25
 Author : Yokoyama Haruki

=============================================================================*/

#include "AssetMetadata.h"
#include "StringUtils.h"
#include "WindowsHeaders.h"

#define RYML_SINGLE_HDR_DEFINE_NOW
#include "RapidYaml/rapidyaml.hpp"

#include <algorithm>
#include <bit>
#include <charconv>
#include <format>
#include <fstream>
#include <iterator>
#include <objbase.h>
#include <optional>
#include <unordered_set>

#pragma comment(lib, "ole32.lib")

namespace Hestia
{
    namespace
    {
        const std::unordered_map<std::string_view, AssetType> SUPPORTED_EXTENSIONS =
        {
            { ".fbx", AssetType::Model },
            { ".obj", AssetType::Model },
            { ".gltf", AssetType::Model },
            { ".glb", AssetType::Model },
            { ".png", AssetType::Texture },
            { ".matsh", AssetType::MaterialShader },
            { ".material", AssetType::Material },
            { ".prefab", AssetType::Prefab },
            { ".scene", AssetType::Scene },
            { ".cpp", AssetType::Script },
        };

        std::string_view TypeName(AssetType type)
        {
            switch (type)
            {
            case AssetType::Mesh: return "Mesh";
            case AssetType::Texture: return "Texture";
            case AssetType::MaterialShader: return "MaterialShader";
            case AssetType::Material: return "Material";
            case AssetType::Model: return "Model";
            case AssetType::Prefab: return "Prefab";
            case AssetType::Scene: return "Scene";
            case AssetType::Script: return "Script";
            }
            return {};
        }

        bool MetaError(std::string& error, const std::filesystem::path& path, std::string_view reason)
        {
            error = std::format("'{}': {}", PathToUtf8(path), reason);
            return false;
        }

        std::filesystem::path MetadataPath(std::filesystem::path source)
        {
            source += L".meta";
            return source;
        }

        bool ReadId(ryml::ConstNodeRef node, uint32_t& value)
        {
            if (!node.has_val() || node.val().empty())
                return false;

            const auto text = node.val();
            const auto result = std::from_chars(text.begin(), text.end(), value);
            return result.ec == std::errc{} && result.ptr == text.end() && value != 0;
        }

        bool ReadFile(const std::filesystem::path& path, AssetInfo& info, std::string& error)
        {
            std::ifstream file(path, std::ios::binary);
            if (!file)
                return MetaError(error, path, "Could not open .meta");

            const std::string source((std::istreambuf_iterator<char>(file)), {});
            if (file.bad())
                return MetaError(error, path, "Could not read .meta");

            // RapidYAMLの構文エラーを初期化の失敗として返す
            struct ParseError { std::string m_message; };
            auto callbacks = ryml::get_callbacks();
            callbacks.set_error_parse([](ryml::csubstr message, const ryml::ErrorDataParse& data, void*)
            {
                throw ParseError{ std::format("Line {}, column {}: {}",
                    data.ymlloc.line, data.ymlloc.col, std::string_view(message.str, message.len)) };
            });

            ryml::EventHandlerTree handler(callbacks);
            ryml::Parser parser(&handler);
            ryml::Tree tree;
            try
            {
                tree = ryml::parse_in_arena(&parser, ryml::to_csubstr(source));
            }
            catch (const ParseError& failure)
            {
                return MetaError(error, path, failure.m_message);
            }

            // ファイル形式と共通項目を確認してから、モデル固有データを読む
            const auto root = tree.crootref();
            if (!root.is_map())
                return MetaError(error, path, "Expected a YAML map");

            std::unordered_set<std::string_view> keys;
            for (const auto field : root.children())
            {
                if (!keys.emplace(field.key().str, field.key().len).second)
                    return MetaError(error, path, "Duplicate .meta field");
            }

            const auto version = root.find_child("version");
            uint32_t number = 0;
            if (version.readable() && (!ReadId(version, number) || number != 1))
                return MetaError(error, path, "Unsupported .meta version");

            // 型とGUIDを確認する
            const auto type = root.find_child("type");
            const auto expected = TypeName(info.m_type);    // 型は拡張子により決定済み
            if (!type.readable() || !type.has_val() || type.val() != ryml::to_csubstr(expected))
                return MetaError(error, path, "type does not match the source extension");

            const auto guid = root.find_child("guid");
            if (!guid.readable() || !guid.has_val() ||
                !(info.m_guid = AssetGuid::Parse({ guid.val().str, guid.val().len })))
                return MetaError(error, path, "Invalid or missing GUID");

            if (info.m_type != AssetType::Model)
                return true;

            // モデルはSubAssetの割り当てを読み込む
            const auto subAssets = root.find_child("subAssets");
            const auto materials = root.find_child("materials");
            const auto next = root.find_child("nextLocalId");
            if (!subAssets.readable() && !materials.readable() && !next.readable())
                return true;
            if (!subAssets.readable() || !subAssets.is_map() || !next.readable() || !ReadId(next, info.m_nextLocalId))
                return MetaError(error, path, "Expected subAssets and a positive nextLocalId");

            std::unordered_set<uint32_t> ids;
            // MeshとMaterialをまたいでIDの衝突を検出する
            for (const auto& [group, entries] :
                { std::pair{subAssets, &info.m_subAssets}, std::pair{materials, &info.m_materials} })
            {
                if (!group.readable())
                    continue;
                if (!group.is_map())
                    return MetaError(error, path, "Expected a SubAsset map");
                for (const auto node : group.children())
                {
                    uint32_t id = 0;
                    const auto key = node.key();
                    if (key.empty() || !ReadId(node, id) || id >= info.m_nextLocalId ||
                        !ids.insert(id).second || !entries->emplace(std::string(key.str, key.len), id).second)
                        return MetaError(error, path, "Invalid or duplicate SubAsset key or ID");
                }
            }
            return true;
        }

        bool WriteFile(const std::filesystem::path& path, const AssetInfo& info, DWORD flags, std::string& error)
        {
            ryml::Tree tree;
            auto root = tree.rootref();
            root |= ryml::MAP;
            root["version"].save(1);
            root["type"].save(ryml::to_csubstr(TypeName(info.m_type)));
            root["guid"].save(ryml::to_csubstr(info.m_guid.ToString()));
            if (info.m_nextLocalId != 1 || !info.m_subAssets.empty())
            {
                root["nextLocalId"].save(info.m_nextLocalId);
                for (const auto& [name, entries] :
                    { std::pair{std::string_view("subAssets"), &info.m_subAssets},
                      std::pair{std::string_view("materials"), &info.m_materials} })
                {
                    if (name == "materials" && entries->empty())
                        continue;
                    auto group = root[ryml::to_csubstr(name)];
                    group |= ryml::MAP;
                    for (const auto& [key, id] : *entries)
                    {
                        auto node = group.append_child();
                        node.set_key(tree.to_arena(ryml::to_csubstr(key)));
                        node.set_key_style(ryml::KEY_DQUO);
                        node.save(id);
                    }
                }
            }

            // 元の割り当てを途中の書き込み失敗で失わないよう、一時ファイルから置き換える
            auto temporary = path;
            temporary += L"." + std::to_wstring(GetCurrentProcessId()) + L".tmp";
            std::ofstream file(temporary, std::ios::binary);
            file << ryml::emitrs_yaml<std::string>(tree);
            file.close();
            if (file && MoveFileExW(temporary.c_str(), path.c_str(), flags))
                return true;

            std::error_code ignored;
            std::filesystem::remove(temporary, ignored);
            return MetaError(error, path, "Could not write .meta");
        }

        bool CreateFileMetadata(const std::filesystem::path& path, AssetInfo& info, std::string& error)
        {
            GUID guid{};
            if (FAILED(CoCreateGuid(&guid)))
                return MetaError(error, path, "Could not generate GUID");

            info.m_guid.m_words = std::bit_cast<decltype(info.m_guid.m_words)>(guid);
            return WriteFile(path, info, 0, error);
        }
    }

    AssetGuid AssetGuid::Parse(std::string_view text)
    {
        if (text.size() != 36)
            return {};

        const std::wstring formatted = L"{" + std::wstring(text.begin(), text.end()) + L"}";
        GUID guid{};
        if (FAILED(IIDFromString(formatted.c_str(), &guid)))
            return {};

        return { std::bit_cast<decltype(m_words)>(guid) };
    }

    std::string AssetGuid::ToString() const
    {
        const GUID guid = std::bit_cast<GUID>(m_words);
        wchar_t text[39]{};
        StringFromGUID2(guid, text, static_cast<int>(std::size(text)));
        std::string result;
        result.reserve(36);
        for (const wchar_t c : std::wstring_view(text + 1, 36))
            result.push_back(static_cast<char>(c));
        return ToLowerAscii(result);
    }

    bool AssetMetadata::Read(const std::filesystem::path& source, AssetInfo& info, std::string& error)
    {
        const auto type = SUPPORTED_EXTENSIONS.find(ToLowerAscii(PathToUtf8(source.extension())));
        if (type == SUPPORTED_EXTENSIONS.end())
            return MetaError(error, source, "Unsupported asset extension");

        AssetInfo metadata;
        metadata.m_type = type->second;
        if (!ReadFile(MetadataPath(source), metadata, error))
            return false;

        info = std::move(metadata);
        return true;
    }

    bool AssetMetadata::Ensure(const std::filesystem::path& source, AssetInfo& info, std::string& error)
    {
        const auto type = SUPPORTED_EXTENSIONS.find(ToLowerAscii(PathToUtf8(source.extension())));
        if (type == SUPPORTED_EXTENSIONS.end())
            return MetaError(error, source, "Unsupported asset extension");

        std::error_code status;
        if (!std::filesystem::is_regular_file(source, status))
            return MetaError(error, source, "Asset source is missing");

        const auto path = MetadataPath(source);
        const bool exists = std::filesystem::exists(path, status);
        if (status)
            return MetaError(error, path, status.message());

        AssetInfo metadata;
        metadata.m_type = type->second;
        if (!(exists ? ReadFile(path, metadata, error) : CreateFileMetadata(path, metadata, error)))
            return false;

        info = std::move(metadata);
        return true;
    }

    bool AssetMetadata::SaveModel(const std::filesystem::path& source, const AssetInfo& info, std::string& error)
    {
        return WriteFile(MetadataPath(source), info, MOVEFILE_REPLACE_EXISTING, error);
    }

    bool AssetMetadata::Scan(const std::filesystem::path& root, std::span<const std::string_view> extensions,
        std::vector<AssetInfo>& entries, std::vector<std::string>& warnings, std::string& error)
    {
        // 空の絞り込みは全拡張子を対象にする
        std::vector<std::string> filter;
        for (const auto extension : extensions)
        {
            auto normalized = ToLowerAscii(extension);
            if (!SUPPORTED_EXTENSIONS.contains(normalized))
                return MetaError(error, root, "Unsupported asset extension: " + normalized);

            filter.push_back(std::move(normalized));
        }
        const auto isSelected = [&](std::string_view extension)
        {
            return filter.empty() || std::ranges::find(filter, extension) != filter.end();
        };

        // 既存のメタデータを検証し、不足している元ファイルはGUIDが空のまま控える
        std::vector<AssetInfo> scanned;
        std::unordered_map<AssetGuid, std::string, AssetGuid::Hash> guids;
        std::error_code status;
        for (std::filesystem::recursive_directory_iterator it(root, status), end;
            !status && it != end; it.increment(status))
        {
            const auto& path = it->path();
            const bool isFile = it->is_regular_file(status);
            if (status)
                return MetaError(error, path, status.message());
            if (!isFile)
                continue;

            const auto extension = ToLowerAscii(PathToUtf8(path.extension()));
            if (extension == ".meta")
            {
                auto source = path;
                source.replace_extension();
                const auto sourceExtension = ToLowerAscii(PathToUtf8(source.extension()));
                if (!isSelected(sourceExtension))
                    continue;
                const bool exists = std::filesystem::exists(source, status);
                if (status)
                    return MetaError(error, path, status.message());
                if (!exists)
                    warnings.push_back(std::format("Asset source is missing for '{}': '{}'",
                        PathToUtf8(path), PathToUtf8(source)));
                continue;
            }

            const auto type = SUPPORTED_EXTENSIONS.find(extension);
            if (type == SUPPORTED_EXTENSIONS.end() || !isSelected(extension))
                continue;

            AssetInfo info;
            info.m_type = type->second;
            info.m_path = PathToUtf8(path.lexically_relative(root));
            const auto meta = MetadataPath(path);
            const bool exists = std::filesystem::exists(meta, status);
            if (status)
                return MetaError(error, meta, status.message());
            if (exists)
            {
                if (!ReadFile(meta, info, error))
                    return false;
                const auto [existing, inserted] = guids.emplace(info.m_guid, info.m_path);
                if (!inserted)
                    return MetaError(error, path, std::format("Duplicate GUID also used by '{}'", existing->second));
            }

            scanned.push_back(std::move(info));
        }

        if (status)
            return MetaError(error, root, status.message());

        // 不正な既存ファイルがないことを確認してから不足分を公開する
        for (auto& info : scanned)
        {
            if (!info.m_guid && !CreateFileMetadata(MetadataPath(root / PathFromUtf8(info.m_path)), info, error))
                return false;
        }

        entries = std::move(scanned);
        return true;
    }
}
