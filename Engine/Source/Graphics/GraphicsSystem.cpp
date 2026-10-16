/*=============================================================================

 File   : GraphicsSystem.cpp
 Desc   : GraphicsSystem の初期化、Asset Upload、描画受付を実装する。

------------------------------------------------------------------------------

 Date   : 2026/10/08
 Author : Yokoyama Haruki

=============================================================================*/

#include "pch.h"
#include "Graphics/GraphicsSystem.h"

#include "Graphics/DX12Backend.h"

namespace Hestia
{
    GraphicsSystem::GraphicsSystem() = default;

    GraphicsSystem::~GraphicsSystem()
    {
        Finalize();
    }

    bool GraphicsSystem::Initialize(HWND hWnd, int width, int height)
    {
        if (s_instance != nullptr)
            return false;

        if (m_DX12Backend || !hWnd || width <= 0 || height <= 0)
            return false;

        auto backend = std::make_unique<DX12Backend>();
        if (!backend->Initialize(hWnd, width, height))
            return false;

        m_view = Matrix4x4::Identity();
        m_projection = Matrix4x4::OrthographicLH(
            static_cast<float>(width), static_cast<float>(height), 0.1f, 100.0f);

        m_renderQueue.reserve(DX12Backend::MAX_DRAW_COUNT);
        m_DX12Backend = std::move(backend);
        s_instance = this;
        return true;
    }

    void GraphicsSystem::Finalize()
    {
        if (m_DX12Backend)
        {
            m_DX12Backend->Finalize();
            m_DX12Backend.reset();
        }

        m_renderQueue.clear();
        if (s_instance == this)
            s_instance = nullptr;
    }

    bool GraphicsSystem::Submit(RenderData renderData)
    {
        if (!s_instance || !s_instance->m_DX12Backend ||
            s_instance->m_renderQueue.size() >= DX12Backend::MAX_DRAW_COUNT ||
            !s_instance->m_DX12Backend->HasMesh(renderData.mesh))
            return false;

        s_instance->m_renderQueue.push_back(std::move(renderData));
        return true;
    }

    void GraphicsSystem::SetView(const Matrix4x4& view)
    {
        if (s_instance)
            s_instance->m_view = view;
    }

    void GraphicsSystem::SetProj(const Matrix4x4& projection)
    {
        if (s_instance)
            s_instance->m_projection = projection;
    }

    bool GraphicsSystem::UploadMesh(AssetHandle<Mesh> handle, const Mesh& mesh)
    {
        return m_DX12Backend && m_DX12Backend->UploadMesh(handle, mesh);
    }

    bool GraphicsSystem::UploadTexture(
        AssetHandle<Texture> handle,
        const TextureDesc& desc,
        const void* pixels,
        size_t rowPitch,
        size_t slicePitch)
    {
        return m_DX12Backend && m_DX12Backend->UploadTexture(
            handle, desc, pixels, rowPitch, slicePitch);
    }

    void GraphicsSystem::Render()
    {
        if (!m_DX12Backend)
            return;

        m_DX12Backend->Update();

        if (!m_DX12Backend->BeginDraw(m_renderQueue, m_view, m_projection))
            return;

        m_DX12Backend->Draw();
        m_DX12Backend->EndDraw();
    }
}
