/*=============================================================================

 File   : DX12Backend.h
 Desc   : DirectX12を直接叩くレイヤーの宣言

 ------------------------------------------------------------------------------

 Date   : 2026/09/30
 Author : Yokoyama Haruki

===============================================================================*/

#ifndef _DX12_BACKEND_H_
#define _DX12_BACKEND_H_

#include <d3d12.h>
#include <d3dcompiler.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;

#pragma comment (lib, "d3d12.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")

#include <string>
#include <vector>
#include <cassert>
#include <unordered_map>
#include <memory>
#include <filesystem>
#include <array>
#include <queue>
#include <cstdint>
#include "Common/Include/WindowsHeaders.h"

#include "HMath.h"
#include "Graphics/DX12Object.h"

namespace Hestia
{

    class DX12Backend
    {
        struct FrameContext
        {
            ComPtr<ID3D12CommandAllocator> commandAllocator;
            ComPtr<ID3D12GraphicsCommandList> commandList;
            ComPtr<ID3D12Resource> perFrameCB;
            ComPtr<ID3D12DescriptorHeap> srvVisibleHeap; // フレームごとのSRVヒープ

            int visibleSRVCount = 0;	// SRVをコミットした数

            UINT64 fenceValue = 0; // フレームのフェンス値
            std::vector<DXMesh> drewMesh; // Frame Fence 完了まで GPU 資源を保持
        };

        struct WaitingResource
        {
            ComPtr<ID3D12Resource> resource;
            uint64_t fenceValue;
        };

        // テクスチャをGPUにアップロードするためのプール
        // GPUにアップロードする際、遅延して破棄する必要があるため、フェンス値による管理を行う
        class UploadContextPool
        {
        private:
            struct UploadContext
            {
                ComPtr<ID3D12CommandAllocator> allocator;
                ComPtr<ID3D12GraphicsCommandList> commandList;

                UINT64 fenceValue = 0;

                bool isFree{ true };
            };

            ComPtr<ID3D12Device> m_Device;
            static constexpr int INITIAL_CONTEXT_COUNT = 16; // 初期コンテキスト数

            std::vector<UploadContext> contexts;
            bool CreateUploadContext(UploadContext& context);

        public:

            bool Initialize(ComPtr<ID3D12Device> device)
            {
                m_Device = device;
                contexts.reserve(INITIAL_CONTEXT_COUNT);

                for (int i = 0; i < INITIAL_CONTEXT_COUNT; ++i)
                {
                    UploadContext context;
                    if (!CreateUploadContext(context))
                        return false;
                    contexts.push_back(std::move(context));
                }
                return true;
            }

            void ReleaseContext(UINT64 completedFenceValue)
            {
                for (auto& context : contexts)
                {
                    if (!context.isFree && context.fenceValue <= completedFenceValue)
                    {
                        context.isFree = true;
                    }
                }
            }

            /// @brief 空いているUploadContextのインデックスを取得する
            /// @param outIndex 
            /// @return 空きが取得or作成成功:true, 失敗:false
            bool GetFreeIndex(int& outIndex)
            {
                for (int i = 0; i < contexts.size(); ++i)
                {
                    if (contexts[i].isFree)
                    {
                        outIndex = i;
                        return true;
                    }
                }
                UploadContext newContext;
                if (!CreateUploadContext(newContext))
                    return false;

                contexts.push_back(std::move(newContext));
                outIndex = contexts.size() - 1;
                return true;
            }

            /// @brief UploadContextのCommandListをリセットして取得する
            /// @param index GetFreeIndexで取得したindex
            /// @param fenceValue 次のフェンス値(GetNextFenceValue()で取得)
            /// @return リセットされたCommandList
            ComPtr<ID3D12GraphicsCommandList> GetResetCommandList(int index, UINT64 fenceValue)
            {
                if (index < 0 || index >= contexts.size())
                    return nullptr;

                if (FAILED(contexts[index].allocator->Reset()) ||
                    FAILED(contexts[index].commandList->Reset(contexts[index].allocator.Get(), nullptr)))
                    return nullptr;
                contexts[index].fenceValue = fenceValue;
                contexts[index].isFree = false;
                return contexts[index].commandList;
            }
        };

