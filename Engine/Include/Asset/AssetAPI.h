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

#include "Asset/AssetHandle.h"
#include "Asset/AssetType.h"
#include "Common/Include/DllAPI.h"

namespace Hestia
{
    class AssetSystem;

    /// @brief AssetSystem への非所有参照を通して Asset 操作を Game に公開する。
    class HESTIA_ENGINE_API AssetAPI
    {
    public:
        /// @brief 初期化済み AssetSystem への非所有参照を設定する。
        /// @param system AssetAPI の利用期間を通じて生存する AssetSystem。
        void Initialize(AssetSystem* system);

        /// @brief AssetSystem への非所有参照を解除する。
        void Finalize();

        /// @brief Asset 名からロードする。
        /// @param name ASCII 英字の大小を区別しない Asset 名。
        /// @return ロード済み Handle。未接続または失敗時は無効 Handle。
        template<class T>
        AssetHandle<T> Load(std::string_view name);

        /// @brief Handle に対応する Asset を借用する。
        /// @param handle 取得する Asset の Handle。
        /// @return Asset への非所有ポインタ。未接続または不一致時は nullptr。
        template<class T>
        T* Get(AssetHandle<T> handle);

        /// @brief Asset の所有権を渡して名前 Cache に登録する。
        /// @param name ASCII 英字の大小を区別しない一意な Asset 名。
        /// @param asset 登録する Asset。呼び出し時点で所有権が移る。
        /// @return 登録成功時の Handle。未接続または失敗時は無効 Handle。
        template<class T>
        AssetHandle<T> Register(std::string_view name, T* asset);

    private:
        AssetSystem* m_system = nullptr;
    };

    extern template HESTIA_ENGINE_API AssetHandle<Mesh> AssetAPI::Load<Mesh>(std::string_view);
    extern template HESTIA_ENGINE_API AssetHandle<Texture> AssetAPI::Load<Texture>(std::string_view);
    extern template HESTIA_ENGINE_API Mesh* AssetAPI::Get<Mesh>(AssetHandle<Mesh>);
    extern template HESTIA_ENGINE_API Texture* AssetAPI::Get<Texture>(AssetHandle<Texture>);
    extern template HESTIA_ENGINE_API AssetHandle<Mesh> AssetAPI::Register<Mesh>(
        std::string_view, Mesh*);
    extern template HESTIA_ENGINE_API AssetHandle<Texture> AssetAPI::Register<Texture>(
        std::string_view, Texture*);
}

#endif // _ASSET_API_H_
