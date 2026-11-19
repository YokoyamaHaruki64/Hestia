/*=============================================================================

 File   : MeshRenderer.h
 Desc   : EntityHandle にひも付く Game 向け MeshRenderer Facade を定義する。

------------------------------------------------------------------------------

 Date   : 2026/11/16
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _GAME_MESH_RENDERER_H_
#define _GAME_MESH_RENDERER_H_

#include "Engine/Include/API/ECSProcess/MeshRendererAPI.h"

namespace HestiaGame
{
    class World;

    /// @brief Entity の MeshRenderer を操作する値型 Facade。
    /// @note API を借用するため、取得した GameRuntime の寿命内で使用する。
    class MeshRenderer
    {
    public:
        using API = Hestia::MeshRendererAPI;

        /// @brief 未接続の Facade を作る。
        MeshRenderer() = default;

        /// @brief 生存中かつ MeshRenderer を持つ Entity を参照していれば true。
        bool IsValid() const noexcept { return m_api != nullptr && m_api->IsValid(m_entity); }

        /// @brief MeshRenderer 自身の有効設定を返す。無効な参照は false。
        bool IsActive() const noexcept { return m_api != nullptr && m_api->IsActive(m_entity); }

        /// @brief MeshRenderer 自身の有効設定を更新する。成功時は true。
        bool SetActive(bool active) noexcept { return m_api != nullptr && m_api->SetActive(m_entity, active); }

        /// @brief 設定済みの Mesh を返す。未設定または無効な参照は無効 Handle。
        Hestia::AssetHandle<Hestia::Mesh> GetMesh() const noexcept
        {
            return m_api != nullptr ? m_api->GetMesh(m_entity) : Hestia::AssetHandle<Hestia::Mesh>{};
        }

        /// @brief Mesh を設定する。空の Handle で解除する。成功時は true。
        bool SetMesh(Hestia::AssetHandle<Hestia::Mesh> mesh)
        {
            return m_api != nullptr && m_api->SetMesh(m_entity, mesh);
        }

        /// @brief 空きを含む MaterialSlot 数を返す。無効な参照は 0。
        std::size_t GetMaterialCount() const noexcept
        {
            return m_api != nullptr ? m_api->GetMaterialCount(m_entity) : 0;
        }

        /// @brief MaterialSlot の値を返す。未設定・範囲外・無効な参照は無効 Handle。
        Hestia::AssetHandle<Hestia::Material> GetMaterial(std::size_t index) const noexcept
        {
            return m_api != nullptr ? m_api->GetMaterial(m_entity, index) : Hestia::AssetHandle<Hestia::Material>{};
        }

        /// @brief MaterialSlot を設定する。必要なら Slot を増やし、空の Handle で既存 Slot を解除する。
        /// @return 成功時は true。解除する Slot が範囲外の場合は false。
        bool SetMaterial(std::size_t index, Hestia::AssetHandle<Hestia::Material> material)
        {
            return m_api != nullptr && m_api->SetMaterial(m_entity, index, material);
        }

        /// @brief インスタンス色を返す。無効な参照は既定色の白。
        Vector4 GetInstanceColor() const noexcept
        {
            return m_api != nullptr ? m_api->GetInstanceColor(m_entity) : Vector4{ 1.0f, 1.0f, 1.0f, 1.0f };
        }

        /// @brief インスタンス色を更新する。成功時は true。
        bool SetInstanceColor(const Vector4& color) noexcept
        {
            return m_api != nullptr && m_api->SetInstanceColor(m_entity, color);
        }

    private:
        friend class World;

        MeshRenderer(Hestia::EntityHandle entity, const API* api) noexcept : m_entity(entity), m_api(api) {}

        Hestia::EntityHandle m_entity;
        const API* m_api = nullptr;
    };
}

#endif // _GAME_MESH_RENDERER_H_
