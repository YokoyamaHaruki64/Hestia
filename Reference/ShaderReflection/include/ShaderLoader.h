/*=============================================================================
 File   : [ShaderLoader.h]
 Desc   : シェーダーの読み込みとコンパイルを行うクラスの宣言

 ------------------------------------------------------------------------------

 Date   : 2026/03/12
 Author : Yokoyama Haruki

===============================================================================*/

#ifndef _SHADER_LOADER_H_
#define _SHADER_LOADER_H_

#include <string>
#include <memory>
#include <vector>

#include "Graphics/Shader/ShaderFactory.h"
#include "Graphics/GraphicInc.h"

class GraphicsShader;

class ShaderLoader
{
private:

    ShaderFactory m_shaderFactory;

    bool ResolvePropertyLayout(
        std::vector<ShaderPropertyDesc>& properties,
        const GraphicsShaderPass& shaderPass) const;

public:
    ShaderLoader() = default;

    GraphicsShader* LoadShaderFromFile(const std::string& filePath, ShaderID shaderID);

    ~ShaderLoader() = default;
};

#endif // _SHADER_LOADER_H_
