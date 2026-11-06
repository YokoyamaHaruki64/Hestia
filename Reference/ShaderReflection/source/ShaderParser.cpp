/*=============================================================================

File   : [ShaderParser.cpp]
Desc   : HLSLファイルを解析して、シェーダーの情報を取得するクラスの実装

------------------------------------------------------------------------------

Date   : 2026/04/10
Author : Yokoyama Haruki

===============================================================================*/

#include "Graphics/Shader/ShaderParser.h"
#include "Graphics/Shader/ShaderToken.h"
#include "Graphics/Shader/ShaderInc.h"
#include "Graphics/TextureManager.h"

const std::unordered_map<std::string,ShaderParser::TypeParser> ShaderParser::typeTable =
{
    { "float", { PropertyType::Float, ShaderParser::parseFloat } },
    { "Float", { PropertyType::Float, ShaderParser::parseFloat } },
    { "int", { PropertyType::Int, ShaderParser::parseInt } },
    { "Int", { PropertyType::Int, ShaderParser::parseInt } },
    { "bool", { PropertyType::Bool, ShaderParser::parseBool } },
    { "Bool", { PropertyType::Bool, ShaderParser::parseBool } },

    { "Vector2", { PropertyType::Vector2, ShaderParser::parseFloat2 } },
    { "float2", { PropertyType::Vector2, ShaderParser::parseFloat2 } },
    { "Float2", { PropertyType::Vector2, ShaderParser::parseFloat2 } },

    { "Vector3", { PropertyType::Vector3, ShaderParser::parseFloat3 } },
    { "float3", { PropertyType::Vector3, ShaderParser::parseFloat3 } },
    { "Float3", { PropertyType::Vector3, ShaderParser::parseFloat3 } },

    { "Vector4", { PropertyType::Vector4, ShaderParser::parseFloat4 } },
    { "float4", { PropertyType::Vector4, ShaderParser::parseFloat4 } },
    { "Float4", { PropertyType::Vector4, ShaderParser::parseFloat4 } },

    { "matrix3x3", { PropertyType::Matrix3x3, ShaderParser::parseMatrix3x3 } },
    { "Matrix3x3", { PropertyType::Matrix3x3, ShaderParser::parseMatrix3x3 } },
    { "float3x3", { PropertyType::Matrix3x3, ShaderParser::parseMatrix3x3 } },
    { "Float3x3", { PropertyType::Matrix3x3, ShaderParser::parseMatrix3x3 } },

    { "matrix4x4", { PropertyType::Matrix4x4, ShaderParser::parseMatrix4x4 } },
    { "Matrix4x4", { PropertyType::Matrix4x4, ShaderParser::parseMatrix4x4 } },
    { "float4x4", { PropertyType::Matrix4x4, ShaderParser::parseMatrix4x4 } },
    { "Float4x4", { PropertyType::Matrix4x4, ShaderParser::parseMatrix4x4 } },

    { "Color", { PropertyType::Color, ShaderParser::parseColor } },
    { "color", { PropertyType::Color, ShaderParser::parseColor } },

    { "Texture2D", { PropertyType::Texture2D, ShaderParser::parseTexture } },
    { "2D", { PropertyType::Texture2D, ShaderParser::parseTexture } },
};

bool ShaderParser::removeComments()
{
    std::string source(m_sourceCode.begin(), m_sourceCode.end());

    // シングルラインコメントを削除
    size_t pos = 0;
    size_t endPos = 0;
    while ((pos = source.find(ShaderToken::COMMENT_SINGLE_LINE, pos)) != std::string::npos)
    {
        endPos = source.find('\n', pos);
        if (endPos == std::string::npos)
        {
            // 最終行がコメントの場合は以降を削除して終了
            source.erase(pos);
            break;
        }
        source.erase(pos, endPos - pos);
    }

    // マルチラインコメントを削除
    pos = 0;
    while ((pos = source.find(ShaderToken::COMMENT_MULTI_LINE_START, pos)) != std::string::npos)
    {
        endPos = source.find(ShaderToken::COMMENT_MULTI_LINE_END, pos + TokenSize::COMMENT_MULTI_LINE_START);
        if (endPos == std::string::npos)
            return false; // コメントの終了が見つからない場合はエラー
        source.erase(pos, endPos - pos + TokenSize::COMMENT_MULTI_LINE_END);
    }

    m_sourceCode = source;
    return true;
}

