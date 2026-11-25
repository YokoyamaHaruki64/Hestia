/*=============================================================================

 File   : AssetDB.h
 Desc   : .metaからAssetの永続ID、種別、Pathを解決する。

------------------------------------------------------------------------------

 Date   : 2026/10/15
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _ASSET_DB_H_
#define _ASSET_DB_H_

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>

#include "Common/Include/AssetMetadata.h"

namespace Hestia
{
    /// @brief .metaから作るGUIDとAssetルート相対パスの索引
    class AssetDB
    {
    public:
        /// @brief 元ファイルと.metaを走査し、欠けている.metaを作成する
        /// @return 成功: true / 読み込み・検証・作成失敗: false
        bool Initialize(const std::filesystem::path& assetsRoot = "Assets");

        /// @brief 永続IDから元ファイルの情報を取得する。未登録ならnullptr
        const AssetInfo* FindByGuid(const AssetGuid& guid) const;
        /// @brief Assetルート相対パスから元ファイルの情報を取得する。未登録ならnullptr
        const AssetInfo* FindByPath(std::string_view path) const;
        /// @brief Loaderへ渡す元ファイルのUTF-8パスを組み立てる
        std::string SourcePath(const AssetInfo& info) const;
        /// @brief Assetルート相対のパスを物理パスへ解決する。
        std::filesystem::path SourcePath(std::string_view relativePath) const;
        /// @brief 保存したファイルのメタデータを揃え、索引を1件追加する。
        bool RegisterFile(std::string_view relativePath);
        /// @brief 変換済みモデルの新しいID割り当てを、差分がある場合だけ保存する
        bool SaveModelMeta(const AssetInfo& previous, const AssetInfo& updated) const;

    private:
        std::filesystem::path m_root;
        std::unordered_map<AssetGuid, AssetInfo, AssetGuid::Hash> m_entries;
        std::unordered_map<std::string, AssetGuid> m_paths;

    };
}

#endif // _ASSET_DB_H_
