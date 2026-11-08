/*=============================================================================

 File   : DX12Presentation.cpp
 Desc   : SwapChain、描画先、Depth、Present を管理する。

------------------------------------------------------------------------------

 Date   : 2026/11/07
 Author : Yokoyama Haruki

=============================================================================*/

#include "pch.h"
#include "Graphics/DX12Presentation.h"

#include "Graphics/DX12Support.h"

#include "Graphics/DX12Descriptors.h"
#include "D3DX12/d3dx12.h"

using namespace Hestia;

bool DX12Presentation::CreateSwapChain(IDXGIFactory7* dxgiFactory, ID3D12CommandQueue* queue, HWND hWnd,
    int width, int height)
{
    HRESULT hr;
    // SwapChainの作成
    BOOL allowTearing = FALSE;

    dxgiFactory->CheckFeatureSupport(
        DXGI_FEATURE_PRESENT_ALLOW_TEARING,
        &allowTearing,
        sizeof(allowTearing));

    m_AllowTearing = allowTearing == TRUE;

    DXGI_SWAP_CHAIN_DESC1 desc{};
    desc.Width = width;
    desc.Height = height;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.Stereo = FALSE;

    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;

    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = MAX_BACKBUFFER_COUNT;

    desc.Scaling = DXGI_SCALING_STRETCH;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    desc.Flags = m_AllowTearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

    ComPtr<IDXGISwapChain1> swapChain;
    hr = dxgiFactory->CreateSwapChainForHwnd(
        queue,
        hWnd,
        &desc,
        nullptr,
        nullptr,
        swapChain.GetAddressOf());
    if (!CheckDX12Result(hr, L"Failed to create swap chain.")) return false;
    if (!CheckDX12Result(swapChain.As(&m_SwapChain), L"Failed to query swap chain.")) return false;

    // BackBufferの取得
    for (int i = 0; i < MAX_BACKBUFFER_COUNT; ++i)
    {
        hr = m_SwapChain->GetBuffer(i, IID_PPV_ARGS(m_BackBuffers[i].GetAddressOf()));
        if (!CheckDX12Result(hr, L"Failed to get back buffer.")) return false;
    }


    return true;
}

bool DX12Presentation::CreateTargets(ID3D12Device* device, DX12Descriptors& descriptors, int width, int height)
{
    HRESULT hr;
    // RenderTargetの作成
    D3D12_RESOURCE_DESC resourceDesc{};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resourceDesc.Alignment = 0;
    resourceDesc.Width = width;
    resourceDesc.Height = height;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.SampleDesc.Quality = 0;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    D3D12_CLEAR_VALUE clearValue{};
    clearValue.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    for( int i = 0; i < 4; ++i)
        clearValue.Color[i] = CLEAR_COLOR[i];

    D3D12_HEAP_PROPERTIES heapProperties{};
    heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

    for (int i = 0; i < 2; ++i)
    {

        hr = device->CreateCommittedResource(
            &heapProperties,
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc,
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            &clearValue,
            IID_PPV_ARGS(m_sceneColors[i].resource.GetAddressOf()));
        if (!CheckDX12Result(hr, L"Failed to create render target.")) return false;

        // RTVの作成
        if (!descriptors.CreateRenderTargetView(device, m_sceneColors[i].resource.Get(), m_sceneColors[i].rtvHeapIndex))
        {
            MessageBox(nullptr, L"Failed to create RTV for render target.", L"Error", MB_OK);
            return false;
        }
		
        // SRVの作成
        if (!descriptors.CreateShaderResourceView(device, m_sceneColors[i].resource.Get(), m_sceneColors[i].srvHeapIndex))
        {
            MessageBox(nullptr, L"Failed to create SRV for render target.", L"Error", MB_OK);
            return false;
        }

    }

    // DepthStancilResourceの作成
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    resourceDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;

    clearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    clearValue.DepthStencil.Depth = 1.0f;
    clearValue.DepthStencil.Stencil = 0;

    hr = device->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &clearValue,
        IID_PPV_ARGS(m_DepthStencil.resource.GetAddressOf()));
    if (!CheckDX12Result(hr, L"Failed to create depth stencil.")) return false;

    // DSVの作成
    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
    dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Flags = D3D12_DSV_FLAG_NONE;

    if (!descriptors.CreateDepthStencilView(device, m_DepthStencil.resource.Get(), m_DepthStencil.dsvHeapIndex, &dsvDesc))
    {
        MessageBox(nullptr, L"Failed to create DSV for depth stencil.", L"Error", MB_OK);
        return false;
    }

    // SRV(Depth)の作成
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping =
        D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = 1;
    srvDesc.Texture2D.PlaneSlice = 0;

    if (!descriptors.CreateShaderResourceView(device, m_DepthStencil.resource.Get(), m_DepthStencil.depthSrvHeapIndex, &srvDesc))
    {
        MessageBox(nullptr, L"Failed to create SRV for depth stencil.", L"Error", MB_OK);
        return false;
    }

    // SRV(Stencil)の作成
    srvDesc.Format = DXGI_FORMAT_X24_TYPELESS_G8_UINT;
    srvDesc.Texture2D.PlaneSlice = 1;

    if(!descriptors.CreateShaderResourceView(device, m_DepthStencil.resource.Get(), m_DepthStencil.stencilSrvHeapIndex, &srvDesc))
    {
        MessageBox(nullptr, L"Failed to create SRV for stencil.", L"Error", MB_OK);
        return false;
    }

    return true;
}