void ShaderParser::removeSpaceAndNewlines(std::string& str) const
{
    str.erase(std::remove_if(str.begin(), str.end(), [](unsigned char c) {
        return std::isspace(c);
    }), str.end());
}

const PropertyType ShaderParser::parsePropertyType(const std::string& typeStr) const
{
    auto it = typeTable.find(typeStr);
    if (it != typeTable.end())
        return it->second.type;
    else
        return PropertyType::Unknown;
}

PropertyValue ShaderParser::parsePropertyValue(const std::string& typeStr, const std::string& valueStr) const
{
    auto it = typeTable.find(typeStr);
    if (it != typeTable.end())
    {
        std::string normalizedValue = valueStr;
        if (normalizedValue.size() >= 2 && normalizedValue.front() == '(' && normalizedValue.back() == ')')
        {
            // Property記法の値全体を囲む括弧だけを除去し、配列Parserへ数値列を渡す。
            normalizedValue = normalizedValue.substr(1, normalizedValue.size() - 2);
        }

        return it->second.parserFunc(normalizedValue);
    }
    return PropertyValue();
}

bool ShaderParser::parseIntNumber(const char*& valueStr, int& out)
{
    if (*valueStr == '\0')
        return false;

    // 符号の処理
    bool neg = false;
    if (*valueStr == '-')
    {
        neg = true;
        ++valueStr;
    }

    // 数値部分の処理
    int value = 0;
    bool hasDigit = false;
    while (*valueStr >= '0' && *valueStr <= '9')
    {
        hasDigit = true;
        value = value * 10 + (*valueStr - '0');
        ++valueStr;
    }

    if (!hasDigit)
        return false;

    out = neg ? -value : value;
    return true;
}

bool ShaderParser::parseFloatNumber(const char*& valueStr, float& out)
{
    if (*valueStr == '\0')
        return false;

    // 符号の処理
    bool neg = false;
    if (*valueStr == '-')
    {
        neg = true;
        ++valueStr;
    }

    // 整数部分の処理
    float value = 0;
    bool hasInteger = false;
    while (*valueStr >= '0' && *valueStr <= '9')
    {
        hasInteger = true;
        value = value * 10 + (*valueStr - '0');
        ++valueStr;
    }

    // 小数部分の処理
    bool hasFraction = false;
    if (*valueStr == '.')
        ++valueStr;
    float frac = 0.1f;
    while (*valueStr >= '0' && *valueStr <= '9')
    {
        hasFraction = true;
        value += (*valueStr - '0') * frac;
        frac *= 0.1f;
        ++valueStr;
    }

    if (!hasInteger && !hasFraction)
        return false;

    out = neg ? -value : value;
    return true;
}

PropertyValue ShaderParser::parseFloat(const std::string& valueStr)
{
    if (valueStr.empty())
        return 0.0f;

    float result = 0.0f;
    const char* cstr = valueStr.c_str();
    if (!parseFloatNumber(cstr, result))
        return 0.0f;
    return result;
}

PropertyValue ShaderParser::parseInt(const std::string& valueStr)
{
    if (valueStr.empty())
        return 0;

    int result = 0;
    const char* cstr = valueStr.c_str();
    if (!parseIntNumber(cstr, result))
        return 0;
    return PropertyValue{ result };
}

PropertyValue ShaderParser::parseBool(const std::string& valueStr)
{
    if (valueStr.empty())
        return false;
    return valueStr == "true";
}

PropertyValue ShaderParser::parseFloat2(const std::string& valueStr)
{
    if (valueStr.empty())
        return Vector2::Zero();

    const int COMPONENT_COUNT = 2;
    float result[COMPONENT_COUNT] = { 0.0f };
    const char* cstr = valueStr.c_str();
    if (!parseArray<float, COMPONENT_COUNT>(cstr, result, parseFloatNumber))
        return Vector2::Zero();
    return Vector2(result[0], result[1]);
}

PropertyValue ShaderParser::parseFloat3(const std::string& valueStr)
{
    if( valueStr.empty())
        return Vector3::Zero();

    const int COMPONENT_COUNT = 3;
    float result[COMPONENT_COUNT] = { 0.0f };
    const char* cstr = valueStr.c_str();
    if (!parseArray<float, COMPONENT_COUNT>(cstr, result, parseFloatNumber))
        return Vector3::Zero();
    return Vector3(result[0], result[1], result[2]);
}

