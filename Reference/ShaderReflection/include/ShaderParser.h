/*=============================================================================

File   : [ShaderParser.h]
Desc   : HLSLファイルを解析して、シェーダーの情報を取得するクラスの宣言

------------------------------------------------------------------------------

Date   : 2026/04/10
Author : Yokoyama Haruki

===============================================================================*/

#ifndef _SHADER_PARSER_H_
#define _SHADER_PARSER_H_

#include "Graphics/GraphicInc.h"
#include "Graphics/Shader/ShaderInc.h"

#include <cctype>
#include <string>
#include <vector>
class ShaderParser
{
private:
    std::string m_sourceCode{};
    bool m_isValid = true;

    bool removeComments();
    void removeSpaceAndNewlines(std::string& str) const;
    const PropertyType parsePropertyType(const std::string& typeStr) const;
    PropertyValue parsePropertyValue(
        const std::string& typeStr,
        const std::string& valueStr) const;

    using ParseFunc = PropertyValue(*)(const std::string&);
    struct TypeParser
    {
        PropertyType type;
        ParseFunc parserFunc;
    };

    static bool parseIntNumber(const char*& valueStr, int& out);
    static bool parseFloatNumber(const char*& valueStr, float& out);
    template<typename T, int N>
    static bool parseArray(const char*& p, T* out, bool(*parseElem)(const char*&, T&))
    {
        for (int i = 0; i < N; ++i)
        {
            if (*p == '\0')
                return false;

            // 要素の前のスペースをスキップ
            while (*p != '\0' && std::isspace(static_cast<unsigned char>(*p)))
                ++p;

            if (!parseElem(p, out[i]))
                return false;

            // 要素の後のスペースをスキップ
            while (*p != '\0' && std::isspace(static_cast<unsigned char>(*p)))
                ++p;

            if (i < N - 1)
            {
                if (*p != ',') return false;
                ++p;
            }
        }

        while (*p != '\0' && std::isspace(static_cast<unsigned char>(*p)))
            ++p;

        if (*p != '\0')
            return false;

        return true;
    }

    static PropertyValue parseFloat(const std::string& valueStr);
    static PropertyValue parseInt(const std::string& valueStr);
    static PropertyValue parseBool(const std::string& valueStr);
    static PropertyValue parseFloat2(const std::string& valueStr);
    static PropertyValue parseFloat3(const std::string& valueStr);
    static PropertyValue parseFloat4(const std::string& valueStr);
    static PropertyValue parseMatrix3x3(const std::string& valueStr);
    static PropertyValue parseMatrix4x4(const std::string& valueStr);
    static PropertyValue parseColor(const std::string& valueStr);
    static PropertyValue parseTexture(const std::string& valueStr);

    static const std::unordered_map<std::string, TypeParser> typeTable;

public:
    ShaderParser(const std::string& sourceCode)
        : m_sourceCode(sourceCode)
    {
        m_isValid = removeComments();
    }

    ~ShaderParser() = default;

    bool IsValid() const { return m_isValid; }

    bool ExtractBlock(const std::string& blockName, std::string& outBlock, int blockOffset = 0) const;
    bool ExtractPassBlock(std::string& outBlock, std::string& outTag, int passOffset = 0) const;
    bool ExtractRenderPhase(RenderPhase& outRenderPhase, int braceOffset = 0) const;
    bool ExtractStateCommand(RenderState& outStateCommand, int braceOffset = 0) const;
    bool ExtractInstanced(bool& outInstanced, int braceOffset = 0) const;
    bool ExtractShaderName(std::string& outName) const;
    bool ExtractHLSL(std::string& outBlock) const;
    bool ExtractPragma(const std::string& pragmaName, std::string& outString) const;
    bool ExtractProperties(std::vector<ShaderPropertyDesc>& outProperties) const;

};


#endif // _SHADER_PARSER_H_
