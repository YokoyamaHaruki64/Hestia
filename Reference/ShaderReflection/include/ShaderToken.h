/*=============================================================================

File   : [ShaderToken.h]
Desc   : シェーダーのトークンに関する情報をまとめた構造体の宣言

------------------------------------------------------------------------------

Date   : 2026/04/10
Author : Yokoyama Haruki

===============================================================================*/

#ifndef _SHADER_TOKEN_H_
#define _SHADER_TOKEN_H_

namespace ShaderToken
{
    static constexpr char SHADER_BLOCK[] = "Shader";
    static constexpr char PROPERTY_BLOCK[] = "Properties";
    static constexpr char HLSL_HEADER[] = "HLSL_BEGIN";
    static constexpr char HLSL_FOOTER[] = "HLSL_END";

    static constexpr char BLOCK_START = '{';
    static constexpr char BLOCK_END = '}';

    static constexpr char COMMENT_SINGLE_LINE[] = "//";
    static constexpr char COMMENT_MULTI_LINE_START[] = "/*";
    static constexpr char COMMENT_MULTI_LINE_END[] = "*/";

    static constexpr char PRAGMA_DIRECTIVE[] = "#pragma ";
    static constexpr char PRAGMA_VERTEX_SHADER[] = "vertex";
    static constexpr char PRAGMA_PIXEL_SHADER[] = "pixel";
    static constexpr char PRAGMA_SHADER_MODEL[] = "shader_model";

    static constexpr char PASS[] = "Pass";
    static constexpr char RENDER_PHASE[] = "RenderPhase";
    static constexpr char RENDER_PHASE_SHADOW[] = "Shadow";
    static constexpr char RENDER_PHASE_OPAQUE[] = "Opaque";
    static constexpr char RENDER_PHASE_TRANSPARENT[] = "Transparent";
    static constexpr char RENDER_PHASE_POST_PROCESS[] = "PostProcess";
    static constexpr char RENDER_PHASE_OVERLAY[] = "Overlay";
    static constexpr char RENDER_PHASE_FINAL_POST_PROCESS[] = "FinalPostProcess";

    static constexpr char CULL[] = "Cull";
    static constexpr char CULL_FRONT[] = "Front";
    static constexpr char CULL_BACK[] = "Back";

    static constexpr char BLEND[] = "Blend";
    static constexpr char BLEND_ALPAHBLEND[] = "AlphaBlend";
    static constexpr char BLEND_ADD[] = "Add";

    static constexpr char ZWRITE[] = "ZWrite";
    static constexpr char INSTANCED[] = "Instanced";

    static constexpr char ZTEST[] = "ZTest";
    static constexpr char ZTEST_LESS[] = "Less";
    static constexpr char ZTEST_LESSEQUAL[] = "LessEqual";
    static constexpr char ZTEST_GREATER[] = "Greater";
    static constexpr char ZTEST_GREATEREQUAL[] = "GreaterEqual";
    static constexpr char ZTEST_EQUAL[] = "Equal";
    static constexpr char ZTEST_NOTEQUAL[] = "NotEqual";

    static constexpr char NONE[] = "None";
    static constexpr char ON[] = "On";
    static constexpr char OFF[] = "Off";
}

namespace TokenSize
{
    static constexpr size_t SHADER_BLOCK = sizeof(ShaderToken::SHADER_BLOCK) - 1;
    static constexpr size_t PROPERTY_BLOCK = sizeof(ShaderToken::PROPERTY_BLOCK) - 1;
    static constexpr size_t HLSL_HEADER = sizeof(ShaderToken::HLSL_HEADER) - 1;
    static constexpr size_t HLSL_FOOTER = sizeof(ShaderToken::HLSL_FOOTER) - 1;

    static constexpr size_t BLOCK_START = sizeof(ShaderToken::BLOCK_START) - 1;
    static constexpr size_t BLOCK_END = sizeof(ShaderToken::BLOCK_END) - 1;

    static constexpr size_t COMMENT_SINGLE_LINE = sizeof(ShaderToken::COMMENT_SINGLE_LINE) - 1;
    static constexpr size_t COMMENT_MULTI_LINE_START = sizeof(ShaderToken::COMMENT_MULTI_LINE_START) - 1;
    static constexpr size_t COMMENT_MULTI_LINE_END = sizeof(ShaderToken::COMMENT_MULTI_LINE_END) - 1;

    static constexpr size_t PRAGMA_DIRECTIVE = sizeof(ShaderToken::PRAGMA_DIRECTIVE) - 1;
    static constexpr size_t PRAGMA_VERTEX_SHADER = sizeof(ShaderToken::PRAGMA_VERTEX_SHADER) - 1;
    static constexpr size_t PRAGMA_PIXEL_SHADER = sizeof(ShaderToken::PRAGMA_PIXEL_SHADER) - 1;
    static constexpr size_t PASS = sizeof(ShaderToken::PASS) - 1;
}

#endif // _SHADER_TOKEN_H_
