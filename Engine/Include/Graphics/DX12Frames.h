/*=============================================================================

 File   : DX12Frames.h
 Desc   : フレームごとの記録資源と定数領域を管理する。

------------------------------------------------------------------------------

 Date   : 2026/11/07
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _DX12_FRAMES_H_
#define _DX12_FRAMES_H_

#include "Graphics/DX12Object.h"
#include "Graphics/DX12Support.h"
#include <array>
#include <span>
#include <vector>

namespace Hestia
{
    class DX12Device;
    class DX12Presentation;
    class DX12Descriptors;
    class DX12Pipelines;

    /// @brief フレームのフェンスと同じ寿命を持つコマンド記録・定数・Descriptor を保持する。
    class DX12Frames
    {
    public:
        bool CreateVisibleHeaps(ID3D12Device* device, const DX12Descriptors& descriptors);
        bool CreateCommandLists(ID3D12Device* device);
        bool CreateConstants(ID3D12Device* device);
        bool BeginDraw(DX12Device& device, DX12Presentation& presentation, const DX12Descriptors& descriptors,
            const DX12Pipelines& pipelines, const Matrix4x4_SIMD& view, const Matrix4x4_SIMD& projection);
        bool EndDraw(DX12Device& device, DX12Presentation& presentation);
        D3D12_GPU_VIRTUAL_ADDRESS UploadMaterialConstants(ID3D12Device* device, std::span<const std::byte> bytes);
        void Finalize();

        /// @brief Frame の残り Descriptor 容量を確認する。
        bool CanCommitMaterialTextures() const;
        void CommitMaterialTextures(ID3D12Device* device, const DX12Descriptors& descriptors,
            const std::array<int, MAX_MATERIAL_TEXTURE>& slots);

        ID3D12GraphicsCommandList* GetCommandList() { return Current().commandList.Get(); }
        int Index() const { return m_frameIndex; }

    private:
        struct FrameContext
        {
            struct MaterialConstantPage
            {
                ComPtr<ID3D12Resource> resource;
                std::byte* mappedData = nullptr;
            };

            ComPtr<ID3D12CommandAllocator> commandAllocator;
            ComPtr<ID3D12GraphicsCommandList> commandList;
            ComPtr<ID3D12Resource> perFrameCB;
            ComPtr<ID3D12DescriptorHeap> srvVisibleHeap; // フレームごとのSRVヒープ

            // Material Texture のテーブル数。各テーブルは MAX_MATERIAL_TEXTURE 枠を占める。
            int visibleSRVCount = 0;
            std::vector<MaterialConstantPage> materialConstantPages;
            std::size_t materialConstantOffset = 0;

            // この番号が完了するまで Allocator、定数、Descriptor を上書きしない。
            UINT64 fenceValue = 0;
        };

        FrameContext& Current() { return m_FrameContexts[m_frameIndex]; }
        FrameContext m_FrameContexts[DX12_FRAME_COUNT];
        int m_frameIndex = 0;
    };
}

#endif // _DX12_FRAMES_H_