PropertyValue ShaderParser::parseFloat4(const std::string& valueStr)
{
    if (valueStr.empty())
        return Vector4::Zero();

    const int COMPONENT_COUNT = 4;
    float result[COMPONENT_COUNT] = { 0.0f };
    const char* cstr = valueStr.c_str();
    if (!parseArray<float, COMPONENT_COUNT>(cstr, result, parseFloatNumber))
        return Vector4::Zero();
    return Vector4(result[0], result[1], result[2], result[3]);
}

PropertyValue ShaderParser::parseMatrix3x3(const std::string& valueStr)
{
    if (valueStr.empty())
        return Matrix3x3::Identity();

    const int INDEX_COUNT = 9;
    float results[INDEX_COUNT] = { 0.0f };
    const char* cstr = valueStr.c_str();
    if (!parseArray<float, INDEX_COUNT>(cstr, results, parseFloatNumber))
        return Matrix3x3::Identity();
    return Matrix3x3(
        results[0], results[1], results[2],
        results[3], results[4], results[5],
        results[6], results[7], results[8]);
}

PropertyValue ShaderParser::parseMatrix4x4(const std::string& valueStr)
{
    if (valueStr.empty())
        return Matrix4x4::Identity();

    const int INDEX_COUNT = 16;
    float results[INDEX_COUNT] = { 0.0f };
    const char* cstr = valueStr.c_str();
    if (!parseArray<float, INDEX_COUNT>(cstr, results, parseFloatNumber))
        return Matrix4x4::Identity();
    return Matrix4x4(
        results[0], results[1], results[2], results[3],
        results[4], results[5], results[6], results[7],
        results[8], results[9], results[10], results[11],
        results[12], results[13], results[14], results[15]);
}

PropertyValue ShaderParser::parseColor(const std::string& valueStr)
{
    if (valueStr.empty())
        return Color::White();

    const int COMPONENT_COUNT = 4;
    float result[COMPONENT_COUNT] = { 0.0f };
    const char* cstr = valueStr.c_str();
    if (!parseArray<float, COMPONENT_COUNT>(cstr, result, parseFloatNumber))
        return Color::White();
    return Color(result[0], result[1], result[2], result[3]);

}

PropertyValue ShaderParser::parseTexture(const std::string& valueStr)
{
    if (valueStr.empty())
        return TextureHandle(-1);

    // Property記法の引用符を除去し、Shaderが宣言した相対パスを既定Textureとして解決する。
    const bool isQuoted = valueStr.size() >= 2 && valueStr.front() == '"' && valueStr.back() == '"';
    const std::string filePath = isQuoted ? valueStr.substr(1, valueStr.size() - 2) : valueStr;
    return TextureManager::LoadTexture(filePath);
}

bool ShaderParser::ExtractBlock(const std::string& blockName, std::string& outBlock,int blockOffset) const
{
    const std::string& source = m_sourceCode;

    // ブロック名を見つける(複数個の同名ブロックがある場合は、blockOffsetで指定された回数スキップする)
    size_t namePos = source.find(blockName);
    int skipCount = 0;
    while (namePos != std::string::npos && skipCount < blockOffset)
    {
        namePos = source.find(blockName, namePos + 1);
        skipCount++;
    }

    // ブロック名が見つからなかった場合はfalseを返す
    if (namePos == std::string::npos)
        return false;

    // ブロックの開始位置を見つける
    size_t bracePos = source.find(ShaderToken::BLOCK_START, namePos);
    if (bracePos == std::string::npos)
        return false;

    // ブロックの終了位置を見つける
    int braceCount = 1;
    size_t endPos = bracePos + 1;

    while (endPos < source.size() && braceCount > 0)
    {
        if (source[endPos] == ShaderToken::BLOCK_START)
            braceCount++;
        else if (source[endPos] == ShaderToken::BLOCK_END)
            braceCount--;
        endPos++;
    }

    if (braceCount != 0)
        return false;

    // ブロックの内容を抽出
    outBlock = source.substr(bracePos + 1, endPos - bracePos - TokenSize::BLOCK_END);
    return true;
}

