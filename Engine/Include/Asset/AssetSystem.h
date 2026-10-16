/*=============================================================================

 File   : AssetSystem.h
 Desc   : Asset 名解決、ロード、CPU 所有、Graphics への登録を調停する。

------------------------------------------------------------------------------

 Date   : 2026/10/15
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _ASSET_SYSTEM_H_
#define _ASSET_SYSTEM_H_

#include <memory>
#include <string_view>
#include <tuple>

#include "Asset/AssetDB.h"
#include "Asset/AssetLoader.h"
#include "Asset/AssetStorage.h"

namespace Hestia
{
    class GraphicsSystem;

    /// @brief Mesh / Texture Asset のロードと寿命を管理する。
    class AssetSystem
    {
    public:
        /// @brief 未初期化の AssetSystem を生成する。
        AssetSystem() = default;

        /// @brief 初期化済みなら所有 Asset と接続を解放する。
        ~AssetSystem();

        AssetSystem(const AssetSystem&) = delete;
        AssetSystem& operator=(const AssetSystem&) = delete;

        /// @brief GraphicsSystem を借用して Asset 管理を開始する。
        /// @param graphics 本 System より長く生存する非所有 GraphicsSystem。未初期化でも受理する。
        /// @return 初期化成功: true / null Graphics または別の稼働中 instance: false。
        /// @note Graphics が未初期化の場合も CPU Cache を利用できる。GPU Upload は失敗する。
        bool Initialize(GraphicsSystem* graphics);

        /// @brief Asset を破棄して singleton 接続を解除する。
        void Finalize();

        /// @brief DB に登録された Asset をロードし、名前 Cache を再利用する。
        /// @param name ASCII 英字の大小を区別しない Asset 名。
        /// @return ロード済み Handle。未登録・型不一致・ロード失敗時は無効 Handle。
        template<class T>
        static AssetHandle<T> Load(std::string_view name);

        /// @brief Handle に対応する Asset を借用する。
        /// @param handle 取得する Asset の Handle。
        /// @return Asset への非所有ポインタ。初期化前後や不一致時は nullptr。
        /// @note 返されたポインタは Finalize まで有効。
        template<class T>
        static T* Get(AssetHandle<T> handle);

        /// @brief CPU Asset の所有権を受け取り、名前 Cache に登録する。
        /// @param name ASCII 英字の大小を区別しない一意な Asset 名。
        /// @param asset 登録する Asset。呼び出し時点で所有権が移る。
        /// @return 登録成功時の Handle。失敗時は無効 Handle。
        /// @note Mesh は無効なデータを拒否する。GPU Upload 失敗後も CPU Asset と Handle を保持する。
        /// @note Texture の手動登録は拒否する。受け取った Asset の所有権は失敗時も消費する。
        template<class T>
        static AssetHandle<T> Register(std::string_view name, T* asset);

    private:
        template<class T>
        AssetHandle<T> LoadImpl(std::string_view name);

        template<class T>
        AssetHandle<T> RegisterImpl(
            std::string_view name, std::unique_ptr<T> asset, bool databaseOrigin);

        template<class T>
        static AssetHandle<T> RegisterFromLoader(
            std::string_view name, std::unique_ptr<T> asset);

        template<class T>
        AssetLoader<T>& GetLoader();

        template<class T>
        AssetStorage<T>& GetStorage();

        void OnRegistered(AssetHandle<Mesh> handle, Mesh& mesh);

        template<class T>
        friend class AssetLoader;

        AssetDB m_assetDB;
        std::tuple<AssetLoader<Mesh>, AssetLoader<Texture>> m_loaders;
        std::tuple<AssetStorage<Mesh>, AssetStorage<Texture>> m_storages;
        GraphicsSystem* m_graphics = nullptr;

        static AssetSystem* s_instance;
    };
}

#endif // _ASSET_SYSTEM_H_
