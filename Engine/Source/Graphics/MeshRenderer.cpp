/*=============================================================================

 File   : MeshRenderer.cpp
 Desc   : ECSのMeshRendererを描画キューへ提出する

------------------------------------------------------------------------------

 Date   : 2026/11/09
 Author : Yokoyama Haruki

=============================================================================*/

#include "pch.h"
#include "Graphics/MeshRenderer.h"

#include "Graphics/GraphicsSystem.h"
#include "Transform/TransformData.h"

namespace Hestia
{
    void MeshRendererProcess::Run(std::size_t count, const Entity* entities,
        const TransformCache* caches, const MeshRenderer* renderers)
    {
        for (std::size_t index = 0; index < count; ++index)
        {
            // 非アクティブ・破棄予約済み、描画資源が未設定の行を除く
            const MeshRenderer& renderer = renderers[index];
            if (!entities[index].IsActive() || !renderer.m_active || !renderer.m_mesh || renderer.m_materials.empty())
                continue;

            // Chunkの移動後も記録できるよう、行列とSlot配列は描画キューへコピー
            GraphicsSystem::Submit({
                .worldMatrix = caches[index].m_worldMatrix,
                .mesh = renderer.m_mesh,
                .materials = renderer.m_materials,
            });
        }
    }
}
