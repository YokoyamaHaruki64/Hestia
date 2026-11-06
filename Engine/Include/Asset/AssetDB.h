/*=============================================================================

 File   : AssetDB.h
 Desc   : Asset 名から Mesh / Texture の種別と Path を解決する。

------------------------------------------------------------------------------

 Date   : 2026/10/15
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _ASSET_DB_H_
#define _ASSET_DB_H_

#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

#include "Asset/AssetType.h"

namespace Hestia
{
    /// @brief AssetDB に登録する Asset の種別とファイル Path。
    struct AssetInfo
    {
        AssetType m_type = AssetType::Mesh;
        std::string m_path;
    };

    /// @brief 起動時に用意する Asset 名、種別、Path の索引。
    class AssetDB
    {
    public:
        using EntryMap = std::unordered_map<std::string, AssetInfo>;

        /// @brief AssetSystem が構築した登録情報を正規化して格納する。
        /// @param entries Asset 名を Key とする登録情報。呼び出し時点で値を受け取る。
        /// @return 初期化成功: true / 空名・Path・未対応型や正規化後の衝突がある場合: false。
        bool Initialize(EntryMap entries);

        /// @brief 名前に対応する登録情報を検索する。
        /// @param name ASCII 英字の大小を区別しない Asset 名。
        /// @return 登録情報への非所有ポインタ。未登録なら nullptr。
        const AssetInfo* Find(std::string_view name) const;

        /// @brief 登録名との衝突を確認する。
        /// @param name ASCII 英字の大小を区別しない Asset 名。
        /// @return 登録済みなら true。
        bool Contains(std::string_view name) const;

    private:
        static std::string NormalizeName(std::string_view name);

        std::unordered_map<std::string, AssetInfo> m_entries;
    };
}

#endif // _ASSET_DB_H_
