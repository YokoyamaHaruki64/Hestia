#pragma once

#include <filesystem>
#include <string>

#include "System/Serialization/YAMLTypes.h"

// YAMLライブラリの詳細を隠し、YAMLValueとYAMLファイルの入出力だけを担当する。
class YAMLDocumentIO final
{
public:
    static bool Save(const std::filesystem::path& path, const YAMLValue& document);
    static bool Load(const std::filesystem::path& path, YAMLValue& document);

private:
    // rapidyamlのNode型を公開せず、中間表現との相互変換だけを担当する。
    template <typename Node>
    static void WriteValue(Node node, const YAMLValue& value);

    template <typename Node>
    static bool ReadValue(const Node& node, YAMLValue& value);

    // BOM対応にばらつきがある外部Parserへ渡す前にUTF-8 BOMを除去する。
    static void RemoveUtf8Bom(std::string& source);

    // rapidyamlのLF出力を、プロジェクト規約のCRLFへ変換する。
    static void NormalizeLineEndings(std::string& source);
};