bool ShaderParser::ExtractPassBlock(std::string& outBlock, std::string& outTag, int passOffset) const
{
    size_t searchPosition = 0;
    size_t passPosition = std::string::npos;
    for (int index = 0; index <= passOffset; ++index)
    {
        passPosition = m_sourceCode.find(ShaderToken::PASS, searchPosition);
        if (passPosition == std::string::npos)
            return false;

        searchPosition = passPosition + TokenSize::PASS;
    }

    const size_t blockStart = m_sourceCode.find(ShaderToken::BLOCK_START, passPosition);
    if (blockStart == std::string::npos)
        return false;

    std::string tag = m_sourceCode.substr(passPosition + TokenSize::PASS, blockStart - (passPosition + TokenSize::PASS));
    tag.erase(std::remove_if(tag.begin(), tag.end(), [](unsigned char character)
        {
            return std::isspace(character);
        }), tag.end());

    if (!tag.empty())
    {
        if (tag.size() < 2 || tag.front() != '"' || tag.back() != '"')
            return false;

        outTag = tag.substr(1, tag.size() - 2);
    }
    else
    {
        outTag.clear();
    }

    int braceCount = 1;
    size_t blockEnd = blockStart + 1;
    while (blockEnd < m_sourceCode.size() && braceCount > 0)
    {
        if (m_sourceCode[blockEnd] == ShaderToken::BLOCK_START)
            ++braceCount;
        else if (m_sourceCode[blockEnd] == ShaderToken::BLOCK_END)
            --braceCount;

        ++blockEnd;
    }

    if (braceCount != 0)
        return false;

    outBlock = m_sourceCode.substr(blockStart + 1, blockEnd - blockStart - 2);
    return true;
}

bool ShaderParser::ExtractRenderPhase(RenderPhase& outRenderPhase, int braceOffset) const
{
    int braceCount = 0;
    for (size_t i = 0; i < m_sourceCode.size(); ++i)
    {
        char c = m_sourceCode[i];
        if (c == ShaderToken::BLOCK_START)
        {
            braceCount++;
        }
        else if (c == ShaderToken::BLOCK_END)
        {
            braceCount--;
        }
        else if (braceCount == braceOffset)
        {
            // ブロック外の内容を解析
            std::string line;
            while (i < m_sourceCode.size() && m_sourceCode[i] != '\n')
            {
                line += m_sourceCode[i];
                ++i;
            }
            // 行を解析してRenderPhaseに反映
            removeSpaceAndNewlines(line);
            if (line.find(ShaderToken::RENDER_PHASE) != std::string::npos)
            {
                size_t pos = line.find('=');
                if (pos != std::string::npos)
                {
                    std::string phaseStr = line.substr(pos + 1);
                    if (phaseStr == ShaderToken::RENDER_PHASE_SHADOW)
                        outRenderPhase = RenderPhase::Shadow;

                    else if (phaseStr == ShaderToken::RENDER_PHASE_OPAQUE)
                        outRenderPhase = RenderPhase::Opaque;

                    else if (phaseStr == ShaderToken::RENDER_PHASE_TRANSPARENT)
                        outRenderPhase = RenderPhase::Transparent;

                    else if (phaseStr == ShaderToken::RENDER_PHASE_POST_PROCESS)
                        outRenderPhase = RenderPhase::PostProcess;

                    else if (phaseStr == ShaderToken::RENDER_PHASE_OVERLAY)
                        outRenderPhase = RenderPhase::Overlay;

                    else if (phaseStr == ShaderToken::RENDER_PHASE_FINAL_POST_PROCESS)
                        outRenderPhase = RenderPhase::FinalPostProcess;

                    else
                        return false; // 不明なコマンド

                    return true;
                }
            }
        }
    }
    return true;
}

