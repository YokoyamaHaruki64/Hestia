/*=============================================================================

 File   : AssetLoader.h
 Desc   : Mesh / Texture の初期ロード経路を宣言する。

------------------------------------------------------------------------------

 Date   : 2026/10/15
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _ASSET_LOADER_H_
#define _ASSET_LOADER_H_

#include <string_view>

#include "Asset/AssetType.h"

namespace Hestia
{
    class GraphicsSystem;

    template<class T>
    class AssetLoader;

    /// @brief Mesh Asset のロード境界。
    template<>
    class AssetLoader<Mesh>
    {
    public:
        /// @brief 指定情報から Mesh をロードする。
        /// @param name 登録する Asset 名。
        /// @param path Mesh ファイルへの Path。
        /// @return 登録済み Handle。初期実装では未対応のため無効 Handle。
        AssetHandle<Mesh> Load(std::string_view name, std::string_view path);
    };

    /// @brief Texture Asset のデコードと GPU Upload 境界。
    template<>
    class AssetLoader<Texture>
    {
    public:
        bool Initialize(GraphicsSystem* graphics);

        /// @brief 画像をデコードし CPU Texture と GPU Texture を登録する。
        /// @param name 登録する Asset 名。
        /// @param path 画像ファイルへの Path。
        /// @return CPU 登録済み Handle。読込または登録失敗時は無効 Handle。
        /// @note GPU Upload 失敗時も CPU Asset を保持し、有効 Handle を返す。
        AssetHandle<Texture> Load(std::string_view name, std::string_view path);

    private:
        GraphicsSystem* m_graphics = nullptr;
    };
}

#endif // _ASSET_LOADER_H_
