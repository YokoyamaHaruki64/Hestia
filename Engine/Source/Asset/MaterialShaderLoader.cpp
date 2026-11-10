/*=============================================================================

 File   : MaterialShaderLoader.cpp
 Desc   : .matsh を読み込み、MaterialShader Asset を登録する。

------------------------------------------------------------------------------

 Date   : 2026/11/06
 Author : Yokoyama Haruki

=============================================================================*/

#include "pch.h"
#include "Asset/AssetLoader.h"

#include <filesystem>
#include <fstream>
#include <iterator>

#include "Asset/AssetSystem.h"
#include "Asset/MaterialShaderParser.h"
#include "Graphics/DXMaterialShaderCompiler.h"
#include "Log/Logger.h"

namespace Hestia
{
    AssetHandle<MaterialShader> AssetLoader<MaterialShader>::Load(std::string_view name, std::string_view path)
    {
        // エラー報告用のラムダ。呼び出し元に AssetHandle<MaterialShader>{} を返す
        const auto fail = [path](std::string_view message)
        {
            Logger::Error("MaterialShader [" + std::string(path) + "]: " + std::string(message));
            return AssetHandle<MaterialShader>{};
        };

        // UTF-8 のパスと DSL 本体を読み込み、すべての Pass を先に解析
        std::error_code pathError;
        const auto filePath = std::filesystem::absolute(
            std::filesystem::path(std::u8string(path.begin(), path.end())), pathError);

        if (pathError || filePath.extension() != L".matsh")
            return fail("expected a .matsh path");

        std::ifstream file(filePath, std::ios::binary);

        if (!file)
            return fail("could not open file");

        const std::string source((std::istreambuf_iterator<char>(file)), {});
        ParsedMaterialShader parsed;

        std::string error;
        if (!ParseMaterialShader(source, parsed, error))
            return fail(error);

        // 解析済みのPassをコンパイルしMaterialの共通レイアウトを取得
        DXMaterialShaderCompiler::Result compiled;
        if (!DXMaterialShaderCompiler::Compile(filePath, parsed, compiled, error))
            return fail(error);

        // 名前付きTextureはAssetDBで解決し、whiteは既定Texture用に空Handleを保持
        for (std::size_t index = 0; index < compiled.m_properties.size(); ++index)
        {
            const std::string& textureName = parsed.m_properties[index].m_textureName;
            if (textureName.empty() || textureName == "white")
                continue;

            const auto texture = AssetSystem::Load<Texture>(textureName);
            if (!texture.IsValid())
                return fail("could not resolve Texture '" + textureName + "'");

            compiled.m_properties[index].m_defaultValue = texture;
        }

        auto shader = MaterialShader::Create(
            std::move(parsed.m_name), std::move(compiled.m_properties),
            compiled.m_constantBufferSize, std::move(compiled.m_passes), !parsed.m_instanceStructName.empty());

        if (!shader)
            return fail("invalid MaterialShader definition");

        return AssetSystem::RegisterFromLoader(name, std::move(shader));
    }
}
