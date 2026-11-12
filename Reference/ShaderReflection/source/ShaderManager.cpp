#include "pch.h"
#include "Graphics/Shader/ShaderManager.h"
#include "Graphics/Shader/ShaderLoader.h"
#include "Graphics/Shader/ShaderProgram.h"
#include "System/Diagnostics/Logger.h"

std::vector<std::unique_ptr<GraphicsShader>> ShaderManager::m_shaders;
std::unique_ptr<ShaderLoader> ShaderManager::m_shaderLoader;
GraphicsShader* ShaderManager::m_defaultShader{};
ShaderID ShaderManager::m_shaderIDs{};

bool ShaderManager::Init()
{
    // Renderer初期化後にLoaderを生成し、DefaultLitを全Materialの既定Shaderとして一度だけ読み込む。
    m_shaderLoader = std::make_unique<ShaderLoader>();
    m_defaultShader = LoadShaderFromFile(DEFAULT_SHADER_FILE_PATH);

    return m_defaultShader != nullptr;
}

void ShaderManager::Uninit()
{
    // MaterialManagerより後に呼ばれ、Materialから参照されるShaderを安全に破棄する。
    m_defaultShader = nullptr;
    m_shaders.clear();
    m_shaderLoader.reset();
    m_shaderIDs = 0;
}

GraphicsShader* ShaderManager::GetShaderByFilePath(const std::string& filePath)
{
    for (const std::unique_ptr<GraphicsShader>& shader : m_shaders)
    {
        if (shader->GetFilePath() == filePath)
            return shader.get();
    }

    return nullptr;
}

GraphicsShader* ShaderManager::GetShaderByID(ShaderID id)
{
    for (const std::unique_ptr<GraphicsShader>& shader : m_shaders)
    {
        if (shader->GetID() == id)
            return shader.get();
    }

    return nullptr;
}

GraphicsShader* ShaderManager::LoadShaderFromFile(const std::string& filePath)
{
    // 同じパスのShaderは共有し、再コンパイルや重複所有を避ける。
    if (GraphicsShader* existingShader = GetShaderByFilePath(filePath))
        return existingShader;

    if (!m_shaderLoader)
    {
        Logger::Error("シェーダーローダーが初期化されていません。\nShader: " + filePath);
        return nullptr;
    }

    const ShaderID shaderID = m_shaderIDs++;
    std::unique_ptr<GraphicsShader> shader(m_shaderLoader->LoadShaderFromFile(filePath, shaderID));
    if (!shader || !shader->IsValid())
    {
        Logger::Error("シェーダーの読み込みに失敗しました。\nShader: " + filePath);
        return nullptr;
    }

    GraphicsShader* result = shader.get();
    m_shaders.push_back(std::move(shader));
    return result;
}

void ShaderManager::RemoveShaderByFilePath(const std::string& filePath)
{
    // DefaultLitは常に有効な既定Shaderとして残す。
    if (filePath == DEFAULT_SHADER_FILE_PATH)
        return;

    m_shaders.erase(
        std::remove_if(m_shaders.begin(), m_shaders.end(),
            [&filePath](const std::unique_ptr<GraphicsShader>& shader)
            {
                return shader->GetFilePath() == filePath;
            }),
        m_shaders.end());
}
