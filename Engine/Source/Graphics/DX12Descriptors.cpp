/*=============================================================================

 File   : DX12Descriptors.cpp
 Desc   : Descriptor Heap とビューのスロットを管理する。

------------------------------------------------------------------------------

 Date   : 2026/11/07
 Author : Yokoyama Haruki

=============================================================================*/

#include "pch.h"
#include "Graphics/DX12Descriptors.h"

using namespace Hestia;

bool DX12Descriptors::Initialize(ID3D12Device* device)
{
    // CPU 側のスロット番号は作成順に割り当て、Backend の終了まで維持する。
    HRESULT hr;
    // 各DescriptorHeapのインクリメントサイズを取得
    m_RTVIncrementSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    m_DSVIncrementSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    m_SRVIncrementSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    // RTVHeapの作成
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
    heapDesc.NumDescriptors = MAX_RTV_COUNT;
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    hr = device->CreateDescriptorHeap(
        &heapDesc, IID_PPV_ARGS(m_RTVHeap.GetAddressOf()));
    if (!CheckDX12Result(hr, L"Failed to create RTV descriptor heap.")) return false;

    // RTVHeapの先頭ハンドルを取得
    m_RTVHeapStart = m_RTVHeap->GetCPUDescriptorHandleForHeapStart();


    // DSVHeapの作成
    heapDesc.NumDescriptors = 64;
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    hr = device->CreateDescriptorHeap(
        &heapDesc, IID_PPV_ARGS(m_DSVHeap.GetAddressOf()));
    if (!CheckDX12Result(hr, L"Failed to create DSV descriptor heap.")) return false;

    // DSVHeapの先頭ハンドルを取得
    m_DSVHeapStart = m_DSVHeap->GetCPUDescriptorHandleForHeapStart();


    // SRVMasterHeapの作成
    heapDesc.NumDescriptors = MAX_MASTER_SRV_COUNT;
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    hr = device->CreateDescriptorHeap(
        &heapDesc, IID_PPV_ARGS(m_SRVMasterHeap.GetAddressOf()));
    if (!CheckDX12Result(hr, L"Failed to create SRV master descriptor heap.")) return false;
	
    // SRVMasterHeapの先頭ハンドルを取得
    m_SRVMasterHeapStart = m_SRVMasterHeap->GetCPUDescriptorHandleForHeapStart();


    return true;
}

bool DX12Descriptors::CreateVisibleHeap(ID3D12Device* device, ComPtr<ID3D12DescriptorHeap>& heap) const
{
    // Material Texture の連続スロットを Draw ごとに使用する。
    D3D12_DESCRIPTOR_HEAP_DESC desc{};
    desc.NumDescriptors = MAX_SYSTEM_TEXTURE + MAX_MATERIAL_TEXTURE * DX12_MAX_MATERIAL_COUNT;
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    return CheckDX12Result(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(heap.GetAddressOf())),
        L"Failed to create SRV visible descriptor heap.");
}

void DX12Descriptors::Finalize()
{
    m_RTVHeap.Reset();
    m_DSVHeap.Reset();
    m_SRVMasterHeap.Reset();
    m_RTVHeapIndex = m_DSVHeapIndex = m_SRVMasterHeapIndex = 0;
    m_RTVIncrementSize = m_DSVIncrementSize = m_SRVIncrementSize = 0;
}

bool DX12Descriptors::CreateShaderResourceView(ID3D12Device* device,
    ID3D12Resource * resource,
    uint32_t& srvHeapIndex,
    D3D12_SHADER_RESOURCE_VIEW_DESC* srvDesc)
{
    if (!device || !resource || !m_SRVMasterHeap)
        return false;

    if (m_SRVMasterHeapIndex >= MAX_MASTER_SRV_COUNT)
    {
        MessageBox(nullptr, L"Exceeded maximum SRV count.", L"Error", MB_OK);
        return false;
    }

    // SRVの作成
    D3D12_CPU_DESCRIPTOR_HANDLE handle = m_SRVMasterHeapStart;
    handle.ptr += m_SRVIncrementSize * m_SRVMasterHeapIndex;

    device->CreateShaderResourceView(
        resource,
        srvDesc,
        handle);

    // SRVヒープのインデックスを返す
    srvHeapIndex = static_cast<uint32_t>(m_SRVMasterHeapIndex);

    // SRVヒープのインデックスを更新
    m_SRVMasterHeapIndex++;

    return true;
}

bool DX12Descriptors::CreateRenderTargetView(ID3D12Device* device,
    ID3D12Resource* resource,
    int& rtvHeapIndex,
    D3D12_RENDER_TARGET_VIEW_DESC* rtvDesc)
{
    if(m_RTVHeapIndex >= MAX_RTV_COUNT)
    {
        MessageBox(nullptr, L"Exceeded maximum RTV count.", L"Error", MB_OK);
        return false;
    }

    // RTVの作成
    D3D12_CPU_DESCRIPTOR_HANDLE handle = m_RTVHeapStart;
    handle.ptr += m_RTVIncrementSize * m_RTVHeapIndex;
    device->CreateRenderTargetView(
        resource,
        rtvDesc,
        handle);

    // RTVヒープのインデックスを返す
    rtvHeapIndex = m_RTVHeapIndex;

    // RTVヒープのインデックスを更新
    m_RTVHeapIndex++;
    return true;
}

bool DX12Descriptors::CreateDepthStencilView(ID3D12Device* device,
    ID3D12Resource* resource,
    int& dsvHeapIndex,
    D3D12_DEPTH_STENCIL_VIEW_DESC* dsvDesc)
{
    if(m_DSVHeapIndex >= MAX_DSV_COUNT)
    {
        MessageBox(nullptr, L"Exceeded maximum DSV count.", L"Error", MB_OK);
        return false;
    }

    // DSVの作成
    D3D12_CPU_DESCRIPTOR_HANDLE handle = m_DSVHeapStart;
    handle.ptr += m_DSVIncrementSize * m_DSVHeapIndex;
    device->CreateDepthStencilView(
        resource,
        dsvDesc,
        handle);

    // DSVヒープのインデックスを返す
    dsvHeapIndex = m_DSVHeapIndex;

    // DSVヒープのインデックスを更新
    m_DSVHeapIndex++;
    return true;
}
