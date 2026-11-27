/*=============================================================================

 File   : YamlFile.cpp
 Desc   : Assetで共有するYAMLのファイル入出力と値変換

------------------------------------------------------------------------------

 Date   : 2026/11/26
 Author : Yokoyama Haruki

=============================================================================*/

#include "pch.h"
#include "Serialize/YamlFile.h"
#include "Common/Include/StringUtils.h"
#include "Log/Logger.h"

#include <atomic>
#include <charconv>
#include <cmath>
#include <fstream>
#include <iterator>
#include <limits>
#include <type_traits>

namespace Hestia
{
    namespace
    {
        std::string_view View(ryml::csubstr text)
        {
            return { text.str, text.len };
        }

        template<class T>
        bool ReadNumber(std::string_view text, T& value)
        {
            if (text.empty())
                return false;
            const auto end = text.data() + text.size();
            const auto result = std::from_chars(text.data(), end, value);
            return result.ec == std::errc{} && result.ptr == end;
        }

        template<class T>
        bool ReadNumber(ryml::ConstNodeRef node, T& value)
        {
            if (!node.readable() || !node.has_val())
                return false;

            return ReadNumber(View(node.val()), value);
        }
    }

    std::optional<ryml::Tree> YamlReader::Read(const std::filesystem::path& path, std::string& error)
    {
        // ファイルの本文をTreeのArenaへ取り込み、呼び出し元に所有権を渡す
        std::ifstream file(path, std::ios::binary);
        if (!file)
        {
            error = "could not open file";
            return std::nullopt;
        }
        const std::string source((std::istreambuf_iterator<char>(file)), {});
        if (file.bad())
        {
            error = "could not read file";
            return std::nullopt;
        }

        // RapidYAMLの構文エラーはAssetの読み込み失敗として扱う
        struct ParseError { std::string m_message; };
        auto callbacks = ryml::get_callbacks();
        callbacks.set_error_parse([](ryml::csubstr message, const ryml::ErrorDataParse&, void*)
        {
            throw ParseError{ std::string(View(message)) };
        });
        ryml::EventHandlerTree handler(callbacks);
        ryml::Parser parser(&handler);
        try
        {
            return ryml::parse_in_arena(&parser, ryml::to_csubstr(source));
        }
        catch (const ParseError& failure)
        {
            error = failure.m_message;
            return std::nullopt;
        }
    }

    bool YamlWriter::Write(const ryml::Tree& tree, const std::filesystem::path& path, bool replaceExisting)
    {
        const auto fail = [&](std::string_view reason)
        {
            Logger::Error("Could not save YAML '" + PathToUtf8(path) + "': " + std::string(reason));
            return false;
        };

        // 書き込み途中やclose時の失敗で既存ファイルを壊さないよう同じフォルダで一時保存する
        std::error_code error;
        std::filesystem::create_directories(path.parent_path(), error);
        if (error)
            return fail(error.message());

        static std::atomic_uint sequence{ 0 };
        auto temporary = path;
        temporary += L"." + std::to_wstring(GetCurrentProcessId()) + L"." +
            std::to_wstring(sequence.fetch_add(1, std::memory_order_relaxed)) + L".tmp";
        const auto yaml = ryml::emitrs_yaml<std::string>(tree);
        std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
        if (!file)
            return fail("could not open the temporary file");
        file.write(yaml.data(), static_cast<std::streamsize>(yaml.size()));
        file.close();

        // 自動命名した新規Assetは、候補が使用済みになった場合も置き換えない
        if (file && MoveFileExW(temporary.c_str(), path.c_str(), replaceExisting ? MOVEFILE_REPLACE_EXISTING : 0))
            return true;

        std::filesystem::remove(temporary, error);
        return fail("could not write or replace the file");
    }

    std::optional<SerializedValue> ReadYamlValue(ryml::ConstNodeRef node)
    {
        if (!node.readable())
            return std::nullopt;

        // 配列とAssetRefはYAMLの構造で区別する
        if (node.is_seq())
        {
            SerializedValue::Array items;
            items.reserve(node.num_children());
            for (const auto child : node.children())
            {
                auto item = ReadYamlValue(child);
                if (!item)
                    return std::nullopt;
                items.push_back(std::move(*item));
            }
            return SerializedValue{ std::move(items) };
        }
        if (node.is_map())
        {
            const auto guid = node.find_child("guid");
            AssetRef ref;

            if (node.num_children() != 2 ||
                !guid.readable() ||
                !guid.has_val() ||
                !(ref.m_guid = AssetGuid::Parse(View(guid.val()))) ||   // GUID文字列をAssetGuidへ変換する
                !ReadNumber(node.find_child("localId"), ref.m_localId))
                return std::nullopt;
            return SerializedValue{ ref };
        }

        if (!node.has_val())
            return std::nullopt;
        if (node.val_is_null())
            return SerializedValue{ AssetRef{} };

        const auto text = View(node.val());
        if (node.is_val_quoted())
            return SerializedValue{ std::string(text) };
        if (text == "true" || text == "false")
            return SerializedValue{ text == "true" };

        int64_t signedValue = 0;
        uint64_t unsignedValue = 0;
        double floatingValue = 0;
        if (ReadNumber(text, signedValue))
            return SerializedValue{ signedValue };
        if (ReadNumber(text, unsignedValue))
            return SerializedValue{ unsignedValue };
        if (ReadNumber(text, floatingValue))
            return SerializedValue{ floatingValue };
        if (text == ".inf" || text == "-.inf")
            return SerializedValue{ text.front() == '-' ? -std::numeric_limits<double>::infinity() :
                std::numeric_limits<double>::infinity() };
        if (text == ".nan")
            return SerializedValue{ std::numeric_limits<double>::quiet_NaN() };
        return SerializedValue{ std::string(text) };
    }

    void WriteYamlValue(ryml::NodeRef node, const SerializedValue& value)
    {
        // 配列は要素を再帰的に展開し、文字列は数値や真偽値と区別する
        std::visit([node](const auto& item) mutable
        {
            using Value = std::decay_t<decltype(item)>;
            if constexpr (std::is_same_v<Value, SerializedValue::Array>)
            {
                node |= ryml::SEQ | ryml::FLOW_SL;
                for (const auto& element : item)
                    WriteYamlValue(node.append_child(), element);
            }
            else if constexpr (std::is_same_v<Value, std::string>)
                node.save(ryml::csubstr(item.data(), item.size()), ryml::VAL_DQUO);
            else if constexpr (std::is_same_v<Value, bool>)
                node.set_val(item ? "true" : "false");
            else if constexpr (std::is_same_v<Value, AssetRef>)
            {
                if (!item.m_guid)
                {
                    node.set_val("null");
                    return;
                }

                node |= ryml::MAP | ryml::FLOW_SL;
                node["guid"].save(ryml::to_csubstr(item.m_guid.ToString()));
                node["localId"].save(item.m_localId);
            }
            else if constexpr (std::is_same_v<Value, double>)
            {
                // 指数表記で精度を保ち、整数に見える値も浮動小数として保存する
                if (std::isnan(item))
                    node.set_val(".nan");
                else if (std::isinf(item))
                    node.set_val(std::signbit(item) ? "-.inf" : ".inf");
                else
                {
                    char buffer[32];
                    const auto result = std::to_chars(buffer, buffer + sizeof(buffer), item,
                        std::chars_format::scientific);
                    node.save(ryml::csubstr(buffer, static_cast<size_t>(result.ptr - buffer)));
                }
            }
            else
                node.save(item);
        }, value.m_value);
    }
}
