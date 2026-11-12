/*=============================================================================

File   : [ShaderReflector.h]
Desc   : シェーダーのリフレクションを行うクラスの宣言
        シェーダーから定数バッファやテクスチャ、サンプラーなどの情報を取得するためのクラス

------------------------------------------------------------------------------

Date   : 2026/04/10
Author : Yokoyama Haruki

===============================================================================*/

#ifndef _SHADER_REFLECTOR_H_
#define _SHADER_REFLECTOR_H_

#include "Graphics/Shader/ShaderInc.h"

#include <d3d11.h>
#include <d3d11shader.h>
#include <wrl/client.h>
#include <vector>

using Microsoft::WRL::ComPtr;

class ShaderReflector
{
    ID3DBlob* m_shaderBlob{ nullptr };                        // 外部から受け取る
    ComPtr<ID3D11ShaderReflection> m_reflection{ nullptr };    // 内部で作成する
    D3D11_SHADER_DESC m_shaderDesc{};
    ShaderStage m_shaderStage{ ShaderStage::Unknown };

    void getShaderInfo(
        std::vector<ConstantBufferInfo>& outConstantBuffers,
        std::vector<ShaderTextureInfo>& outTextures);

    void getConstantBufferInfo(std::vector<ConstantBufferInfo>& outConstantBuffers);

    void getConstantBufferParamInfo(
        ID3D11ShaderReflectionConstantBuffer* constantBufferReflection,
        D3D11_SHADER_BUFFER_DESC cbDesc,
        ConstantBufferInfo& outConstantBufferInfo);

public:
    struct ReflectionResult
    {
        std::vector<ConstantBufferInfo> constantBuffers{};
        std::vector<ShaderTextureInfo> textures{};
    };

    ShaderReflector(ID3DBlob* shaderBlob, ShaderStage shaderStage)
        : m_shaderBlob(shaderBlob), m_shaderStage(shaderStage)
    {
    }

    ~ShaderReflector() = default;

    bool ReflectShader(ReflectionResult& outResult);
};

#endif // _SHADER_REFLECTOR_H_
