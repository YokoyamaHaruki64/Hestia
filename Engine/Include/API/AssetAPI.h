/*=============================================================================

 File   : AssetAPI.h
 Desc   : Game.dll へ Asset 操作を公開する境界 API。

------------------------------------------------------------------------------

 Date   : 2026/10/15
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _ASSET_API_H_
#define _ASSET_API_H_

#include <string_view>
#include <type_traits>

#include "Engine/Include/Asset/AssetHandle.h"
#include "Engine/Include/Asset/AssetType.h"

namespace Hestia
{
    class AssetSystem;
    class Material;

    /// @brief AssetSystem の操作を型ごとの関数テーブルとして Game に公開する。
    /// @note 未接続時の各ポインタは nullptr。
    class AssetAPI
    {
    public:
        /// @brief 初期化済み AssetSystem の操作を接続する。
        /// @param system AssetAPI の利用期間を通じて生存する AssetSystem。
        void Initialize(AssetSystem* system);

        /// @brief AssetSystem の操作との接続を解除する。
        void Finalize();

        /// @brief 名前で登録済みの Material の Handle を返す。未接続・未登録時は無効 Handle。
        AssetHandle<Material> LoadMaterial(std::string_view name) const
        {
            return m_loadMaterial != nullptr ? m_loadMaterial(name) : AssetHandle<Material>{};
        }

        /// @brief Asset 型ごとの公開操作。
        template<class T>
        struct Operations
        {
            /// @brief 相対パスまたは組み込み名でロードする。失敗時は無効Handle
            AssetHandle<T> (*Load)(std::string_view path) = nullptr;
            /// @brief Handle に対応する Asset を借用する。不一致時は nullptr。
            T* (*Get)(AssetHandle<T> handle) = nullptr;
            /// @brief 名前 Cache に登録する。失敗時も Asset の所有権を受け取る。
            AssetHandle<T> (*Register)(std::string_view name, T* asset) = nullptr;
            /// @brief 永続参照でロードする。失敗時は無効Handle
            AssetHandle<T> (*LoadByReference)(const AssetRef& ref) = nullptr;
        };

        /// @brief 指定された Asset 型の関数テーブルを返す。
        template<class T>
        const Operations<T>& GetOperations() const noexcept
        {
            static_assert(std::is_same_v<T, Mesh> || std::is_same_v<T, Texture> || std::is_same_v<T, Prefab>);
            if constexpr (std::is_same_v<T, Mesh>)
                return m_mesh;
            else if constexpr (std::is_same_v<T, Texture>)
                return m_texture;
            else
                return m_prefab;
        }

    private:
        Operations<Mesh> m_mesh;
        Operations<Texture> m_texture;
        Operations<Prefab> m_prefab;
        AssetHandle<Material> (*m_loadMaterial)(std::string_view name) = nullptr;
    };
}

#endif // _ASSET_API_H_
