/*=============================================================================

 File   : DXMaterialShaderCompiler.h
 Desc   : MaterialShader の HLSL をコンパイルして DX12 用の Pass 記述を生成する。

------------------------------------------------------------------------------

 Date   : 2026/11/07
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _DX_MATERIAL_SHADER_COMPILER_H_
#define _DX_MATERIAL_SHADER_COMPILER_H_

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "Asset/MaterialShader.h"

namespace Hestia
{
    struct ParsedMaterialShader;

    /// @brief 解析済み MaterialShader のDXCコンパイルと DX12 Reflection を担当する
    class DXMaterialShaderCompiler
    {
    public:
        struct Result
        {
            std::vector<MaterialPropertyDesc> m_properties; // Reflection を統合した Property 定義
            std::vector<MaterialShaderPass> m_passes;       // Bytecode と必要な頂点 Stream を含む Pass 記述
            uint32_t m_constantBufferSize = 0;
        };

        /// @brief 全 Pass をコンパイルして共通の Material レイアウトを確定する
        /// @param sourcePath 元の .matsh のパス。診断と include 解決に使用する
        /// @param parsed DSL の解析結果
        /// @param out 成功時にコンパイル結果を格納する
        /// @param error 失敗理由。成功時は空
        /// @return コンパイルと Reflection の統合に成功した場合 true
        static bool Compile(const std::filesystem::path& sourcePath, const ParsedMaterialShader& parsed,
            Result& out, std::string& error);
    };
}

#endif // _DX_MATERIAL_SHADER_COMPILER_H_
