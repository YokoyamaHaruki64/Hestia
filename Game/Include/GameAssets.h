/*=============================================================================

 File   : GameAssets.h
 Desc   : GameRuntime から Engine の Asset 操作を利用する Facade を定義する。

------------------------------------------------------------------------------

 Date   : 2026/11/17
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _HESTIA_GAME_ASSETS_H_
#define _HESTIA_GAME_ASSETS_H_

#include "Engine/Include/API/AssetAPI.h"

namespace HestiaGame
{
    namespace Detail { class GameRuntime; }

    /// @brief GameRuntimeに接続したAsset操作のFacade
    class Assets
    {
    public:
        /// @brief Assetルート相対パスまたは組み込み名からロードする
        template<class T>
        static Hestia::AssetHandle<T> Load(std::string_view name)
        {
            const auto load = s_api != nullptr ? s_api->GetOperations<T>().Load : nullptr;
            return load != nullptr ? load(name) : Hestia::AssetHandle<T>{};
        }

        /// @brief 永続参照からロードする。失敗・未接続時は無効Handle
        template<class T>
        static Hestia::AssetHandle<T> Load(const Hestia::AssetRef& ref)
        {
            const auto load = s_api != nullptr ? s_api->GetOperations<T>().LoadByReference : nullptr;
            return load != nullptr ? load(ref) : Hestia::AssetHandle<T>{};
        }

        /// @brief Asset相対パスまたは登録名からMaterialを取得する。失敗時は無効Handle。
        static Hestia::AssetHandle<Hestia::Material> LoadMaterial(std::string_view path)
        {
            return s_api != nullptr ? s_api->LoadMaterial(path) : Hestia::AssetHandle<Hestia::Material>{};
        }

        /// @brief Handle に対応する Asset を借用する。失敗・未接続時は nullptr。
        template<class T>
        static T* Get(Hestia::AssetHandle<T> handle)
        {
            const auto get = s_api != nullptr ? s_api->GetOperations<T>().Get : nullptr;
            return get != nullptr ? get(handle) : nullptr;
        }

        /// @brief Asset を登録する。失敗・未接続時も渡した Asset の所有権は移る。
        template<class T>
        static Hestia::AssetHandle<T> Register(std::string_view name, T* asset)
        {
            const auto registerAsset = s_api != nullptr ? s_api->GetOperations<T>().Register : nullptr;
            if (registerAsset != nullptr)
                return registerAsset(name, asset);

            // 未接続でも呼び出し時に受け取った所有権を消費する
            delete asset;
            return {};
        }

    private:
        friend class Detail::GameRuntime;

        static void Bind(const Hestia::AssetAPI* api) noexcept { s_api = api; }
        static void Unbind() noexcept { s_api = nullptr; }

        inline static const Hestia::AssetAPI* s_api = nullptr;
    };
}

#endif // _HESTIA_GAME_ASSETS_H_
