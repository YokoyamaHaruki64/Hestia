/*=============================================================================

File   : [ShaderFactory.cpp]
Desc   : シェーダーの生成を行うクラスの実装

------------------------------------------------------------------------------

Date   : 2026/04/10
Author : Yokoyama Haruki

===============================================================================*/

#include "Graphics/Shader/ShaderFactory.h"
#include "Graphics/Shader/ShaderCompiler.h"
#include "Graphics/Shader/ShaderReflector.h"
#include "Graphics/Shader/InputLayoutBuilder.h"
#include "Graphics/Shader/ShaderInc.h"
#include "Graphics/Shader/ShaderProgram.h"
#include "System/Rendering/renderer.h"
#include "System/Diagnostics/Logger.h"

bool ShaderFactory::createVertexShader(
    ID3DBlob* shaderBlob,
    ID3D11VertexShader** outVertexShader,
    ID3D11InputLayout** outInputLayout,
    std::vector<InputSlotBinding>& outInputSlot)
{
    HRESULT hr = Renderer::GetDevice()->CreateVertexShader(
        shaderBlob->GetBufferPointer(),
        shaderBlob->GetBufferSize(),
        nullptr,
        outVertexShader
    );

    if (FAILED(hr))
    {
        Logger::Error("頂点シェーダーの生成に失敗しました。");
        return false;
    }

    std::vector<InputSlotBinding> inputSlot;
    bool success = m_inputLayoutBuilder.CreateInputLayout(
        shaderBlob,
        inputSlot,
        outInputLayout
    );

    if (!success)
    {
        return false;
    }

    outInputSlot = std::move(inputSlot);

    return true;
}

bool ShaderFactory::createPixelShader(
    ID3DBlob* shaderBlob,
    ID3D11PixelShader** outPixelShader)
{
    HRESULT hr = Renderer::GetDevice()->CreatePixelShader(
        shaderBlob->GetBufferPointer(),
        shaderBlob->GetBufferSize(),
        nullptr,
        outPixelShader
    );

    if (FAILED(hr))
    {
        Logger::Error("ピクセルシェーダーの生成に失敗しました。");
        return false;
    }

    return true;
}

bool ShaderFactory::compileVertexShader(
    const std::string& hlslCode,
    const std::string& sourceName,
    const std::string& entryPoint,
    const std::string& shaderModel,
    ID3DBlob** outVertexShaderBlob)
{

    return (m_shaderCompiler.CompileShader(
        hlslCode,
        sourceName,
        entryPoint,
        shaderModel,
        outVertexShaderBlob
    ));
}

bool ShaderFactory::compilePixelShader(
    const std::string& hlslCode,
    const std::string& sourceName,
    const std::string& entryPoint,
    const std::string& shaderModel,
    ID3DBlob** outPixelShaderBlob)
{
    return (m_shaderCompiler.CompileShader(
        hlslCode,
        sourceName,
        entryPoint,
        shaderModel,
        outPixelShaderBlob
    ));
}

void ShaderFactory::mergeShaderReflectionResults(
    const ShaderReflector::ReflectionResult& vertexReflectionResult,
    const ShaderReflector::ReflectionResult& pixelReflectionResult,
    std::vector<ConstantBufferInfo>& outConstantBufferInfo,
    std::vector<ShaderTextureInfo>& outTextureInfo)
{
    // 定数バッファのマージ
    outConstantBufferInfo = vertexReflectionResult.constantBuffers;
    for (const auto& cbInfo : pixelReflectionResult.constantBuffers)
    {
        // 名前一致による判定
        auto it = std::find_if(outConstantBufferInfo.begin(), outConstantBufferInfo.end(),
            [&cbInfo](const ConstantBufferInfo& existingCbInfo)
            {
                return existingCbInfo.name == cbInfo.name;
            });

        if (it != outConstantBufferInfo.end())
        {
            // 既に存在する定数バッファがある場合は、スロット情報を更新
            it->vsSlot = (it->vsSlot != -1) ? it->vsSlot : cbInfo.vsSlot;
            it->psSlot = (it->psSlot != -1) ? it->psSlot : cbInfo.psSlot;
        }
        else
        {
            outConstantBufferInfo.push_back(cbInfo);
        }
    }

    // テクスチャのマージ
    outTextureInfo = vertexReflectionResult.textures;
    for (const auto& texInfo : pixelReflectionResult.textures)
    {
        auto it = std::find_if(outTextureInfo.begin(), outTextureInfo.end(),
            [&texInfo](const ShaderTextureInfo& existingTexInfo)
            {
                return existingTexInfo.name == texInfo.name;
            });
        if (it != outTextureInfo.end())
        {
            it->vsSlot = (it->vsSlot != -1) ? it->vsSlot : texInfo.vsSlot;
            it->psSlot = (it->psSlot != -1) ? it->psSlot : texInfo.psSlot;
        }
        else
        {
            outTextureInfo.push_back(texInfo);
        }
    }

}

