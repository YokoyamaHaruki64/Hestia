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
#include <d3dcompiler.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include <string>
#include <string_view>
#include <vector>
#include <array>
#include <cstdint>
#include <cstddef>

#include "Asset/AssetType.h"
#include "HMath.h"
#include "Color.h"

using Microsoft::WRL::ComPtr;

namespace Hestia
{
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

    struct Material
    {
        std::string name;

        Color diffuseColor = Color::White();
        Color specularColor = Color::White();
        Color emissionColor = Color::Black();
        float roughness = 0.5f;
        float metallic = 0.0f;
        std::vector<AssetHandle<Texture>> textures;

        static Material Default()
        {
            Material mat;
            mat.name = "Default";
            mat.diffuseColor = Color::White();
            mat.specularColor = Color::White();
            mat.emissionColor = Color::Black();
            mat.roughness = 0.5f;
            mat.metallic = 0.0f;
            return mat;
        }
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
        uint32_t m_indexCount = 0;
    };

    struct MaterialConstants
    {
        Color diffuseColor = Color::White();
        Color specularColor = Color::White();
        Color emissionColor = Color::Black();
        float roughness = 0.5f;
        float metallic = 0.0f;
    };

    struct RenderData
    {
        Matrix4x4_SIMD worldMatrix = Matrix4x4_SIMD::Identity();
        Material material;
        AssetHandle<Mesh> mesh;

        std::wstring_view vsShaderPath;
        std::wstring_view psShaderPath;
    };
}

#endif // _DX12_OBJECT_H_
