/*=============================================================================

 File   : DX12Presentation.h
 Desc   : SwapChain、描画先、Depth、Present を管理する。

------------------------------------------------------------------------------

 Date   : 2026/11/07
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _DX12_PRESENTATION_H_
#define _DX12_PRESENTATION_H_

#include "Graphics/DX12Object.h"
#include <dxgi1_6.h>

namespace Hestia
{
    class DX12Descriptors;

    /// @brief SceneColor に描画し、フレーム末尾で BackBuffer へ転送する。
    class DX12Presentation
    {
    public:
        bool CreateSwapChain(IDXGIFactory7* dxgiFactory, ID3D12CommandQueue* queue, HWND hWnd, int width, int height);
        bool CreateTargets(ID3D12Device* device, DX12Descriptors& descriptors, int width, int height);
        void SetViewport(int width, int height);
        void BeginDraw(ID3D12GraphicsCommandList* cmd, const DX12Descriptors& descriptors);
        void RecordBackBufferCopy(ID3D12GraphicsCommandList* commandList);
        void Present();
        void Finalize();

    private:
        static constexpr int MAX_BACKBUFFER_COUNT = 2;
        static constexpr float CLEAR_COLOR[4] = { 0.1f, 0.4f, 0.5f, 1.0f };
        ComPtr<IDXGISwapChain4> m_SwapChain;
        ComPtr<ID3D12Resource> m_BackBuffers[MAX_BACKBUFFER_COUNT];
        bool m_AllowTearing = false;
        DXRenderTexture m_sceneColors[2];
        int m_sceneColorIndex = 0;
        DXDepthStencil m_DepthStencil;
        D3D12_VIEWPORT m_Viewport{};
        D3D12_RECT m_ScissorRect{};
    };
}

#endif // _DX12_PRESENTATION_H_