bool ShaderFactory::CreateGraphicsShaderPass(
    const std::string& hlslCode,
    const std::string& sourceName,
    const std::string& vertexEntryPoint,
    const std::string& vertexShaderModel,
    const std::string& pixelEntryPoint,
    const std::string& pixelShaderModel,
    const RenderState& renderState,
    bool isInstanced,
    GraphicsShaderPass& outShaderPass)
{
    // ===========================
    // シェーダーのコンパイル
    // ===========================

    // 頂点シェーダー
    ComPtr<ID3DBlob> vertexShaderBlob;
    if (!compileVertexShader(
        hlslCode,
        sourceName,
        vertexEntryPoint,
        vertexShaderModel,
        vertexShaderBlob.GetAddressOf()))
    {
        return false;
    }
    // ピクセルシェーダー
    ComPtr<ID3DBlob> pixelShaderBlob;
    if (!compilePixelShader(
        hlslCode,
        sourceName,
        pixelEntryPoint,
        pixelShaderModel,
        pixelShaderBlob.GetAddressOf()))
    {
        return false;
    }

    // ===========================
    // シェーダーオブジェクトの生成
    // ===========================

    // 頂点シェーダーと入力レイアウトの生成
    ComPtr<ID3D11VertexShader> vertexShader;
    ComPtr<ID3D11InputLayout> inputLayout;
    std::vector<InputSlotBinding> inputSlot;
    if (!createVertexShader(
        vertexShaderBlob.Get(),
        vertexShader.GetAddressOf(),
        inputLayout.GetAddressOf(),
        inputSlot))
    {
        return false;
    }

    // ピクセルシェーダーの生成
    ComPtr<ID3D11PixelShader> pixelShader;
    if (!createPixelShader(pixelShaderBlob.Get(), pixelShader.GetAddressOf()))
    {
        return false;
    }

    // ===========================
    // シェーダーのリフレクション
    // ===========================

    // 頂点シェーダーのリフレクション
    ShaderReflector vertexReflector(vertexShaderBlob.Get(), ShaderStage::Vertex);
    ShaderReflector::ReflectionResult vertexReflectionResult;
    if (!vertexReflector.ReflectShader(vertexReflectionResult))
    {
        return false;
    }

    // ピクセルシェーダーのリフレクション
    ShaderReflector pixelReflector(pixelShaderBlob.Get(), ShaderStage::Pixel);
    ShaderReflector::ReflectionResult pixelReflectionResult;
    if (!pixelReflector.ReflectShader(pixelReflectionResult))
    {
        return false;
    }

    // 頂点シェーダーとピクセルシェーダーのリフレクション結果をマージする
    std::vector<ConstantBufferInfo> constantBufferInfo;
    std::vector<ShaderTextureInfo> textureInfo;
    mergeShaderReflectionResults(
        vertexReflectionResult,
        pixelReflectionResult,
        constantBufferInfo,
        textureInfo);

    // ===========================
    // シェーダーパスの生成
    // ===========================
    outShaderPass = GraphicsShaderPass(
        std::move(vertexShader),
        std::move(pixelShader),
        std::move(inputLayout),
        renderState,
        isInstanced,
        std::move(inputSlot),
        std::move(constantBufferInfo),
        std::move(textureInfo)
    );

    return true;
}
