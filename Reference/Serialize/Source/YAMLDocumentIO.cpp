#include "pch.h"

#define RYML_DEFAULT_CALLBACK_USES_EXCEPTIONS
#define RYML_SINGLE_HDR_DEFINE_NOW
#include "External/Include/rapidyaml/rapidyaml.hpp"

#include "System/Serialization/YAMLDocumentIO.h"

#include "System/Diagnostics/Logger.h"

void YAMLDocumentIO::RemoveUtf8Bom(std::string& source)
{
    static constexpr std::string_view Utf8Bom{ "\xEF\xBB\xBF", 3 };
    if (source.starts_with(Utf8Bom))
    {
        // BOMを含めたまま渡すと、rapidyamlがネストしたSequenceの字下げを誤判定する。
        source.erase(0, Utf8Bom.size());
    }
}

void YAMLDocumentIO::NormalizeLineEndings(std::string& source)
{
    std::string normalized;
    normalized.reserve(source.size());

    for (size_t index = 0; index < source.size(); ++index)
    {
        if (source[index] == '\r')
        {
            if (index + 1 < source.size() && source[index + 1] == '\n')
            {
                ++index;
            }
            normalized += "\r\n";
        }
        else if (source[index] == '\n')
        {
            normalized += "\r\n";
        }
        else
        {
            normalized += source[index];
        }
    }

    source = std::move(normalized);
}

template <typename Node>
void YAMLDocumentIO::WriteValue(Node node, const YAMLValue& value)
{
    // YAMLライブラリ非依存の中間表現を、出力Treeへ再帰的に展開する。
    switch (value.type)
    {
    case YAMLValue::Type::Null:
        node.set_val(c4::csubstr{});
        break;
    case YAMLValue::Type::Scalar:
        if (value.isString)
        {
            node.save(value.scalar);
        }
        else
        {
            node.set_val(c4::to_csubstr(value.scalar));
        }
        break;
    case YAMLValue::Type::Sequence:
        node |= ryml::SEQ;
        if (value.collectionStyle == YAMLValue::CollectionStyle::Flow)
        {
            node |= ryml::FLOW_SL;
        }
        for (const YAMLValue& element : value.sequence)
        {
            WriteValue(node.append_child(), element);
        }
        break;
    case YAMLValue::Type::Map:
        node |= ryml::MAP;
        for (const auto& [key, childValue] : value.map)
        {
            WriteValue(node[c4::to_csubstr(key)], childValue);
        }
        break;
    }
}

template <typename Node>
bool YAMLDocumentIO::ReadValue(const Node& node, YAMLValue& value)
{
    if (node.is_map())
    {
        // Mapのキー順を保持し、読み込み後の未知Field報告にも利用する。
        value = YAMLValue::Map();
        value.map.reserve(node.num_children());
        for (const auto child : node.children())
        {
            YAMLValue childValue;
            if (!ReadValue(child, childValue))
            {
                return false;
            }
            value.map.emplace_back(
                std::string(child.key().str, child.key().len),
                std::move(childValue));
        }
        return true;
    }

    if (node.is_seq())
    {
        // フロー形式とブロック形式はrapidyaml上では同じSequenceとして扱う。
        value = YAMLValue::Sequence();
        value.sequence.reserve(node.num_children());
        for (const auto child : node.children())
        {
            YAMLValue childValue;
            if (!ReadValue(child, childValue))
            {
                return false;
            }
            value.sequence.emplace_back(std::move(childValue));
        }
        return true;
    }

    if (node.val_is_null())
    {
        value = YAMLValue::Null();
        return true;
    }
    if (!node.has_val())
    {
        return false;
    }

    value = YAMLValue::Scalar(std::string(node.val().str, node.val().len));
    return true;
}

bool YAMLDocumentIO::Save(const std::filesystem::path& path, const YAMLValue& document)
{
    try
    {
        // 呼び出し側ごとの形式差を持ち込まず、Document単位で一括保存する。
        ryml::Tree tree;
        WriteValue(tree.rootref(), document);

        const std::filesystem::path parent = path.parent_path();
        if (!parent.empty())
        {
            std::filesystem::create_directories(parent);
        }

        std::ostringstream serialized;
        serialized << tree;
        if (!serialized)
        {
            Logger::Error("YAMLのシリアライズに失敗しました: " + path.string());
            return false;
        }

        std::string source = serialized.str();
        NormalizeLineEndings(source);

        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        if (!stream)
        {
            Logger::Error("YAMLファイルを開けません: " + path.string());
            return false;
        }

        stream.write(source.data(), static_cast<std::streamsize>(source.size()));
        if (!stream)
        {
            Logger::Error("YAMLファイルの書き込みに失敗しました: " + path.string());
            return false;
        }
        return true;
    }
    catch (const std::exception& exception)
    {
        Logger::Error("YAML保存に失敗しました: " + std::string(exception.what()));
        return false;
    }
}

bool YAMLDocumentIO::Load(const std::filesystem::path& path, YAMLValue& document)
{
    try
    {
        std::ifstream stream(path, std::ios::binary);
        if (!stream)
        {
            Logger::Error("YAMLファイルを開けません: " + path.string());
            return false;
        }

        std::string source((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
        RemoveUtf8Bom(source);
        ryml::Tree tree = ryml::parse_in_arena(c4::to_csubstr(source));
        if (!ReadValue(tree.crootref(), document))
        {
            Logger::Error("YAMLの読み込みに失敗しました: " + path.string());
            return false;
        }
        return true;
    }
    catch (const ryml::ExceptionParse& exception)
    {
        std::string message;
        ryml::format_exc(&message, exception);
        Logger::Error("YAML読み込みに失敗しました: " + path.string() + "\n" + message);
        return false;
    }
    catch (const std::exception& exception)
    {
        Logger::Error("YAML読み込みに失敗しました: " + std::string(exception.what()));
        return false;
    }
}