void DX12Presentation::SetViewport(int width, int height)
{
    // Viewportの設定

    m_Viewport.TopLeftX = 0.0f;
    m_Viewport.TopLeftY = 0.0f;
    m_Viewport.Width = static_cast<float>(width);
    m_Viewport.Height = static_cast<float>(height);
    m_Viewport.MinDepth = 0.0f;
    m_Viewport.MaxDepth = 1.0f;

    m_ScissorRect.left = 0;
    m_ScissorRect.top = 0;
    m_ScissorRect.right = width;
    m_ScissorRect.bottom = height;
}

void DX12Presentation::BeginDraw(ID3D12GraphicsCommandList* cmd, const DX12Descriptors& descriptors)
{
    // ViewportとScissorRectの設定
    cmd->RSSetViewports(1, &m_Viewport);
    cmd->RSSetScissorRects(1, &m_ScissorRect);

    // RenderTargetとDepthStencilの設定
    auto rtvHandle = descriptors.GetRTVHandle(m_sceneColors[m_sceneColorIndex].rtvHeapIndex);
    auto dsvHandle = descriptors.GetDSVHandle(m_DepthStencil.dsvHeapIndex);
    cmd->OMSetRenderTargets(
        1, 
        &rtvHandle,
        FALSE, 
        &dsvHandle);

    // RenderTargetとDepthStencilのクリア

    cmd->ClearRenderTargetView(
        rtvHandle,
        CLEAR_COLOR, 0, nullptr);

    cmd->ClearDepthStencilView(
        dsvHandle,
        D3D12_CLEAR_FLAG_DEPTH,
        1.0f, 0, 0, nullptr);
}

void DX12Presentation::RecordBackBufferCopy(ID3D12GraphicsCommandList* commandList)
{
    // BackBufferへのコピー
    size_t backBufferIndex = m_SwapChain->GetCurrentBackBufferIndex();

    // 描画先から転送できる状態へ移し、コピー後は次の描画と Present に備えて戻す。
    std::array<D3D12_RESOURCE_BARRIER, 2> barriers{};
    barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(
        m_sceneColors[m_sceneColorIndex].resource.Get(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_COPY_SOURCE);
    // BackBufferの状態をCOPY_DESTに変更
    barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(
        m_BackBuffers[backBufferIndex].Get(),
        D3D12_RESOURCE_STATE_PRESENT,
        D3D12_RESOURCE_STATE_COPY_DEST);

    commandList->ResourceBarrier(barriers.size(), barriers.data());

    commandList->CopyResource(
        m_BackBuffers[backBufferIndex].Get(),
        m_sceneColors[m_sceneColorIndex].resource.Get());

    // RTVの状態をRENDER_TARGETに戻す
    barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(
        m_sceneColors[m_sceneColorIndex].resource.Get(),
        D3D12_RESOURCE_STATE_COPY_SOURCE,
        D3D12_RESOURCE_STATE_RENDER_TARGET);

    // BackBufferの状態をPRESENTに戻す
    barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(
        m_BackBuffers[backBufferIndex].Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_PRESENT);

    commandList->ResourceBarrier(barriers.size(), barriers.data());
}

void DX12Presentation::Present()
{
    // Present

    UINT presentFlags = 0;
    if(m_AllowTearing)
        presentFlags |= DXGI_PRESENT_ALLOW_TEARING;

    m_SwapChain->Present(0, presentFlags);
}

void DX12Presentation::Finalize()
{
    // GPU 完了後に描画先を解放する。
    for (auto& sceneColor : m_sceneColors)
        sceneColor = {};
    m_DepthStencil = {};
    for (auto& backBuffer : m_BackBuffers)
        backBuffer.Reset();
    m_SwapChain.Reset();
    m_sceneColorIndex = 0;
    m_AllowTearing = false;
}
