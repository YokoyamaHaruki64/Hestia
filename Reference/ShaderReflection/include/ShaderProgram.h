/*=============================================================================
File   : [ShaderProgram.h]
Desc   : シェーダーの管理を行うクラスの宣言

------------------------------------------------------------------------------

Date   : 2026/03/12
Author : Yokoyama Haruki

===============================================================================*/

#ifndef _SHADER_PROGRAM_H_
#define _SHADER_PROGRAM_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>
#include <d3d11.h>
#include <wrl/client.h>
#include "Graphics/Shader/ShaderInc.h"
#include "Graphics/GraphicInc.h"

using Microsoft::WRL::ComPtr;


/// @brief 描画用シェーダークラス
/// シェーダーのリソースと情報を持つクラス
class GraphicsShaderPass
{
private:
    std::string m_tag{};
    ComPtr<ID3D11VertexShader> m_vertexShader{ nullptr };
    ComPtr<ID3D11PixelShader> m_pixelShader{ nullptr };
    ComPtr<ID3D11InputLayout> m_inputLayout{ nullptr };

    RenderState m_renderState{};
    bool m_isInstanced{};

    std::vector<InputSlotBinding> m_bindings;
    std::vector<ConstantBufferInfo> m_constantBufferInfo{};
    std::vector<ShaderTextureInfo> m_textureInfo{};

public:

    GraphicsShaderPass() = default;

    GraphicsShaderPass(
        ComPtr<ID3D11VertexShader> vertexShader,
        ComPtr<ID3D11PixelShader> pixelShader,
        ComPtr<ID3D11InputLayout> inputLayout,
        RenderState renderState,
        bool isInstanced,
        std::vector<InputSlotBinding> bindings,
        std::vector<ConstantBufferInfo> constantBufferInfo,
        std::vector<ShaderTextureInfo> textureInfo
    )
        :
        m_vertexShader(std::move(vertexShader)),
        m_pixelShader(std::move(pixelShader)),
        m_inputLayout(std::move(inputLayout)),
        m_renderState(renderState),
        m_isInstanced(isInstanced),
        m_bindings(std::move(bindings)),
        m_constantBufferInfo(std::move(constantBufferInfo)),
        m_textureInfo(std::move(textureInfo))
    {
    }

    // このシェーダーが正当なものであるかをチェックする関数
    bool IsValid() const
    {
        return m_vertexShader != nullptr && m_pixelShader != nullptr;
    }

    void Bind() const;
    void SetTag(std::string tag) { m_tag = std::move(tag); }

    const std::string& GetTag() const { return m_tag; }
    const RenderState& GetRenderState() const { return m_renderState; }
    bool IsInstanced() const { return m_isInstanced; }
    const std::vector<InputSlotBinding>& GetInputSlotBindings() const { return m_bindings; }
    const std::vector<ConstantBufferInfo>& GetConstantBufferInfo() const { return m_constantBufferInfo; }
    const std::vector<ShaderTextureInfo>& GetTextureInfo() const { return m_textureInfo; }
};

class GraphicsShader
{
    ShaderID m_id{ 0 };

    std::string m_shaderName{};
    std::string m_filePath{};

    std::vector<GraphicsShaderPass> m_shaderPasses{};
    std::unordered_map<std::string, size_t> m_passIndicesByTag{};
    std::vector<ShaderPropertyDesc> m_properties{};
    std::unordered_map<std::string, ShaderPropertyID> m_propertyIDsByName{};

public:
    GraphicsShader(
        ShaderID id,
        const std::string& shaderName,
        const std::string& filePath,
        std::vector<GraphicsShaderPass> shaderPasses,
        std::vector<ShaderPropertyDesc> properties)
        :
        m_id(id),
        m_shaderName(shaderName),
        m_filePath(filePath),
        m_shaderPasses(std::move(shaderPasses)),
        m_properties(std::move(properties))
    {
        for (const ShaderPropertyDesc& property : m_properties)
            m_propertyIDsByName.emplace(property.name, property.id);

        for (size_t index = 0; index < m_shaderPasses.size(); ++index)
        {
            const std::string& tag = m_shaderPasses[index].GetTag();
            if (!tag.empty())
                m_passIndicesByTag.emplace(tag, index);
        }
    }
    const ShaderID& GetID() const { return m_id; }
    const std::string& GetShaderName() const { return m_shaderName; }
    const std::string& GetFilePath() const { return m_filePath; }
    const std::vector<GraphicsShaderPass>& GetShaderPasses() const { return m_shaderPasses; }
    const GraphicsShaderPass* GetShaderPass(std::string_view tag) const;
    std::optional<size_t> GetShaderPassIndex(std::string_view tag) const;
    const std::vector<ShaderPropertyDesc>& GetProperties() const { return m_properties; }
    const ShaderPropertyDesc* GetProperty(ShaderPropertyID id) const;
    const ShaderPropertyDesc* GetProperty(std::string_view name) const;

    bool IsValid() const
    {
        return !m_shaderPasses.empty();
    }
};

#endif // _SHADER_PROGRAM_H_
