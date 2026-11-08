/*=============================================================================

 File   : DX12Draw.h
 Desc   : 描画提出、Material バインドと Draw 記録を管理する。

------------------------------------------------------------------------------

 Date   : 2026/11/07
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _DX12_DRAW_H_
#define _DX12_DRAW_H_

#include "Graphics/DX12Object.h"
#include "Graphics/DX12Support.h"
#include <array>
#include <span>
#include <vector>

namespace Hestia
{
    class DX12Frames;
    class DX12Resources;
    class DX12Descriptors;
    class DX12Pipelines;
    class MaterialShader;

    /// @brief 描画提出を Pass に展開し、現在のフレームのコマンドリストに記録する。
    class DX12Draw
    {
    public:
        bool CreateConstants(ID3D12Device* device);
        void TakeSubmissions(std::vector<RenderData>& renderData) { m_currentFrameData.swap(renderData); }
        void ClearSubmissions();
        void Finalize();

        void Draw(ID3D12Device* device, DX12Frames& frames, const DX12Resources& resources,
            const DX12Descriptors& descriptors, DX12Pipelines& pipelines);

    private:
        struct ForwardDraw
        {
            const DXMesh* m_mesh;
            const SubMesh* m_subMesh;
            const Material* m_material;
            const MaterialShader* m_shader;
            std::size_t m_objectIndex;
            std::size_t m_passIndex;
        };

        void ExpandSubmissions(const DX12Resources& resources);
        void BindObject(DX12Frames& frames, const Matrix4x4_SIMD& worldMatrix, size_t objectIndex);
        bool BindMaterial(ID3D12Device* device, DX12Frames& frames, const DX12Resources& resources,
            const DX12Descriptors& descriptors, const Material& material, const MaterialShader& shader);
        void DrawMesh(DX12Frames& frames, const DX12Resources& resources, const DXMesh& mesh, const SubMesh& range,
            D3D_PRIMITIVE_TOPOLOGY topology, std::span<const VertexStream> streams);

        struct PerObjectCB
        {
            ComPtr<ID3D12Resource> constantBuffer;
            void* mappedData = nullptr;
            static constexpr size_t SIZE = (sizeof(PerObjectConstants) + 255) & ~size_t(255);
        } m_PerObjectCB;

        std::vector<RenderData> m_currentFrameData;
        std::array<std::vector<ForwardDraw>, 3> m_forwardDraws; // Opaque、Masked、Transparent の順
    };
}

#endif // _DX12_DRAW_H_
