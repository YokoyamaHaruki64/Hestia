/*=============================================================================

 File   : MeshRenderer.h
 Desc   : Mesh描画用のComponentと描画収集Processを定義する

------------------------------------------------------------------------------

 Date   : 2026/11/09
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _MESH_RENDERER_H_
#define _MESH_RENDERER_H_

#include <cstddef>
#include <vector>
#include "Common/Include/Math/Vector4.h"
#include "Serialize/SerializeFields.h"

namespace Hestia
{
    class Entity;
    class Material;
    struct Mesh;
    struct TransformCache;

    /// @brief Renderer種別で共有する設定の格納先
    struct RendererSettings
    {
    };

    /// @brief EntityのMesh描画に必要な設定とAssetへの非所有参照を保持する
    /// @note MeshとMaterialはAssetSystemの寿命内で使用する
    struct MeshRenderer
    {
        bool m_active = true;
        RendererSettings m_settings;
        const Mesh* m_mesh = nullptr;
        std::vector<const Material*> m_materials; // SubMeshのMaterialSlotに対応
        Vector4 m_instanceColor{ 1.0f, 1.0f, 1.0f, 1.0f };

        SERIALIZE_FIELDS(
            ("mesh", &MeshRenderer::GetMeshReference, &MeshRenderer::SetMeshReference)
            ("materials", &MeshRenderer::GetMaterialReferences, &MeshRenderer::SetMaterialReferences)
        )

    private:
        static AssetRef GetMeshReference(EntityHandle entity);
        static bool SetMeshReference(EntityHandle entity, const AssetRef& ref);
        static std::vector<AssetRef> GetMaterialReferences(EntityHandle entity);
        static bool SetMaterialReferences(EntityHandle entity, const std::vector<AssetRef>& refs);
    };

    class MeshRendererProcess
    {
    public:
        /// @brief 計算済みのWorld行列を使い、Meshの描画を提出する
        /// @param count Chunk内の行数
        /// @param entities Entity列
        /// @param caches TransformCache列
        /// @param renderers MeshRenderer列
        /// @note PreRenderでTransformProcessの後に実行する
        static void Run(std::size_t count, const Entity* entities,
            const TransformCache* caches, const MeshRenderer* renderers);
    };
}

#endif // _MESH_RENDERER_H_
