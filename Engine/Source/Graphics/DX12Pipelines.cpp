/*=============================================================================

 File   : DX12Pipelines.cpp
 Desc   : Root Signature、Input Layout と PSO を管理する。

------------------------------------------------------------------------------

 Date   : 2026/11/07
 Author : Yokoyama Haruki

=============================================================================*/

#include "pch.h"
#include "Graphics/DX12Pipelines.h"

#include "Graphics/DX12Support.h"

#include "D3DX12/d3dx12.h"

using namespace Hestia;

bool DX12Pipelines::Initialize(ID3D12Device* device)
{
    HRESULT hr;
    // すべての MaterialShader Pass で使う CBV と SRV のスロットを定義する。
    const int rootParamCount = 6;
    std::array<D3D12_ROOT_PARAMETER, rootParamCount> rootParams{};
    std::array<D3D12_ROOT_PARAMETER_TYPE, rootParamCount> rootParamTypes
    {
        D3D12_ROOT_PARAMETER_TYPE_CBV,	// PerFrame Constant Buffer
        D3D12_ROOT_PARAMETER_TYPE_CBV,	// PerObject Constant Buffer
        D3D12_ROOT_PARAMETER_TYPE_CBV,	// PerMaterial Constant Buffer
        D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,	// System Texture SRV
        D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,	// Material Texture SRV
        D3D12_ROOT_PARAMETER_TYPE_SRV                 // インスタンスデータ
    };

    std::array<D3D12_DESCRIPTOR_RANGE, 2> descriptorRanges
    {
        // System Texture SRV
        D3D12_DESCRIPTOR_RANGE
        {
            D3D12_DESCRIPTOR_RANGE_TYPE_SRV,		// Range Type
            16,										// Descriptor数
            0,										// Registerの開始位置
            0,										// Register Space
            D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND	// Offset (APPEND:自動で割り当て = m_SRVIncementSizeと同一サイズ？)
        },
        // Material Texture SRV
        D3D12_DESCRIPTOR_RANGE
        {
            D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
            MAX_MATERIAL_TEXTURE,
            0,
            MATERIAL_TEXTURE_REGISTER_SPACE,
            D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND
        }
    };

    int tableCount = 0;
    for(int i = 0; i < rootParamCount; ++i)
    {
        rootParams[i].ParameterType = rootParamTypes[i];
        rootParams[i].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        switch (rootParamTypes[i])
        {
        case D3D12_ROOT_PARAMETER_TYPE_CBV:
            rootParams[i].Descriptor.ShaderRegister = i;
            rootParams[i].Descriptor.RegisterSpace = 0;
            break;
        case D3D12_ROOT_PARAMETER_TYPE_SRV:
            rootParams[i].Descriptor.ShaderRegister = 0;
            rootParams[i].Descriptor.RegisterSpace = INSTANCE_BUFFER_REGISTER_SPACE;
            break;
		
        case D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE:
            rootParams[i].DescriptorTable.NumDescriptorRanges = 1;
            rootParams[i].DescriptorTable.pDescriptorRanges = &descriptorRanges[tableCount];
            tableCount++;
            break;

        case D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS:
            rootParams[i].Constants.ShaderRegister = i;
            rootParams[i].Constants.RegisterSpace = 0;
            rootParams[i].Constants.Num32BitValues = 4;
            break;

        default:
            break;
        }
    }

    // Static Samplerの設定
    std::array<D3D12_STATIC_SAMPLER_DESC, 4> staticSamplers{};
    auto CreateSampler = [](D3D12_FILTER filter, D3D12_TEXTURE_ADDRESS_MODE addressMode, UINT shaderRegister)->D3D12_STATIC_SAMPLER_DESC
    {
        D3D12_STATIC_SAMPLER_DESC sampler{};
        sampler.Filter = filter;
        sampler.AddressU = addressMode;
        sampler.AddressV = addressMode;
        sampler.AddressW = addressMode;
        sampler.MipLODBias = 0.0f;
        sampler.MaxAnisotropy = 1;
        sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
        sampler.MinLOD = 0.0f;
        sampler.MaxLOD = D3D12_FLOAT32_MAX;
        sampler.ShaderRegister = shaderRegister;
        return sampler;
    };

    staticSamplers[0] = CreateSampler(
        D3D12_FILTER_MIN_MAG_MIP_LINEAR,
        D3D12_TEXTURE_ADDRESS_MODE_WRAP, 0);
    staticSamplers[1] = CreateSampler(
        D3D12_FILTER_MIN_MAG_MIP_LINEAR,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP, 1);
    staticSamplers[2] = CreateSampler(
        D3D12_FILTER_MIN_MAG_MIP_POINT,
        D3D12_TEXTURE_ADDRESS_MODE_WRAP, 2);
    staticSamplers[3] = CreateSampler(
        D3D12_FILTER_MIN_MAG_MIP_POINT,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP, 3);

    // RootSignatureの作成
    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
    rootSignatureDesc.NumParameters = rootParamCount;
    rootSignatureDesc.pParameters = rootParams.data();
    rootSignatureDesc.NumStaticSamplers = static_cast<UINT>(staticSamplers.size());
    rootSignatureDesc.pStaticSamplers = staticSamplers.data();
    rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> signatureBlob;
    ComPtr<ID3DBlob> errorBlob;
    hr = D3D12SerializeRootSignature(
        &rootSignatureDesc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        signatureBlob.GetAddressOf(),
        errorBlob.GetAddressOf());

    if(!CheckDX12Result(hr, L"Failed to serialize root signature."))
    {
        if (errorBlob)
        {
            OutputDebugStringA(static_cast<const char*>(errorBlob->GetBufferPointer()));
        }
        return false;
    }

    hr = device->CreateRootSignature(
        0,
        signatureBlob->GetBufferPointer(),
        signatureBlob->GetBufferSize(),
        IID_PPV_ARGS(m_RootSignature.GetAddressOf()));
    if (!CheckDX12Result(hr, L"Failed to create root signature.")) return false;


    // InputLayoutの設定
    static constexpr D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    1, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TANGENT",  0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, static_cast<UINT>(offsetof(NormalTangentVertex, m_tangent)), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       2, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 3, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };
    m_InputLayoutDesc.NumElements = _countof(inputElementDescs);
    m_InputLayoutDesc.pInputElementDescs = inputElementDescs;

    return true;
}

void DX12Pipelines::Finalize()
{
    m_materialPipelines.clear();
    m_RootSignature.Reset();
    m_InputLayoutDesc = {};
}
