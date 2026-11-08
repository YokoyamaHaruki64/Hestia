/*=============================================================================

 File   : DX12Descriptors.h
 Desc   : Descriptor Heap とビューのスロットを管理する。

------------------------------------------------------------------------------

 Date   : 2026/11/07
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _DX12_DESCRIPTORS_H_
#define _DX12_DESCRIPTORS_H_

#include "Graphics/DX12Object.h"
#include "Graphics/DX12Support.h"


namespace Hestia
{
    /// @brief CPU 側の Descriptor を割り当て、フレーム専用の Visible Heap を作る。
    class DX12Descriptors
    {
    public:
        bool Initialize(ID3D12Device* device);
        void Finalize();
        bool CreateVisibleHeap(ID3D12Device* device, ComPtr<ID3D12DescriptorHeap>& heap) const;
        bool CreateShaderResourceView(ID3D12Device* device, ID3D12Resource* resource, uint32_t& index,
            D3D12_SHADER_RESOURCE_VIEW_DESC* desc = nullptr);
        bool CreateRenderTargetView(ID3D12Device* device, ID3D12Resource* resource, int& index,
            D3D12_RENDER_TARGET_VIEW_DESC* desc = nullptr);
        bool CreateDepthStencilView(ID3D12Device* device, ID3D12Resource* resource, int& index,
            D3D12_DEPTH_STENCIL_VIEW_DESC* desc = nullptr);

        D3D12_CPU_DESCRIPTOR_HANDLE GetRTVHandle(int rtvHeapIndex) const
        {
            D3D12_CPU_DESCRIPTOR_HANDLE handle = m_RTVHeapStart;
            handle.ptr += m_RTVIncrementSize * rtvHeapIndex;
            return handle;
        }
        D3D12_CPU_DESCRIPTOR_HANDLE GetDSVHandle(int dsvHeapIndex) const
        {
            D3D12_CPU_DESCRIPTOR_HANDLE handle = m_DSVHeapStart;
            handle.ptr += m_DSVIncrementSize * dsvHeapIndex;
            return handle;
        }
        D3D12_CPU_DESCRIPTOR_HANDLE GetSRVHandle(uint32_t srvHeapIndex) const
        {
            D3D12_CPU_DESCRIPTOR_HANDLE handle = m_SRVMasterHeapStart;
            handle.ptr += m_SRVIncrementSize * srvHeapIndex;
            return handle;
        }

        UINT GetSRVIncrementSize() const { return m_SRVIncrementSize; }

    private:
        static constexpr int MAX_RTV_COUNT = 64;
        static constexpr int MAX_DSV_COUNT = 64;
        static constexpr int MAX_MASTER_SRV_COUNT = 2048;
        static constexpr int MAX_SYSTEM_TEXTURE = 16;
        ComPtr<ID3D12DescriptorHeap> m_RTVHeap;
        ComPtr<ID3D12DescriptorHeap> m_DSVHeap;
        ComPtr<ID3D12DescriptorHeap> m_SRVMasterHeap;

        // RTVヒープの管理
        D3D12_CPU_DESCRIPTOR_HANDLE m_RTVHeapStart{};	// 先頭ハンドル
        UINT m_RTVIncrementSize = 0;					// インクリメントサイズ
        int m_RTVHeapIndex = 0;							// インデックス

        // DSVヒープの管理
        D3D12_CPU_DESCRIPTOR_HANDLE m_DSVHeapStart{};
        UINT m_DSVIncrementSize = 0;
        int m_DSVHeapIndex = 0;

        // SRVヒープの管理
        D3D12_CPU_DESCRIPTOR_HANDLE m_SRVMasterHeapStart{};
        UINT m_SRVIncrementSize = 0;
        int m_SRVMasterHeapIndex = 0;
    };
}

#endif // _DX12_DESCRIPTORS_H_
