/*=============================================================================

 File   : DX12Resources.h
 Desc   : GPU Asset、代替バッファと転送の寿命を管理する。

------------------------------------------------------------------------------

 Date   : 2026/11/07
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _DX12_RESOURCES_H_
#define _DX12_RESOURCES_H_

#include "Graphics/DX12Object.h"
#include "Graphics/GPUResourceStorage.h"
#include <array>
#include <queue>
#include <vector>

namespace Hestia
{
    class DX12Device;
    class DX12Descriptors;

    /// @brief CPU Asset を GPU へ登録し、転送中の資源をフェンス完了まで保持する。
    class DX12Resources
    {
    public:
        bool Initialize(ID3D12Device* device);
        void Update(const DX12Device& device);
        void Finalize();
        bool CreateWhiteTexture(DX12Device& device, DX12Descriptors& descriptors);
        bool UploadMesh(DX12Device& device, AssetHandle<Mesh> handle, const Mesh& mesh);
        bool UploadTexture(DX12Device& device, DX12Descriptors& descriptors, AssetHandle<Texture> handle,
            const TextureDesc& desc, const void* pixels, size_t rowPitch, size_t slicePitch);
        const DXMesh* GetMesh(AssetHandle<Mesh> handle) const { return m_gpuMeshes.Get(handle); }
        const DXTexture* GetTexture(AssetHandle<Texture> handle) const { return m_gpuTextures.Get(handle); }
        const DXTexture& WhiteTexture() const { return m_whiteTexture; }
        const DXVertexBuffer& GetVertexBuffer(const DXMesh& mesh, VertexStream stream) const;

    private:
        struct WaitingResource
        {
            ComPtr<ID3D12Resource> resource;
            uint64_t fenceValue;
        };

        // Mesh と Texture のアップロードで共有する記録資源。
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
            bool Initialize(ComPtr<ID3D12Device> device);
            void Finalize();
            // 完了した転送の Allocator と CommandList だけを再利用する。
            void ReleaseContext(UINT64 completedFenceValue);
            bool GetFreeIndex(int& outIndex);
            ComPtr<ID3D12GraphicsCommandList> GetResetCommandList(int index, UINT64 fenceValue);
        };

        DXTexture m_whiteTexture; // Material の Texture が未設定の場合に使う白 Texture
        GPUResourceStorage<Mesh, DXMesh> m_gpuMeshes;
        GPUResourceStorage<Texture, DXTexture> m_gpuTextures;
        std::array<DXVertexBuffer, static_cast<size_t>(VertexStream::Count)> m_defaultStreamBuffers;
        std::array<size_t, static_cast<size_t>(VertexStream::Count)> m_defaultStreamCapacities{};
        UploadContextPool m_UploadContextPool;
        std::queue<WaitingResource> m_WaitingResources;
        std::vector<DXTexture> m_SystemTextures;

        bool CreateMesh(DX12Device& device, const Mesh& mesh, DXMesh& result);
        bool EnsureDefaultStreamBuffers(DX12Device& device, size_t vertexCount);
        bool CreateVertexStreamBuffer(
            DX12Device& device, 
            VertexStream stream,
            const void* data,
            size_t vertexCount,
            DXVertexBuffer& result);
        bool CreateIndexBuffer(
            DX12Device& device, 
            const uint32_t* indices,
            size_t indexCount,
            ComPtr<ID3D12Resource>& indexBuffer,
            D3D12_INDEX_BUFFER_VIEW& indexBufferView);
        bool UploadBufferToDefault(
            DX12Device& device, 
            const void* data,
            size_t byteSize,
            D3D12_RESOURCE_STATES finalState,
            ComPtr<ID3D12Resource>& resource);
        void RetireResource(const DX12Device& device, ComPtr<ID3D12Resource> resource);
        bool UploadTextureResource(
            DX12Device& device, DX12Descriptors& descriptors, 
            DXTexture& texture,
            const TextureDesc& desc,
            const void* pixels,
            size_t rowPitch,
            size_t slicePitch);
    };
}

#endif // _DX12_RESOURCES_H_
