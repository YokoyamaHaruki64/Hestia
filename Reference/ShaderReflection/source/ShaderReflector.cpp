/*=============================================================================

File   : [ShaderReflector.cpp]
Desc   : シェーダーのリフレクションを行うクラスの実装
        シェーダーから定数バッファやテクスチャ、サンプラーなどの情報を取得するためのクラス

------------------------------------------------------------------------------

Date   : 2026/04/10
Author : Yokoyama Haruki

===============================================================================*/

#include "Graphics/Shader/ShaderReflector.h"
#include "Graphics/Shader/ShaderInc.h"
#include "System/Diagnostics/Logger.h"

#include <d3dcompiler.h>

#define VERTEX_SLOT(bindPoint) ((m_shaderStage == ShaderStage::Vertex) ? bindPoint : -1)
#define PIXEL_SLOT(bindPoint) ((m_shaderStage == ShaderStage::Pixel) ? bindPoint : -1)

bool ShaderReflector::ReflectShader(ReflectionResult& outResult)
{
    // リフレクションの生成
    HRESULT hr = D3DReflect(
        m_shaderBlob->GetBufferPointer(),
        m_shaderBlob->GetBufferSize(),
        IID_PPV_ARGS(m_reflection.GetAddressOf())
    );

    if (FAILED(hr))
    {
        Logger::Error("シェーダーリフレクションの生成に失敗しました。");
        return false;
    }

    // シェーダーの情報を取得する
    hr = m_reflection->GetDesc(&m_shaderDesc);
    if (FAILED(hr))
    {
        Logger::Error("シェーダーリフレクション情報の取得に失敗しました。");
        return false;
    }

    // シェーダーの大まかな情報を取得する
    // 定数バッファ、テクスチャ、サンプラーのスロット,名前,ステージを取得する
    getShaderInfo(
        outResult.constantBuffers,
        outResult.textures);

    // 定数バッファの詳細情報を取得する
    getConstantBufferInfo(outResult.constantBuffers);

    return true;
}


void ShaderReflector::getShaderInfo(
    std::vector<ConstantBufferInfo>& outConstantBuffers,
    std::vector<ShaderTextureInfo>& outTextures)
{
    // スロット情報を取得する
    for (UINT i = 0; i < m_shaderDesc.BoundResources; i++)
    {
        D3D11_SHADER_INPUT_BIND_DESC bind;
        m_reflection->GetResourceBindingDesc(i, &bind);

        switch (bind.Type)
        {
        case D3D_SIT_CBUFFER:
        {
            ConstantBufferInfo cbInfo;
            cbInfo.name = bind.Name;
            cbInfo.vsSlot = VERTEX_SLOT(bind.BindPoint);
            cbInfo.psSlot = PIXEL_SLOT(bind.BindPoint);
            outConstantBuffers.push_back(cbInfo);
        }
        break;

        case D3D_SIT_TEXTURE:
        {
            ShaderTextureInfo texInfo;
            texInfo.name = bind.Name;
            texInfo.vsSlot = VERTEX_SLOT(bind.BindPoint);
            texInfo.psSlot = PIXEL_SLOT(bind.BindPoint);
            outTextures.push_back(texInfo);
        }
        break;

        default:
            break;
        }
    }
}

void ShaderReflector::getConstantBufferInfo(
    std::vector<ConstantBufferInfo>& outConstantBuffers)
{
    // 定数バッファの詳細情報を取得する
    for (UINT i = 0; i < m_shaderDesc.ConstantBuffers; i++)
    {
        ID3D11ShaderReflectionConstantBuffer* cb = m_reflection->GetConstantBufferByIndex(i);
        D3D11_SHADER_BUFFER_DESC cbDesc;
        cb->GetDesc(&cbDesc);

        // 定数バッファのスロットはリソースバインディング情報から取得する必要があるため、先に取得したスロット情報と照合してサイズを設定する
        for (auto& cbInfo : outConstantBuffers)
        {
            if (cbInfo.name == cbDesc.Name)
            {
                cbInfo.size = cbDesc.Size;
                getConstantBufferParamInfo(cb, cbDesc, cbInfo);
                break;
            }
        }

    }

}

void ShaderReflector::getConstantBufferParamInfo(
    ID3D11ShaderReflectionConstantBuffer* constantBufferReflection,
    D3D11_SHADER_BUFFER_DESC cbDesc,
    ConstantBufferInfo& outConstantBufferInfo)
{
    // 定数バッファ内のパラメーター情報を取得する
    for (UINT j = 0; j < cbDesc.Variables; j++)
    {
        ID3D11ShaderReflectionVariable* var
            = constantBufferReflection->GetVariableByIndex(j);
        D3D11_SHADER_VARIABLE_DESC varDesc;

        var->GetDesc(&varDesc);
        ShaderParameterDesc param;
        param.name = varDesc.Name;
        param.size = varDesc.Size;
        param.offset = varDesc.StartOffset;

        outConstantBufferInfo.parameters.push_back(param);
    }
}
