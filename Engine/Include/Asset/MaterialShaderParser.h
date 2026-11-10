/*=============================================================================

 File   : MaterialShaderParser.h
 Desc   : MaterialShader DSL の解析結果と解析関数を定義する。

------------------------------------------------------------------------------

 Date   : 2026/11/06
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _MATERIAL_SHADER_PARSER_H_
#define _MATERIAL_SHADER_PARSER_H_

#include <string>
#include <string_view>
#include <vector>

#include "Asset/MaterialShader.h"

namespace Hestia
{
    /// @brief 型付き既定値と、HLSL 宣言・Texture 解決に必要な文字列
    struct ParsedMaterialShaderProperty
    {
        MaterialPropertyDesc m_desc;
        std::string m_hlslType;
        std::string m_textureName;
    };

    /// @brief 固定描画状態と、コンパイル前の HLSL・エントリーポイント
    struct ParsedMaterialShaderPass
    {
        // Parser が Pass 情報と固定描画状態を設定し、Loader が Bytecode と入力レイアウトを補う
        MaterialShaderPass m_passDesc;
        std::string m_hlsl;         // HLSLコード
        std::string m_vertexEntry;  // VSエントリーポイント
        std::string m_pixelEntry;   // PSエントリーポイント

        // GBuffer用のPassはサーフェスシェーダーのエントリーポイントとShadingModel宣言も必要
        std::string m_surfaceEntry; // サーフェスシェーダーエントリーポイント
        std::string m_shadingModel; // サーフェスシェーダーのShadingModel宣言
    };

    /// @brief DSL の宣言順に Property と Pass を所有する解析結果
    struct ParsedMaterialShader
    {
        std::string m_name;
        std::string m_instanceStructName; // 空なら通常描画
        std::string m_sharedHlsl;
        std::vector<ParsedMaterialShaderProperty> m_properties;
        std::vector<ParsedMaterialShaderPass> m_passes;
    };

    /// @brief UTF-8 の MaterialShader DSL を解析する
    /// @param source 1 ファイル分のテキスト
    /// @param out 成功時に解析結果を格納する。失敗時は変更しない
    /// @param error 失敗箇所と理由。成功時は空
    /// @return 解析成功: true / DSL の構文や宣言が不正: false
    bool ParseMaterialShader(std::string_view source, ParsedMaterialShader& out, std::string& error);
}

#endif // _MATERIAL_SHADER_PARSER_H_
