/*=============================================================================

 File   : DX12Backend.cpp
 Desc   : DX12 の機能クラスを初期化し、描画要求を委譲する。

------------------------------------------------------------------------------

 Date   : 2026/09/30
 Author : Yokoyama Haruki

=============================================================================*/

#include "pch.h"
#include "Graphics/DX12Backend.h"

using namespace Hestia;

bool DX12Backend::Initialize(HWND hWnd, int width, int height)
{
    if (m_device.GetDevice() || !hWnd || width <= 0 || height <= 0)
        return false;

    // Device と Queue を先に作り、描画先と Descriptor が同じ Device を使うようにする。
    ComPtr<IDXGIFactory7> factory;
    if (!m_device.CreateDevice(factory) || !m_device.CreateQueue() ||
        !m_presentation.CreateSwapChain(factory.Get(), m_device.GetQueue(), hWnd, width, height) ||
        !m_descriptors.Initialize(m_device.GetDevice()) ||
        !m_frames.CreateVisibleHeaps(m_device.GetDevice(), m_descriptors) ||
        !m_presentation.CreateTargets(m_device.GetDevice(), m_descriptors, width, height))
        return false;

    // 転送と描画でフェンスを共用する。白 Texture は公開 Upload API を有効化する前に用意する。
    if (!m_frames.CreateCommandLists(m_device.GetDevice()) ||
        !m_resources.Initialize(m_device.GetDevice()) || !m_device.CreateFence() ||
        !m_resources.CreateWhiteTexture(m_device, m_descriptors))
        return false;

    // フレームを受け付ける前に、固定の描画設定と定数の書き込み先を確保する。
    m_presentation.SetViewport(width, height);
    if (!m_pipelines.Initialize(m_device.GetDevice()) ||
        !m_frames.CreateConstants(m_device.GetDevice()) ||
        !m_draw.CreateConstants(m_device.GetDevice()))
        return false;

    m_IsInitialized = true;
    return true;
}

void DX12Backend::Finalize()
{
    // 新規要求を止め、アップロードや描画が参照している資源の使用が終わってから解放する。
    m_IsInitialized = false;
    if (!m_device.WaitForGPU())
        return;

    m_draw.Finalize();
    m_resources.Finalize();
    m_frames.Finalize();
    m_presentation.Finalize();
    m_pipelines.Finalize();
    m_descriptors.Finalize();
    m_device.Finalize();
}

void DX12Backend::Update()
{
    if (m_IsInitialized)
        m_resources.Update(m_device);
}

bool DX12Backend::BeginDraw(std::vector<RenderData>& renderData, const Matrix4x4& view, const Matrix4x4& projection)
{
    return BeginDraw(renderData, Matrix4x4_SIMD::Load(view), Matrix4x4_SIMD::Load(projection));
}

bool DX12Backend::BeginDraw(std::vector<RenderData>& renderData, const Matrix4x4_SIMD& view,
    const Matrix4x4_SIMD& projection)
{
    if (!m_IsInitialized || renderData.size() > MAX_DRAW_COUNT ||
        !m_frames.BeginDraw(m_device, m_presentation, m_descriptors, m_pipelines, view, projection))
        return false;

    // 記録できる状態になってから提出データを引き受ける。
    m_draw.TakeSubmissions(renderData);
    return true;
}

void DX12Backend::Draw()
{
    m_draw.Draw(m_device.GetDevice(), m_frames, m_resources, m_descriptors, m_pipelines);
}

void DX12Backend::EndDraw()
{
    if (!m_frames.EndDraw(m_device, m_presentation))
    {
        m_IsInitialized = false;
        return;
    }
    m_draw.ClearSubmissions();
}

bool DX12Backend::UploadMesh(AssetHandle<Mesh> handle, const Mesh& mesh)
{
    return m_IsInitialized && m_resources.UploadMesh(m_device, handle, mesh);
}

bool DX12Backend::UploadTexture(AssetHandle<Texture> handle, const TextureDesc& desc,
    const void* pixels, size_t rowPitch, size_t slicePitch)
{
    return m_IsInitialized && m_resources.UploadTexture(m_device, m_descriptors, handle, desc, pixels, rowPitch, slicePitch);
}

bool DX12Backend::HasMesh(AssetHandle<Mesh> handle) const
{
    return m_resources.HasMesh(handle);
}