        enum class RootParameterIndex
        {
            PerFrameCBV = 0,	// フレームごとの定数バッファ
            PerObjectCBV,		// オブジェクトごとの定数バッファ
            PerMaterialCBV,		// マテリアルごとの定数バッファ
            PerSystemSRV,		// システムSRV
            PerMaterialSRV,		// マテリアルSRV

            Count
        };


        static constexpr int FRAME_COUNT = 2;
        static constexpr int MAX_BACKBUFFER_COUNT = 2; // 最大バックバッファ数
        static constexpr int MAX_RTV_COUNT = 64; // 最大RTV数
        static constexpr int MAX_DSV_COUNT = 64; // 最大DSV数
        static constexpr int MAX_MASTER_SRV_COUNT = 2048; // 最大SRV数

        static constexpr int MAX_SYSTEM_TEXTURE = 16; // システムで使用するテクスチャの数
        static constexpr int MAX_MATERIAL_TEXTURE = 16; // シェーダーにバインドするテクスチャの数

        static constexpr int MAX_MATERIAL_COUNT = 1 << 15; // 最大マテリアル数(32768)
        static constexpr int MAX_OBJECT_COUNT = 1 << 16; // 最大オブジェクト数(65536)

        static constexpr float CLEAR_COLOR[4] = { 0.1f, 0.4f, 0.5f, 1.0f }; // クリアカラー

        bool m_IsInitialized = false;
        bool m_AllowTearing = false;

        ComPtr<ID3D12Device> m_Device;
        ComPtr<IDXGISwapChain4> m_SwapChain;
        ComPtr<ID3D12CommandQueue> m_CommandQueue;
        ComPtr<ID3D12Resource> m_BackBuffers[MAX_BACKBUFFER_COUNT];

        UploadContextPool m_UploadContextPool;

        FrameContext m_FrameContexts[FRAME_COUNT];
        int m_frameIndex = 0;		// 現在のフレームインデックス

        DXRenderTexture m_sceneColors[2];
        int m_sceneColorIndex = 0;	// PingPong用のインデックス

        DXDepthStencil m_DepthStencil;

        DXTexture m_nullTexture; // Nullテクスチャ

        D3D12_VIEWPORT m_Viewport{};
        D3D12_RECT m_ScissorRect{};

        // Descriptor Heaps
        ComPtr<ID3D12DescriptorHeap> m_RTVHeap;
        ComPtr<ID3D12DescriptorHeap> m_DSVHeap;
        ComPtr<ID3D12DescriptorHeap> m_SRVMasterHeap;
        //ComPtr<ID3D12DescriptorHeap> m_SRVVisibleHeap;

        // Pipeline State
        ComPtr<ID3D12RootSignature> m_RootSignature;
        std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> m_PipelineCache;
        ComPtr<ID3D12PipelineState> m_CurrentPipelineState;
        D3D12_INPUT_LAYOUT_DESC m_InputLayoutDesc{};

        // Constant Buffer
        struct PerMaterialCB
        {
            ComPtr<ID3D12Resource> constantBuffer;
            void* mappedData = nullptr;
            static constexpr size_t SIZE = (sizeof(MaterialConstants) + 255) & ~size_t(255);
            std::unordered_map<std::string, UINT> materialCache;
            UINT maxIndex = 0;
        } m_PerMaterialCB;

        struct PerObjectCB
        {
            ComPtr<ID3D12Resource> constantBuffer;
            void* mappedData = nullptr;
            static constexpr size_t SIZE = (sizeof(PerObjectConstants) + 255) & ~size_t(255);
        } m_PerObjectCB;

        // フェンス
        ComPtr<ID3D12Fence> m_Fence;
        HANDLE m_FenceEvent = nullptr;
        UINT64 m_globalFenceValue = 0;

        // ================================
        // リソース管理用
        // ================================

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

        std::array<int, MAX_MATERIAL_TEXTURE> m_BoundMaterialSRVs;

        // GPUが使用中のリソースを待機するためのキュー
        std::queue<WaitingResource> m_WaitingResources;

