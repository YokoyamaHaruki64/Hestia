/*=============================================================================

File   : [InputLayoutBuilder.cpp]
Desc   : 入力レイアウトの構築を行うクラスの実装

------------------------------------------------------------------------------

Date   : 2026/04/10
Author : Yokoyama Haruki

===============================================================================*/

#include "System/Rendering/renderer.h"
#include "System/Diagnostics/Logger.h"

#include "Graphics\Shader\InputLayoutBuilder.h"

#include <d3dcompiler.h>

#pragma comment(lib, "d3dcompiler.lib")

bool InputLayoutBuilder::CreateInputLayout(
    ID3DBlob* shaderBlob,
    std::vector<InputSlotBinding>& outInputSlot,
    ID3D11InputLayout** outInputLayout)
{
    ComPtr<ID3D11ShaderReflection> reflection;

    HRESULT hr = D3DReflect(
        shaderBlob->GetBufferPointer(),
        shaderBlob->GetBufferSize(),
        IID_PPV_ARGS(reflection.GetAddressOf())
    );

    if (FAILED(hr))
    {
        Logger::Error("入力レイアウト用のシェーダーリフレクションに失敗しました。");
        return false;
    }

    D3D11_SHADER_DESC desc;
    hr = reflection->GetDesc(&desc);
    if (FAILED(hr))
    {
        Logger::Error("入力レイアウト用のシェーダー情報取得に失敗しました。");
        return false;
    }

    std::vector<D3D11_INPUT_ELEMENT_DESC> layoutDesc;

    // 頂点シェーダーの入力パラメーターから入力レイアウトを作成する
    for (UINT i = 0; i < desc.InputParameters; i++)
    {
        // 入力パラメーターの情報を取得する
        D3D11_SIGNATURE_PARAMETER_DESC param;
        reflection->GetInputParameterDesc(i, &param);

        // 入力レイアウトの情報を保存する
        InputSlotBinding binding;
        binding.streamType = getVertexAttribute(param);
        if (binding.streamType == VertexStreamType::InstanceID)
        {
            // システム値はGPUが自動供給するため、InputLayout／頂点バッファへ登録しない。
            continue;
        }
        if( binding.streamType == VertexStreamType::Error )
        {
            Logger::Error("シェーダー入力シグネチャに未対応の頂点属性があります。");
            return false;
        }
        // 同じ頂点ストリームに含まれる属性（Normal/Tangent、UV2-7）は同じslotを共有する。
        binding.inputSlot = static_cast<UINT>(binding.streamType);
        outInputSlot.push_back(binding);

        // 入力レイアウトの要素を作成する
        D3D11_INPUT_ELEMENT_DESC elem = {};
        elem.SemanticName = param.SemanticName;
        elem.SemanticIndex = param.SemanticIndex;
        elem.Format = getFormat(param);
        elem.InputSlot = binding.inputSlot;
        elem.AlignedByteOffset = GetVertexAttributeOffset(param, binding.streamType);
        elem.InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;

        layoutDesc.push_back(elem);



    }

    // 入力レイアウトを作成する
    hr = Renderer::GetDevice()->CreateInputLayout(
        layoutDesc.data(),
        static_cast<UINT>(layoutDesc.size()),
        shaderBlob->GetBufferPointer(),
        shaderBlob->GetBufferSize(),
        outInputLayout
    );

    if (FAILED(hr))
    {
        Logger::Error("入力レイアウトの生成に失敗しました。");
        return false;
    }

    return true;
}

DXGI_FORMAT InputLayoutBuilder::getFormat(const D3D11_SIGNATURE_PARAMETER_DESC& param)
{
    if( param.ComponentType == D3D_REGISTER_COMPONENT_UINT32 )
    {
            if (param.Mask == 0x1) return DXGI_FORMAT_R32_UINT;
            if (param.Mask == 0x3) return DXGI_FORMAT_R32G32_UINT;
            if (param.Mask == 0x7) return DXGI_FORMAT_R32G32B32_UINT;
            if (param.Mask == 0xF) return DXGI_FORMAT_R32G32B32A32_UINT;
    }
    else if( param.ComponentType == D3D_REGISTER_COMPONENT_SINT32 )
    {
            if (param.Mask == 0x1) return DXGI_FORMAT_R32_SINT;
            if (param.Mask == 0x3) return DXGI_FORMAT_R32G32_SINT;
            if (param.Mask == 0x7) return DXGI_FORMAT_R32G32B32_SINT;
            if (param.Mask == 0xF) return DXGI_FORMAT_R32G32B32A32_SINT;
    }
    else if (param.ComponentType == D3D_REGISTER_COMPONENT_FLOAT32)
    {
        if (param.Mask == 0x1) return DXGI_FORMAT_R32_FLOAT;
        if (param.Mask == 0x3) return DXGI_FORMAT_R32G32_FLOAT;
        if (param.Mask == 0x7) return DXGI_FORMAT_R32G32B32_FLOAT;
        if (param.Mask == 0xF) return DXGI_FORMAT_R32G32B32A32_FLOAT;

    }
    return DXGI_FORMAT_UNKNOWN;
}

VertexStreamType InputLayoutBuilder::getVertexAttribute(const D3D11_SIGNATURE_PARAMETER_DESC& param)
{
    if (param.SystemValueType == D3D_NAME_INSTANCE_ID)
        return VertexStreamType::InstanceID;

    if ( strcmp(param.SemanticName, "POSITION") == 0)
        return VertexStreamType::Position;

    if (strcmp(param.SemanticName, "NORMAL") == 0)
        return VertexStreamType::NormalTangent;

    if (strcmp(param.SemanticName, "TANGENT") == 0)
        return VertexStreamType::NormalTangent;

    if (strcmp(param.SemanticName, "TEXCOORD") == 0)
    {
        switch (param.SemanticIndex)
        {
        case 0:
            return VertexStreamType::PrimaryTexCoord;
        case 1:
            return VertexStreamType::SecondaryTexCoord;
        default:
            if( param.SemanticIndex >= 2 && param.SemanticIndex <= 7 )
                return VertexStreamType::ExtraTexCoord;
            else
                return VertexStreamType::Error;
        }
    }

    if (strcmp(param.SemanticName, "COLOR") == 0)
        return VertexStreamType::Color;

    return VertexStreamType::Error;
}

UINT InputLayoutBuilder::GetVertexAttributeOffset(
    const D3D11_SIGNATURE_PARAMETER_DESC& param,
    VertexStreamType streamType)
{
    if (strcmp(param.SemanticName, "TANGENT") == 0)
        return sizeof(Vector3);

    if (streamType == VertexStreamType::ExtraTexCoord)
        return sizeof(Vector4) * (param.SemanticIndex - 2);

    return 0;
}
