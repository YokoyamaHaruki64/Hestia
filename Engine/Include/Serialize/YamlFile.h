/*=============================================================================

 File   : YamlFile.h
 Desc   : Assetで共有するYAMLのファイル入出力と値変換

------------------------------------------------------------------------------

 Date   : 2026/11/26
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _YAML_FILE_H_
#define _YAML_FILE_H_

#include <filesystem>
#include <optional>
#include <string>

#include "RapidYaml/rapidyaml.hpp"
#include "Serialize/SerializedEntity.h"

namespace Hestia
{
    namespace YamlReader
    {
        /// @brief UTF-8のファイルを読み込み、文書を所有するTreeを返す
        /// @param path 入力ファイル
        /// @param error 失敗理由の出力先
        /// @return 成功時はTree、読み込み・構文エラー時はstd::nullopt
        std::optional<ryml::Tree> Read(const std::filesystem::path& path, std::string& error);
    }

    namespace YamlWriter
    {
        /// @brief 完成したTreeを一時ファイルに書き出してから保存先へ反映する
        /// @param tree 出力する文書
        /// @param path 保存先
        /// @param replaceExisting 既存ファイルの置き換えを許可するか
        /// @return 出力成功: true / 失敗: false
        bool Write(const ryml::Tree& tree, const std::filesystem::path& path, bool replaceExisting);
    }

    /// @brief YAMLのスカラー、配列またはAssetRefを保存値へ変換する
    std::optional<SerializedValue> ReadYamlValue(ryml::ConstNodeRef node);

    /// @brief 保存値を指定のノードへ書き込む
    void WriteYamlValue(ryml::NodeRef node, const SerializedValue& value);
}

#endif // _YAML_FILE_H_
