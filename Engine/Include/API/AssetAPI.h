/*=============================================================================

 File   : AssetAPI.h
 Desc   : Game.dll へ Mesh / Texture Asset 操作を公開する境界 API。

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

        /// @brief Asset 型ごとの公開操作。
        template<class T>
        struct Operations
        {
            /// @brief ASCII 英字の大小を区別せず名前でロードする。失敗時は無効 Handle。
            AssetHandle<T> (*Load)(std::string_view name) = nullptr;
            /// @brief Handle に対応する Asset を借用する。不一致時は nullptr。
            T* (*Get)(AssetHandle<T> handle) = nullptr;
            /// @brief 名前 Cache に登録する。失敗時も Asset の所有権を受け取る。
            AssetHandle<T> (*Register)(std::string_view name, T* asset) = nullptr;
        };

        /// @brief 指定された Asset 型の関数テーブルを返す。
        template<class T>
        const Operations<T>& GetOperations() const noexcept
        {
            static_assert(std::is_same_v<T, Mesh> || std::is_same_v<T, Texture>);
            if constexpr (std::is_same_v<T, Mesh>)
                return m_mesh;
            else
                return m_texture;
        }

    private:
        Operations<Mesh> m_mesh;
        Operations<Texture> m_texture;
    };
}

#endif // _ASSET_API_H_
