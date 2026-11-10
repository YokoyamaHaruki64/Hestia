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
        bool CreateConstants(ID3D12Device* device);
        /// @brief コマンドが持つ借用参照を破棄し、次のフレームに容量を再利用する
        void ClearFrame();
        void Finalize();

        void Build(std::span<const RenderData> submissions, const DX12Resources& resources) override;
        void Sort() override;
        void Record(std::span<const RenderData> submissions, const RenderPassRecordContext& context) override;

    private:
        struct PhaseDraws
        {
            std::vector<DrawCommand> m_commands;
            std::vector<DrawBatch> m_batches;
        };

        void BindObject(DX12Frames& frames, const Matrix4x4_SIMD& worldMatrix, size_t objectIndex);
        bool BindMaterial(ID3D12Device* device, DX12Frames& frames, const DX12Resources& resources,
            const DX12Descriptors& descriptors, const Material& material, const MaterialShader& shader);
        void DrawMesh(DX12Frames& frames, const DX12Resources& resources, const DXMesh& mesh, const SubMesh& range,
            D3D_PRIMITIVE_TOPOLOGY topology, std::span<const VertexStream> streams, UINT instanceCount);

        struct PerObjectCB
        {
            ComPtr<ID3D12Resource> constantBuffer;
            void* mappedData = nullptr;
            static constexpr size_t SIZE = (sizeof(PerObjectConstants) + 255) & ~size_t(255);
        } m_PerObjectCB;

        std::array<PhaseDraws, 3> m_phaseDraws; // Opaque、Masked、Transparent の順
    };
}

#endif // _FORWARD_RENDER_PASS_H_
