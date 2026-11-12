/*=============================================================================

 File   : ForwardRenderPass.h
 Desc   : Forward描画の展開、ソート、CommandList記録を管理する

------------------------------------------------------------------------------

 Date   : 2026/11/07
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _FORWARD_RENDER_PASS_H_
#define _FORWARD_RENDER_PASS_H_

#include "Graphics/IRenderPass.h"
#include <array>
#include <vector>

namespace Hestia
{
    /// @brief Opaque、Masked、TransparentのForward描画を実行する
    class ForwardRenderPass final : public IRenderPass
    {
    public:
        /// @brief コマンドが持つ借用参照を破棄し、次のフレームに容量を再利用する
        void ClearFrame();

        void Build(std::span<const RenderData> submissions, const DX12Resources& resources) override;
        void Sort() override;
        void Record(std::span<const RenderData> submissions, const RenderPassRecordContext& context) override;

    private:
        struct PhaseDraws
        {
            std::vector<DrawCommand> m_commands;
            std::vector<DrawBatch> m_batches;
        };

        struct InstanceData
        {
            Matrix4x4_SIMD m_worldMatrix;
            Vector4 m_color;
        };
        static_assert(sizeof(InstanceData) == 80 && offsetof(InstanceData, m_color) == 64);

        void DrawMesh(DX12Frames& frames, const DX12Resources& resources, const DXMesh& mesh, const SubMesh& range,
            D3D_PRIMITIVE_TOPOLOGY topology, std::span<const VertexStream> streams, UINT instanceCount);

        std::array<PhaseDraws, 3> m_phaseDraws; // Opaque、Masked、Transparent の順
        std::vector<InstanceData> m_instances; // バッチごとのUpload用作業領域
    };
}

#endif // _FORWARD_RENDER_PASS_H_
