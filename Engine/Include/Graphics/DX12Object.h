/*=============================================================================

 File   : DX12Object.h
 Desc   : DX12Backend で使用するオブジェクトを定義

------------------------------------------------------------------------------

 Date   : 2026/10/08
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _DX12_OBJECT_H_
#define _DX12_OBJECT_H_

#include <d3d12.h>
#include <d3dcompiler.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include <string>
#include <vector>
#include <cstdint>
#include <cstddef>

#include "HMath.h"
#include "Color.h"

using Microsoft::WRL::ComPtr;

namespace Hestia
{
    using TextureHandle = int;

    struct DXRenderTexture
    {
        ComPtr<ID3D12Resource> resource;
        int rtvHeapIndex = -1; // RTVヒープのインデックス
        int srvHeapIndex = -1; // SRVヒープのインデックス
    };

    struct DXDepthStencil
    {
        ComPtr<ID3D12Resource> resource;
        int dsvHeapIndex = -1;			// DSVヒープのインデックス
        int depthSrvHeapIndex = -1;		// DepthのSRVヒープのインデックス
        int stencilSrvHeapIndex = -1;	// StencilのSRVヒープのインデックス
    };

    struct DXTexture
    {
        ComPtr<ID3D12Resource> resource;
        int srvHeapIndex = -1; // SRVヒープのインデックス
    };

    struct Vertex
    {
        Vector3 position;
        Vector2 uv;
        Vector3 normal;
        Vector3 tangent;
        Color color;
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
        std::vector<TextureHandle> textures;

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

    // Mesh構造体
    // 仮構成
    struct Mesh
    {
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
    };

    struct DXMesh
    {
        ComPtr<ID3D12Resource> vertexBuffer;
        D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
        ComPtr<ID3D12Resource> indexBuffer;
        D3D12_INDEX_BUFFER_VIEW indexBufferView{};
        uint32_t indexCount = 0;
    };

    struct MaterialConstants
    {
        Color diffuseColor = Color::White();
        Color specularColor = Color::White();
        Color emissionColor = Color::Black();
        float roughness = 0.5f;
        float metallic = 0.0f;
    };

    // レンダリングに必要なデータをまとめた構造体
    // 仮構成、最終的にMaterialやMeshはIDになる
    struct RenderData
    {
        Matrix4x4_SIMD worldMatrix = Matrix4x4_SIMD::Identity();
        Material material;
        DXMesh mesh;

        std::wstring_view vsShaderPath;
        std::wstring_view psShaderPath;
    };
}
#endif // _DX12_OBJECT_H_
