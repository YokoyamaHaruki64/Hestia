/*=============================================================================

 File   : DX12Object.h
 Desc   : DX12Backend で使用するオブジェクトを定義

------------------------------------------------------------------------------

 Date   : 2026/10/08
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _DX12_OBJECT_H_
#define _DX12_OBJECT_H_

#include "Common/Include/WindowsHeaders.h"

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include <vector>
#include <array>
#include <cstdint>
#include <cstddef>

#include "Asset/AssetType.h"
#include "HMath.h"

using Microsoft::WRL::ComPtr;

namespace Hestia
{
    class Material;

    static constexpr uint32_t MATERIAL_CONSTANT_BUFFER_REGISTER = 2;
    static constexpr uint32_t MATERIAL_TEXTURE_REGISTER_SPACE = 1;
    static constexpr int MAX_MATERIAL_TEXTURE = 16;

    struct DXRenderTexture
    {
        ComPtr<ID3D12Resource> resource;
        int rtvHeapIndex = -1; // RTVヒープのインデックス
        uint32_t srvHeapIndex = UINT32_MAX; // SRVヒープのインデックス
    };

    struct DXDepthStencil
    {
        ComPtr<ID3D12Resource> resource;
        int dsvHeapIndex = -1; // DSVヒープのインデックス
        uint32_t depthSrvHeapIndex = UINT32_MAX; // DepthのSRVヒープのインデックス
        uint32_t stencilSrvHeapIndex = UINT32_MAX; // StencilのSRVヒープのインデックス
    };

    struct DXTexture
    {
        ComPtr<ID3D12Resource> m_resource;
        uint32_t m_masterSRVIndex = UINT32_MAX;
    };

    struct PerFrameConstants
    {
        Matrix4x4_SIMD viewMatrix;
        Matrix4x4_SIMD projectionMatrix;
        Vector3 cameraPosition;
        float time; // 経過時間
    };

    struct PerObjectConstants
    {
        Matrix4x4_SIMD worldMatrix;
    };

    struct DXVertexBuffer
    {
        ComPtr<ID3D12Resource> m_resource;
        D3D12_VERTEX_BUFFER_VIEW m_view{};
    };

    struct DXMesh
    {
        std::array<DXVertexBuffer, static_cast<size_t>(VertexStream::Count)> m_streams;
        ComPtr<ID3D12Resource> m_indexBuffer;
        D3D12_INDEX_BUFFER_VIEW m_indexView{};
        uint32_t m_vertexCount = 0;
        std::vector<SubMesh> m_subMeshes;
    };

    struct RenderData
    {
        Matrix4x4_SIMD worldMatrix = Matrix4x4_SIMD::Identity();
        AssetHandle<Mesh> mesh;

        /// @brief SubMesh の MaterialSlot 順に並べた Material のハンドル
        /// @note 無効な Slot または描画時に解決できない Material の SubMesh は描画しない
        std::vector<AssetHandle<Material>> materials;
    };
}

#endif // _DX12_OBJECT_H_