bool ShaderParser::ExtractStateCommand(RenderState& outStateCommand, int braceOffset) const
{
    int braceCount = 0;

    for (size_t i = 0; i < m_sourceCode.size(); ++i)
    {
        char c = m_sourceCode[i];
        if (c == ShaderToken::BLOCK_START)
        {
            braceCount++;
        }
        else if (c == ShaderToken::BLOCK_END)
        {
            braceCount--;
        }
        else if (braceCount == braceOffset)
        {
            // ブロック外の内容を解析
            std::string line;
            while (i < m_sourceCode.size() && m_sourceCode[i] != '\n')
            {
                line += m_sourceCode[i];
                ++i;
            }
            // 行を解析してStateCommandに反映
            removeSpaceAndNewlines(line);
            if (line.find(ShaderToken::BLEND) != std::string::npos)
            {
                // BlendModeの解析
                size_t pos = line.find('=');
                if (pos != std::string::npos)
                {
                    std::string commandStr = line.substr(pos + 1);
                    if (commandStr == ShaderToken::NONE ||
                        commandStr == ShaderToken::OFF)
                        outStateCommand.blendMode = BlendMode::None;

                    else if (commandStr == ShaderToken::BLEND_ALPAHBLEND)
                        outStateCommand.blendMode = BlendMode::AlphaBlend;

                    else if (commandStr == ShaderToken::BLEND_ADD)
                        outStateCommand.blendMode = BlendMode::Add;

                    else
                        return false; // 不明なコマンド
                }
            }
            else if (line.find(ShaderToken::CULL) != std::string::npos)
            {
                // CullModeの解析
                size_t pos = line.find('=');
                if (pos != std::string::npos)
                {
                    std::string commandStr = line.substr(pos + 1);
                    if (commandStr == ShaderToken::NONE ||
                        commandStr == ShaderToken::OFF)
                        outStateCommand.cullMode = CullMode::None;

                    else if (commandStr == ShaderToken::CULL_FRONT)
                        outStateCommand.cullMode = CullMode::Front;

                    else if (commandStr == ShaderToken::CULL_BACK)
                        outStateCommand.cullMode = CullMode::Back;

                    else
                        return false; // 不明なコマンド
                }
            }
            else if (line.find(ShaderToken::ZTEST) != std::string::npos)
            {
                // DepthTestModeの解析
                size_t pos = line.find('=');
                if (pos != std::string::npos)
                {
                    std::string commandStr = line.substr(pos + 1);
                    if (commandStr == ShaderToken::NONE ||
                        commandStr == ShaderToken::OFF)
                        outStateCommand.depthTestMode = DepthTestMode::None;

                    else if (commandStr == ShaderToken::ZTEST_LESS)
                        outStateCommand.depthTestMode = DepthTestMode::Less;

                    else if (commandStr == ShaderToken::ZTEST_LESSEQUAL)
                        outStateCommand.depthTestMode = DepthTestMode::LessEqual;

                    else if (commandStr == ShaderToken::ZTEST_GREATER)
                        outStateCommand.depthTestMode = DepthTestMode::Greater;

                    else if (commandStr == ShaderToken::ZTEST_GREATEREQUAL)
                        outStateCommand.depthTestMode = DepthTestMode::GreaterEqual;

                    else if (commandStr == ShaderToken::ZTEST_EQUAL)
                        outStateCommand.depthTestMode = DepthTestMode::Equal;

                    else if (commandStr == ShaderToken::ZTEST_NOTEQUAL)
                        outStateCommand.depthTestMode = DepthTestMode::NotEqual;

                    else 
                        return false; // 不明なコマンド
                }
            }
            else if (line.find(ShaderToken::ZWRITE) != std::string::npos)
            {
                // DepthWriteModeの解析
                size_t pos = line.find('=');
                if (pos != std::string::npos)
                {
                    std::string commandStr = line.substr(pos + 1);
                    if (commandStr == ShaderToken::NONE ||
                        commandStr == ShaderToken::OFF)
                        outStateCommand.depthWriteMode = DepthWriteMode::None;

                    else if (commandStr == ShaderToken::ON)
                        outStateCommand.depthWriteMode = DepthWriteMode::Write;

                    else
                        return false; // 不明なコマンド
                }
            }
        }
    }
    return true;
}

bool ShaderParser::ExtractInstanced(bool& outInstanced, int braceOffset) const
{
    outInstanced = false;
    int braceCount = 0;
    for (size_t index = 0; index < m_sourceCode.size(); ++index)
    {
        if (m_sourceCode[index] == ShaderToken::BLOCK_START)
        {
            ++braceCount;
            continue;
        }
        if (m_sourceCode[index] == ShaderToken::BLOCK_END)
        {
            --braceCount;
            continue;
        }
        if (braceCount != braceOffset)
            continue;

        std::string line;
        while (index < m_sourceCode.size() && m_sourceCode[index] != '\n')
        {
            line += m_sourceCode[index];
            ++index;
        }
        removeSpaceAndNewlines(line);
        const size_t equalPosition = line.find('=');
        if (equalPosition == std::string::npos || line.substr(0, equalPosition) != ShaderToken::INSTANCED)
            continue;

        const std::string value = line.substr(equalPosition + 1);
        if (value == "true")
        {
            outInstanced = true;
            return true;
        }
        return value == "false";
    }
    return true;
}

