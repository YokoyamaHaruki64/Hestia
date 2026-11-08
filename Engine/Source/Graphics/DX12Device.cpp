/*=============================================================================

 File   : DX12Device.cpp
 Desc   : デバイス、Direct Queue、共有フェンスを管理する。

------------------------------------------------------------------------------

 Date   : 2026/11/07
 Author : Yokoyama Haruki

=============================================================================*/

#include "pch.h"
#include "Graphics/DX12Device.h"

#include "Graphics/DX12Support.h"

#include <dxgidebug.h>

using namespace Hestia;

bool DX12Device::CreateDevice(ComPtr<IDXGIFactory7>& dxgiFactory)
{
    HRESULT hr = S_OK;

    // デバッグレイヤーの有効化
#ifdef _DEBUG

    ComPtr<ID3D12Debug> debugController;
    if (SUCCEEDED(
        D3D12GetDebugInterface(
            IID_PPV_ARGS(debugController.GetAddressOf()))))
    {
        debugController->EnableDebugLayer();
    }

#endif

    // Deviceの作成
    hr = CreateDXGIFactory1(IID_PPV_ARGS(dxgiFactory.GetAddressOf()));
    if (!CheckDX12Result(hr, L"Failed to create DXGI factory.")) return false;

    ComPtr<IDXGIAdapter4> adapter;
    bool foundSoftwareAdapter = false;

    // 従来どおり高性能順にアダプタを確認し、最初のハードウェア候補を選ぶ。
    for( UINT i = 0; 
        dxgiFactory->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(adapter.GetAddressOf())) != DXGI_ERROR_NOT_FOUND; i++)
    {
        DXGI_ADAPTER_DESC3 desc{};
        adapter->GetDesc3(&desc);
        if (desc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE)
        {
            foundSoftwareAdapter = true;
            continue;
        }

        // デバイスの作成
        hr = D3D12CreateDevice(
            adapter.Get(), 
            D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(m_Device.GetAddressOf()));

        if (!CheckDX12Result(hr, L"Failed to create D3D12 device.")) return false;

        break;
    }

    if(!m_Device)
    {
        if(foundSoftwareAdapter)
        {
            // ソフトウェアアダプタを使用する
            hr = D3D12CreateDevice(
                nullptr, 
                D3D_FEATURE_LEVEL_11_0,
                IID_PPV_ARGS(m_Device.GetAddressOf()));

            if (!CheckDX12Result(hr, L"Failed to create D3D12 device.")) return false;
        }
        else
        {
            MessageBox(nullptr, L"Failed to find a suitable GPU.", L"Error", MB_OK);
            return false;
        }
    }

    return true;
}

bool DX12Device::CreateQueue()
{
    HRESULT hr;
    // CommandQueueの作成
    D3D12_COMMAND_QUEUE_DESC queueDesc{};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.NodeMask = 0;
    queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;

    hr = m_Device->CreateCommandQueue(
        &queueDesc, IID_PPV_ARGS(m_CommandQueue.GetAddressOf()));

    if (!CheckDX12Result(hr, L"Failed to create command queue.")) return false;

    return true;
}

bool DX12Device::CreateFence()
{
    HRESULT hr;
    // Fenceの作成
    hr = m_Device->CreateFence(
        0,
        D3D12_FENCE_FLAG_NONE,
        IID_PPV_ARGS(m_Fence.GetAddressOf()));
    if (!CheckDX12Result(hr, L"Failed to create fence.")) return false;

    // FenceEventの作成
    m_FenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!m_FenceEvent) return false;


    if (!CheckDX12Result(m_Fence->Signal(0), L"Failed to initialize fence.")) return false;

    return true;
}

bool DX12Device::WaitForGPU()
{
    if (!m_CommandQueue || !m_Fence)
        return true;

    const UINT64 fenceValue = NextFenceValue();
    if (FAILED(m_CommandQueue->Signal(m_Fence.Get(), fenceValue)))
        return false;

    if (m_Fence->GetCompletedValue() >= fenceValue)
        return true;

    // 途中まで初期化された状態でも、フェンスを待ってから資源を解放する。
    HANDLE eventHandle = m_FenceEvent;
    const bool ownsEvent = eventHandle == nullptr;
    if (ownsEvent)
        eventHandle = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!eventHandle)
        return false;

    const bool eventSet = SUCCEEDED(m_Fence->SetEventOnCompletion(fenceValue, eventHandle));
    const bool completed = eventSet &&
        WaitForSingleObject(eventHandle, INFINITE) == WAIT_OBJECT_0 &&
        m_Fence->GetCompletedValue() >= fenceValue;
    if (ownsEvent)
        CloseHandle(eventHandle);

    return completed;
}
void DX12Device::Finalize()
{
    // Queue を待った後に呼び出し、次回初期化に備えて番号も戻す。
    m_CommandQueue.Reset();
    m_Fence.Reset();
    m_Device.Reset();
    if (m_FenceEvent)
    {
        CloseHandle(m_FenceEvent);
        m_FenceEvent = nullptr;
    }
    m_globalFenceValue = 0;
}
