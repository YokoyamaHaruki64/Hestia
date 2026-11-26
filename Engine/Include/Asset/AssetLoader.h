/*=============================================================================

 File   : AssetLoader.h
 Desc   : Asset 型ごとのファイルロード境界を宣言する。

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
    class Material;
    class MaterialShader;
    class AssetDB;
    struct AssetInfo;

    template<class T>
    class AssetLoader;

    /// @brief モデルをPrefabと個別Meshへ変換する
    template<>
    class AssetLoader<Prefab>
    {
    public:
        /// @brief 同じファイルに属するPrefabとMeshを一度に登録する
        AssetHandle<Prefab> Load(const AssetInfo& info, const AssetDB& database);

        /// @brief 保存済みPrefabを読み込み、主Assetとして登録する
        AssetHandle<Prefab> LoadYaml(const AssetInfo& info, const AssetDB& database);
    };

    /// @brief Texture Asset のデコードと GPU Upload 境界。
    template<>
    class AssetLoader<Texture>
    {
    public:
        bool Initialize(GraphicsSystem* graphics);

        /// @brief 画像をデコードし CPU Texture と GPU Texture を登録する。
        /// @param ref 登録するAssetの永続参照
        /// @param path 画像ファイルへの Path。
        /// @return CPU 登録済み Handle。読込または登録失敗時は無効 Handle。
        /// @note GPU Upload 失敗時も CPU Asset を保持し、有効 Handle を返す。
        AssetHandle<Texture> Load(const AssetRef& ref, std::string_view path);

    private:
        GraphicsSystem* m_graphics = nullptr;
    };

    /// @brief MaterialShader の解析、コンパイル、登録境界。
    template<>
    class AssetLoader<MaterialShader>
    {
    public:
        /// @brief AssetDB の情報で .matsh をロードする。
        /// @param ref 登録するAssetの永続参照
        /// @param path .matsh ファイルへの UTF-8 パス。
        /// @return CPU Asset の Handle。読込、解析、コンパイル、登録失敗時は無効。
        AssetHandle<MaterialShader> Load(const AssetRef& ref, std::string_view path);
    };

    /// @brief 単独の.materialを読み込み、検証済みのMaterialを登録する
    template<>
    class AssetLoader<Material>
    {
    public:
        /// @brief ShaderとTextureを解決してMaterialを構築する
        /// @param ref 登録するAssetの永続参照
        /// @param path .materialファイルへのUTF-8パス
        /// @return ロード済みHandle。構文・参照・値の検証に失敗した場合は無効Handle
        AssetHandle<Material> Load(const AssetRef& ref, std::string_view path);
    };
}

#endif // _ASSET_LOADER_H_