bool ShaderParser::ExtractShaderName(std::string& outName) const
{
    const std::string& source = m_sourceCode;

    // シェーダーブロックを見つける
    size_t blockPos = source.find(ShaderToken::SHADER_BLOCK);
    if (blockPos == std::string::npos)
        return false;

    // ブロック名を抽出
    size_t nameStart = blockPos + TokenSize::SHADER_BLOCK;

    // 空白スキップ
    while (nameStart < source.size() && std::isspace(source[nameStart]))
        ++nameStart;

    // "を除く
    if( nameStart < source.size() && source[nameStart] == '"')
    {
        ++nameStart;
        size_t nameEnd = source.find('"', nameStart);

        if (nameEnd == std::string::npos)
            return false;

        outName = source.substr(nameStart, nameEnd - nameStart);
        return true;
    }

    // "がない場合エラーとして扱う
    return false;


}

bool ShaderParser::ExtractHLSL(std::string& outBlock) const
{
    const std::string& source = m_sourceCode;

    // HLSLブロックの開始位置を見つける
    size_t startPos = source.find(ShaderToken::HLSL_HEADER);
    if (startPos == std::string::npos)
        return false;

    // HLSLブロックの終了位置を見つける
    size_t endPos = source.find(ShaderToken::HLSL_FOOTER, startPos);
    if (endPos == std::string::npos)
        return false;

    size_t contentStart = startPos + TokenSize::HLSL_HEADER;
    size_t blockSize = endPos - contentStart;
    // HLSLブロックの内容を抽出
    outBlock = source.substr(contentStart, blockSize);
    return true;
}

bool ShaderParser::ExtractPragma(const std::string& pragmaName, std::string& outString) const
{
    const std::string& source = m_sourceCode;
    std::string directive = ShaderToken::PRAGMA_DIRECTIVE + pragmaName + ' ';
    // プラグマ行を見つける
    size_t pos = source.find(directive);
    if (pos == std::string::npos)
        return false;

    // プラグマ行の内容を抽出
    size_t endPos = source.find('\n', pos);
    if (endPos == std::string::npos)
        endPos = source.size();
    outString = source.substr(pos + directive.size(), endPos - pos - directive.size());

    return true;
}

bool ShaderParser::ExtractProperties(std::vector<ShaderPropertyDesc>& outProperties) const
{
    std::string blockContent;
    if (!ExtractBlock(ShaderToken::PROPERTY_BLOCK, blockContent))
        return false;

    // プロパティブロックの内容を行ごとに分割
    std::istringstream stream(blockContent);
    std::string line;
    ShaderPropertyID propertyID = 0; // プロパティIDの初期値
    while (std::getline(stream, line))
    {
        /*
        Propertyの書き方
        name("displayName", type) = defaultValue
        例: _Color("Main Color", Color) = (1,1,1,1)
        */

        // 行をトークンに分割
        std::string name, displayName, typeStr, defaultValue;
        removeSpaceAndNewlines(line);

        // トークンの解析
        size_t nameEnd = line.find('(');
        if (nameEnd == std::string::npos)
            continue;

        name = line.substr(0, nameEnd);

        size_t displayNameStart = nameEnd + 2; // "の次の文字から開始
        size_t displayNameEnd = line.find('"', displayNameStart);
        if (displayNameEnd == std::string::npos)
            continue;
        displayName = line.substr(displayNameStart, displayNameEnd - displayNameStart);

        size_t typeStart = displayNameEnd + 2; // ,の次の文字から開始
        size_t typeEnd = line.find(')', typeStart);
        if (typeEnd == std::string::npos)
            continue;
        typeStr = line.substr(typeStart, typeEnd - typeStart);

        size_t defaultValuePos = line.find('=', typeEnd);
        if (defaultValuePos != std::string::npos)
            defaultValue = line.substr(defaultValuePos + 1);

        // プロパティの情報をShaderProperty構造体に格納
        ShaderPropertyDesc property(
            propertyID++, // プロパティIDを自動で割り当て
            name,
            displayName,
            parsePropertyType(typeStr),
            parsePropertyValue(typeStr, defaultValue)
        );

        outProperties.push_back(property);
    }

    return true;
}
