/*=============================================================================

 File   : AssetMetadata.h
 Desc   : Assetのメタデータ入出力と走査を提供する。

------------------------------------------------------------------------------

 Date   : 2026/11/25
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _ASSET_METADATA_H_
#define _ASSET_METADATA_H_

#include "AssetIdentity.h"

#include <filesystem>
#include <span>
#include <unordered_map>
#include <vector>

namespace Hestia
{
    /// @brief 元Assetファイルの索引情報と、モデルのSubAsset割り当て
    struct AssetInfo
    {
        AssetGuid m_guid;
        AssetType m_type = AssetType::Mesh;
        std::string m_path;
        uint32_t m_nextLocalId = 1;
        std::unordered_map<std::string, uint32_t> m_subAssets;
        std::unordered_map<std::string, uint32_t> m_materials;
    };

    namespace AssetMetadata
    {
        /// @brief 元Assetの既存メタデータを検証して読み込む。
        bool Read(const std::filesystem::path& source, AssetInfo& info, std::string& error);
        /// @brief メタデータを取得し、欠けている場合は作成する。
        bool Ensure(const std::filesystem::path& source, AssetInfo& info, std::string& error);
        /// @brief モデルのSubAsset割り当てを、元のGUIDを維持して保存する。
        bool SaveModel(const std::filesystem::path& source, const AssetInfo& info, std::string& error);
        /// @brief 既存のメタデータを検証してから不足分を作り、索引の材料を返す。
        /// @param extensions 空なら全対応拡張子を対象とする。
        bool Scan(const std::filesystem::path& root, std::span<const std::string_view> extensions,
            std::vector<AssetInfo>& entries, std::vector<std::string>& warnings, std::string& error);
    }
}

#endif // _ASSET_METADATA_H_