        std::vector<DXTexture> m_SystemTextures; // システムで使用するテクスチャのリスト

        std::vector<RenderData> m_currentFrameData; // 現在のフレームのレンダリングデータ

        bool CheckResult(HRESULT hr, const wchar_t* msg)
        {
            if (FAILED(hr))
            {
                MessageBox(nullptr, msg, L"Error", MB_OK);
                return false;
            }

            return true;
        }

        bool CreateShaderResourceView(ID3D12Resource* resource, int& srvHeapIndex, D3D12_SHADER_RESOURCE_VIEW_DESC* srvDesc = nullptr);
        bool CreateRenderTargetView(ID3D12Resource* resource, int& rtvHeapIndex, D3D12_RENDER_TARGET_VIEW_DESC* rtvDesc = nullptr);
        bool CreateDepthStencilView(ID3D12Resource* resource, int& dsvHeapIndex, D3D12_DEPTH_STENCIL_VIEW_DESC* dsvDesc = nullptr);

        bool LoadShaderFromCSO(const wchar_t* path, ComPtr<ID3DBlob>& shaderBlob);

        D3D12_CPU_DESCRIPTOR_HANDLE GetRTVHandle(int rtvHeapIndex)
        {
            D3D12_CPU_DESCRIPTOR_HANDLE handle = m_RTVHeapStart;
            handle.ptr += m_RTVIncrementSize * rtvHeapIndex;
            return handle;
        }
        D3D12_CPU_DESCRIPTOR_HANDLE GetDSVHandle(int dsvHeapIndex)
        {
            D3D12_CPU_DESCRIPTOR_HANDLE handle = m_DSVHeapStart;
            handle.ptr += m_DSVIncrementSize * dsvHeapIndex;
            return handle;
        }
        D3D12_CPU_DESCRIPTOR_HANDLE GetSRVHandle(int srvHeapIndex)
        {
            D3D12_CPU_DESCRIPTOR_HANDLE handle = m_SRVMasterHeapStart;
            handle.ptr += m_SRVIncrementSize * srvHeapIndex;
            return handle;
        }

        UINT64 GetNextFenceValue()
        {
            return ++m_globalFenceValue;
        }

        void WaitForGPU();
        void DrawMesh(const DXMesh& mesh);

    public:
        static constexpr size_t MAX_DRAW_COUNT = MAX_MATERIAL_COUNT;

        DX12Backend() = default;
        ~DX12Backend()
        {
            Finalize();
        };

        bool Initialize(HWND hWnd, int width, int height);
        void Finalize();
        void Update();
        bool BeginDraw(std::vector<RenderData>& renderData, const Matrix4x4& view, const Matrix4x4& projection);
        void Draw();
        void EndDraw();

        bool CreateVertexBuffer(
            const Vertex* vertices,
            size_t vertexCount,
            ComPtr<ID3D12Resource>& vertexBuffer,
            D3D12_VERTEX_BUFFER_VIEW& vertexBufferView);

        bool CreateIndexBuffer(
            const uint32_t* indices,
            size_t indexCount,
            ComPtr<ID3D12Resource>& indexBuffer,
            D3D12_INDEX_BUFFER_VIEW& indexBufferView);

        bool UploadTexture(DXTexture& texture, const std::vector<D3D12_SUBRESOURCE_DATA>& subresources);

        bool SetupPipeline(
            std::filesystem::path vsPath,
            std::filesystem::path psPath);

        void BindObject(const Matrix4x4_SIMD& worldMatrix, size_t objectIndex);
        void BindMaterial(const Material& material);
        bool CreateMesh(const Mesh& mesh, DXMesh& result);
        void BindMaterialTexture(int slot, const DXTexture& texture);
        void CommitMaterialTexture();
        void ResetMaterialTextureBinding();

        ID3D12GraphicsCommandList* GetCommandList()
        {
            return m_FrameContexts[m_frameIndex].commandList.Get();
        }

        ID3D12Device* GetDevice()
        {
            return m_Device.Get();
        }
    };
}

#endif // _DX12_BACKEND_H_
