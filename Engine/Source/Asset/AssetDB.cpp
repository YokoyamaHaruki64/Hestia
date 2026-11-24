/*=============================================================================

 File   : AssetDB.cpp
 Desc   : .metaを走査してAssetのGUIDとPathを索引化する。

------------------------------------------------------------------------------

 Date   : 2026/10/15
 Author : Yokoyama Haruki

=============================================================================*/

#include "pch.h"
#include "Common/Include/StringUtils.h"
#include "Asset/AssetDB.h"
#include "Log/Logger.h"

#define RYML_SINGLE_HDR_DEFINE_NOW
#include "RapidYaml/rapidyaml.hpp"

#include <bit>
#include <charconv>
#include <format>
#include <fstream>
#include <iterator>
#include <objbase.h>
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
            { ".prefab", AssetType::Prefab },
        };

        std::string_view TypeName(AssetType type)
        {
            switch (type)
            {
            case AssetType::Model: return "Model";
            case AssetType::Prefab: return "Prefab";
            case AssetType::Texture: return "Texture";
            case AssetType::MaterialShader: return "MaterialShader";
            case AssetType::Mesh: return "Mesh";
            }
            return {};
        }

        bool MetaError(const std::filesystem::path& path, std::string_view reason)
        {
            Logger::Error(std::format("AssetDB '{}': {}", PathToUtf8(path), reason));
            return false;
        }

        bool ReadId(ryml::ConstNodeRef node, uint32_t& value)
        {
            if (!node.has_val() || node.val().empty())
                return false;

            const auto text = node.val();
            const auto result = std::from_chars(text.begin(), text.end(), value);
            return result.ec == std::errc{} && result.ptr == text.end() && value != 0;
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

    bool AssetDB::Initialize(const std::filesystem::path& assetsRoot)
    {
        decltype(m_entries) entries;
        decltype(m_paths) paths;
        std::error_code error;

        // 元ファイル単位でメタデータを検証し、揃った索引だけを公開する
        // recursive_directory_iteratorでroot以下を走査
        for (std::filesystem::recursive_directory_iterator it(assetsRoot, error), end;
            !error && it != end; it.increment(error))
        {
            if (!it->is_regular_file(error))
                continue;

            const auto& path = it->path();
            const auto extension = ToLowerAscii(PathToUtf8(path.extension()));
            if (extension == ".meta")
            {
                auto source = path;
                source.replace_extension(); // .metaを除いた元ファイルのパス
                if (!std::filesystem::exists(source, error) && !error)
                    Logger::Warning(std::format("Asset source is missing for '{}': '{}'", PathToUtf8(path), PathToUtf8(source)));
                continue;
            }

            // 対応拡張子以外は無視する
            const auto supported = SUPPORTED_EXTENSIONS.find(extension);
            if (supported == SUPPORTED_EXTENSIONS.end())
                continue;

            AssetInfo info;
            info.m_type = supported->second;
            info.m_path = PathToUtf8(path.lexically_relative(assetsRoot));
            auto metaPath = path;
            metaPath += L".meta";
            const bool hasMeta = std::filesystem::exists(metaPath, error);
            if (error)
                return MetaError(metaPath, error.message());

            if (hasMeta)
            {
                if (!ReadMeta(metaPath, info))
                    return false;
            }
            else
            {
                GUID guid{};
                if (FAILED(CoCreateGuid(&guid)))
                    return MetaError(metaPath, "Could not generate GUID");

                info.m_guid.m_words = std::bit_cast<decltype(info.m_guid.m_words)>(guid);
                if (!WriteMeta(metaPath, info))
                    return false;
            }

            // GUIDの重複やパスの重複を検出する
            if (const auto existing = entries.find(info.m_guid); existing != entries.end())
                return MetaError(path, std::format("Duplicate GUID also used by '{}'", existing->second.m_path));
            if (!paths.emplace(ToLowerAscii(info.m_path), info.m_guid).second)
                return MetaError(path, "Duplicate asset path");

            // 成功した索引だけを一時Mapに集める
            entries.emplace(info.m_guid, std::move(info));
        }

        if (error)
            return MetaError(assetsRoot, error.message());

        m_root = assetsRoot;
        m_entries.swap(entries);
        m_paths.swap(paths);
        return true;
    }

    bool AssetDB::ReadMeta(const std::filesystem::path& path, AssetInfo& info)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file)
            return MetaError(path, "Could not open .meta");

        const std::string source((std::istreambuf_iterator<char>(file)), {});
        if (file.bad())
            return MetaError(path, "Could not read .meta");

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
            return MetaError(path, failure.m_message);
        }

        // 型とGUIDを確認し、モデルだけSubAssetの対応を読む
        const auto root = tree.crootref();
        if (!root.is_map())
            return MetaError(path, "Expected a YAML map");

        std::unordered_set<std::string_view> keys;
        for (const auto field : root.children())
        {
            if (!keys.emplace(field.key().str, field.key().len).second)
                return MetaError(path, "Duplicate .meta field");
        }

        // 型とGUIDを確認する
        const auto type = root.find_child("type");
        const auto expected = TypeName(info.m_type);    // 型は拡張子により決定済み
        if (!type.readable() || !type.has_val() || type.val() != ryml::to_csubstr(expected))
            return MetaError(path, "type does not match the source extension");

        const auto guid = root.find_child("guid");
        if (!guid.readable() || !guid.has_val() ||
            !(info.m_guid = AssetGuid::Parse({ guid.val().str, guid.val().len })))
            return MetaError(path, "Invalid or missing GUID");

        if (info.m_type != AssetType::Model)
            return true;

        // モデルはSubAssetの割り当てを読み込む
        const auto subAssets = root.find_child("subAssets");
        const auto materials = root.find_child("materials");
        const auto next = root.find_child("nextLocalId");
        if (!subAssets.readable() && !materials.readable() && !next.readable())
            return true;
        if (!subAssets.readable() || !subAssets.is_map() || !next.readable() || !ReadId(next, info.m_nextLocalId))
            return MetaError(path, "Expected subAssets and a positive nextLocalId");

        std::unordered_set<uint32_t> ids;
        // MeshとMaterialをまたいでIDの衝突を検出する
        for (const auto& [group, entries] :
            { std::pair{subAssets, &info.m_subAssets}, std::pair{materials, &info.m_materials} })
        {
            if (!group.readable())
                continue;
            if (!group.is_map())
                return MetaError(path, "Expected a SubAsset map");
            for (const auto node : group.children())
            {
                uint32_t id = 0;
                const auto key = node.key();
                if (key.empty() || !ReadId(node, id) || id >= info.m_nextLocalId ||
                    !ids.insert(id).second || !entries->emplace(std::string(key.str, key.len), id).second)
                    return MetaError(path, "Invalid or duplicate SubAsset key or ID");
            }
        }
        return true;
    }

    bool AssetDB::WriteMeta(const std::filesystem::path& path, const AssetInfo& info)
    {
        ryml::Tree tree;
        auto root = tree.rootref();
        root |= ryml::MAP;
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
        temporary += L".tmp";
        std::ofstream file(temporary, std::ios::binary);
        file << ryml::emitrs_yaml<std::string>(tree);
        file.close();
        if (file && MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING))
            return true;

        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        return MetaError(path, "Could not write .meta");
    }

    const AssetInfo* AssetDB::FindByGuid(const AssetGuid& guid) const
    {
        const auto found = m_entries.find(guid);
        return found == m_entries.end() ? nullptr : &found->second;
    }

    const AssetInfo* AssetDB::FindByPath(std::string_view path) const
    {
        const auto normalized = PathFromUtf8(path).lexically_normal();
        const auto found = m_paths.find(ToLowerAscii(PathToUtf8(normalized)));
        return found == m_paths.end() ? nullptr : FindByGuid(found->second);
    }

    std::string AssetDB::SourcePath(const AssetInfo& info) const
    {
        return PathToUtf8(m_root / PathFromUtf8(info.m_path));
    }

    bool AssetDB::SaveModelMeta(const AssetInfo& previous, const AssetInfo& updated) const
    {
        if (previous.m_subAssets == updated.m_subAssets && previous.m_materials == updated.m_materials)
            return true;

        const auto source = SourcePath(updated);
        auto path = PathFromUtf8(source);
        path += L".meta";
        return WriteMeta(path, updated);
    }
}
