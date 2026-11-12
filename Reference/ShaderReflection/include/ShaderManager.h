/*=============================================================================

File   : [ShaderManager.h]
Desc   : シェーダーの管理を行うクラスの宣言

------------------------------------------------------------------------------

Date   : 2026/03/13
Author : Yokoyama Haruki

===============================================================================*/

#ifndef _SHADER_MANAGER_H_
#define _SHADER_MANAGER_H_

#include "Graphics/GraphicInc.h"

#include <memory>
#include <string>
#include <vector>

class GraphicsShader;
class ShaderLoader;

class ShaderManager
{
private:
    static constexpr const char* DEFAULT_SHADER_FILE_PATH = "Assets/Shaders/DefaultLit.shader";
    static std::vector<std::unique_ptr<GraphicsShader>> m_shaders;
    static std::unique_ptr<ShaderLoader> m_shaderLoader;
    static GraphicsShader* m_defaultShader;
    static ShaderID m_shaderIDs;

public:
    static bool Init();
    static void Uninit();

    static GraphicsShader* LoadShaderFromFile(const std::string& filePath);
    static GraphicsShader* GetDefaultShader() { return m_defaultShader; }
    static GraphicsShader* GetShaderByFilePath(const std::string& filePath);
    static GraphicsShader* GetShaderByID(ShaderID id);
    static void RemoveShaderByFilePath(const std::string& filePath);
};


#endif // _SHADER_MANAGER_H_
