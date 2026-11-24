/*=============================================================================

 File   : MeshRendererAPI.cpp
 Desc   : EntityHandle から MeshRenderer を解決し、描画設定を読み書きする。

------------------------------------------------------------------------------

 Date   : 2026/11/16
 Author : Yokoyama Haruki

=============================================================================*/

#include "pch.h"
#include "API/ECSProcess/MeshRendererAPI.h"

#include "Asset/AssetSystem.h"
#include "ECS/ECSSystem.h"
#include "Graphics/MeshRenderer.h"

namespace Hestia
{
    namespace
    {
        MeshRenderer* ResolveMeshRenderer(EntityHandle entity) noexcept
        {
            // ECS の実体は破棄予約後も残るため、公開 API では予約済みの対象を除く
            const auto* owner = ECSSystem::GetEntity(entity);
            if (owner == nullptr || owner->IsDestroy())
                return nullptr;

            return ECSSystem::GetComponent<MeshRenderer>(entity);
        }
    }

    AssetRef MeshRenderer::GetMeshReference(EntityHandle entity)
    {
        const auto* renderer = ResolveMeshRenderer(entity);
        return renderer != nullptr && renderer->m_mesh != nullptr
            ? AssetSystem::GetReference(renderer->m_mesh->m_handle) : AssetRef{};
    }

    bool MeshRenderer::SetMeshReference(EntityHandle entity, const AssetRef& ref)
    {
        auto* renderer = ResolveMeshRenderer(entity);
        if (renderer == nullptr)
            return false;

        // 参照が未設定なら解除し、解決失敗なら現在のMeshを保つ
        const auto* mesh = ref.m_guid ? AssetSystem::Get(AssetSystem::Load<Mesh>(ref)) : nullptr;
        if (ref.m_guid && mesh == nullptr)
            return false;

        renderer->m_mesh = mesh;
        return true;
    }

    std::vector<AssetRef> MeshRenderer::GetMaterialReferences(EntityHandle entity)
    {
        std::vector<AssetRef> refs;
        const auto* renderer = ResolveMeshRenderer(entity);
        if (!renderer)
            return refs;

        refs.reserve(renderer->m_materials.size());
        for (const auto* material : renderer->m_materials)
            refs.push_back(material ? AssetSystem::GetReference(material->GetHandle()) : AssetRef{});
        return refs;
    }

    bool MeshRenderer::SetMaterialReferences(EntityHandle entity, const std::vector<AssetRef>& refs)
    {
        auto* renderer = ResolveMeshRenderer(entity);
        if (!renderer)
            return false;

        // 未設定Slotを保持し、全参照が解決してから配列を反映する
        std::vector<const Material*> materials;
        materials.reserve(refs.size());
        for (const auto& ref : refs)
        {
            const auto* material = ref.m_guid ? AssetSystem::Get(AssetSystem::Load<Material>(ref)) : nullptr;
            if (ref.m_guid && !material)
                return false;
            materials.push_back(material);
        }
        renderer->m_materials = std::move(materials);
        return true;
    }

    MeshRendererAPI MeshRendererAPIImpl::CreateAPI() noexcept
    {
        return {
            .IsValid = [](EntityHandle entity) noexcept { return ResolveMeshRenderer(entity) != nullptr; },
            .IsActive = [](EntityHandle entity) noexcept
            {
                const auto* renderer = ResolveMeshRenderer(entity);
                return renderer != nullptr && renderer->m_active;
            },
            .SetActive = [](EntityHandle entity, bool active) noexcept
            {
                auto* renderer = ResolveMeshRenderer(entity);
                if (renderer == nullptr)
                    return false;

                renderer->m_active = active;
                return true;
            },
            .GetMesh = [](EntityHandle entity) noexcept
            {
                const auto* renderer = ResolveMeshRenderer(entity);
                if (renderer == nullptr || renderer->m_mesh == nullptr)
                    return AssetHandle<Mesh>{};

                return renderer->m_mesh->m_handle;
            },
            .SetMesh = [](EntityHandle entity, AssetHandle<Mesh> mesh)
            {
                auto* renderer = ResolveMeshRenderer(entity);
                if (renderer == nullptr)
                    return false;

                // 空の Handle は解除。解決できない有効形式の Handle では現在の Mesh を保つ
                const auto* asset = mesh.IsValid() ? AssetSystem::Get(mesh) : nullptr;
                if (mesh.IsValid() && asset == nullptr)
                    return false;

                renderer->m_mesh = asset;
                return true;
            },
            .GetMaterialCount = [](EntityHandle entity) noexcept -> std::size_t
            {
                const auto* renderer = ResolveMeshRenderer(entity);
                return renderer != nullptr ? renderer->m_materials.size() : 0;
            },
            .GetMaterial = [](EntityHandle entity, std::size_t index) noexcept
            {
                const auto* renderer = ResolveMeshRenderer(entity);
                if (renderer == nullptr || index >= renderer->m_materials.size())
                    return AssetHandle<Material>{};

                const auto* material = renderer->m_materials[index];
                return material != nullptr ? material->GetHandle() : AssetHandle<Material>{};
            },
            .SetMaterial = [](EntityHandle entity, std::size_t index, AssetHandle<Material> material)
            {
                auto* renderer = ResolveMeshRenderer(entity);
                if (renderer == nullptr)
                    return false;

                // Asset の解決に失敗した場合は Slot の内容と数を保つ
                const auto* asset = material.IsValid() ? AssetSystem::Get(material) : nullptr;
                if (material.IsValid() && asset == nullptr)
                    return false;

                auto& materials = renderer->m_materials;
                if (index >= materials.size())
                {
                    // 既存 Slot の解除では配列を伸ばさず、設定時は間を未設定のまま残す
                    if (asset == nullptr || index >= materials.max_size())
                        return false;

                    materials.resize(index + 1, nullptr);
                }

                materials[index] = asset;
                return true;
            },
            .GetInstanceColor = [](EntityHandle entity) noexcept
            {
                const auto* renderer = ResolveMeshRenderer(entity);
                return renderer != nullptr ? renderer->m_instanceColor : Vector4{ 1.0f, 1.0f, 1.0f, 1.0f };
            },
            .SetInstanceColor = [](EntityHandle entity, const Vector4& color) noexcept
            {
                auto* renderer = ResolveMeshRenderer(entity);
                if (renderer == nullptr)
                    return false;

                renderer->m_instanceColor = color;
                return true;
            },
        };
    }
}
