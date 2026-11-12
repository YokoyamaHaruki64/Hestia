/*=============================================================================

 File   : [ShaderFactory.h]
 Desc   : シェーダーの生成を行うクラスの宣言

 ------------------------------------------------------------------------------

 Date   : 2026/04/10
 Author : Yokoyama Haruki

===============================================================================*/

#ifndef _SHADER_FACTORY_H_
#define _SHADER_FACTORY_H_

#include "Graphics/Shader/ShaderCompiler.h"
#include "Graphics/Shader/InputLayoutBuilder.h"
#include "Graphics/Shader/ShaderReflector.h"
#include "Graphics/Shader/ShaderInc.h"

class GraphicsShaderPass;

class ShaderFactory
{
private:
    ShaderCompiler m_shaderCompiler;
    InputLayoutBuilder m_inputLayoutBuilder;

    bool createVertexShader(
        ID3DBlob* shaderBlob,
        ID3D11VertexShader** outVertexShader,
        ID3D11InputLayout** outInputLayout,
        std::vector<InputSlotBinding>& outInputSlot);

    bool createPixelShader(
        ID3DBlob* shaderBlob,
        ID3D11PixelShader** outPixelShader);

    bool compileVertexShader(
        const std::string& hlslCode,
        const std::string& sourceName,
        const std::string& entryPoint,
        const std::string& shaderModel,
        ID3DBlob** outVertexShaderBlob);

    bool compilePixelShader(
        const std::string& hlslCode,
        const std::string& sourceName,
        const std::string& entryPoint,
        const std::string& shaderModel,
        ID3DBlob** outPixelShaderBlob);

    void mergeShaderReflectionResults(
        const ShaderReflector::ReflectionResult& vertexReflectionResult,
        const ShaderReflector::ReflectionResult& pixelReflectionResult,
        std::vector<ConstantBufferInfo>& outConstantBufferInfo,
        std::vector<ShaderTextureInfo>& outTextureInfo);

public:

    ShaderFactory() = default;

    bool CreateGraphicsShaderPass(
        const std::string& hlslCode,
        const std::string& sourceName,
        const std::string& vertexEntryPoint,
        const std::string& vertexShaderModel,
        const std::string& pixelEntryPoint,
        const std::string& pixelShaderModel,
        const RenderState& renderState,
        bool isInstanced,
        GraphicsShaderPass& outShaderPass);


};


#endif // _SHADER_FACTORY_H_
