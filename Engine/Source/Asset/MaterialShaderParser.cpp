/*=============================================================================

 File   : MaterialShaderParser.cpp
 Desc   : MaterialShader DSL の宣言と HLSL を解析する。

------------------------------------------------------------------------------

 Date   : 2026/11/06
 Author : Yokoyama Haruki

=============================================================================*/

#include "pch.h"
#include "Asset/MaterialShaderParser.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <type_traits>
#include <utility>

namespace Hestia
{
    namespace
    {
        // Word は識別子や数値の候補。用途ごとの妥当性は Parser で検証
        enum class TokenKind
        {
            End,        // EOF
            Invalid,    // コメントや引用符が閉じていない
            Word,       // 符号・小数点・指数表記を含む英数字の連続
            Quoted,     // " || ' で囲まれた文字(列)。エスケープされた引用符は閉じない
            Symbol      // 1文字だけの記号
        };

        // 元のソースを参照し、字句の位置をエラーの行番号と HLSL の切り出しに使う
        struct Token
        {
            TokenKind m_kind = TokenKind::End;
            std::string_view m_text;
            std::size_t m_offset = 0;

            std::size_t End() const { return m_offset + m_text.size(); }
        };

        // 先頭文字が識別子の開始文字か判定する。C++などと同じルールで、先頭は英字かアンダースコア
        bool IsIdentifierStart(char c)
        {
            return c == '_' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
        }

        // 先頭文字が数字か判定する
        bool IsDigit(char c)
        {
            return c >= '0' && c <= '9';
        }

        // 識別子のルールに従うか判定する。先頭は英字かアンダースコア、2文字目以降は数字も許可
        bool IsIdentifier(std::string_view text)
        {
            // std::ranges::all_of は C++20 で導入された範囲ベースのアルゴリズムで、
            // 指定された範囲内のすべての要素が条件を満たすかどうかを判定する

            return !text.empty() &&
                IsIdentifierStart(text.front()) &&
                std::ranges::all_of(text, [](char c) { return IsIdentifierStart(c) || IsDigit(c); });
        }

        // 符号・小数点・指数表記を含めて Word の候補か判定する。用途ごとの妥当性は Parser で検証
        bool IsWordCharacter(char c)
        {
            return IsIdentifierStart(c) || IsDigit(c) || c == '.' || c == '+' || c == '-';
        }

        // 改行を含まない空白だけならインデントとして扱う。空の区間も含む
        bool IsOnlyIndentation(std::string_view text)
        {
            return std::ranges::all_of(text, [](char c)
            {
                return c == ' ' || c == '\t' || c == '\f' || c == '\v';
            });
        }

        // エスケープされた引用符を飛ばし、閉じ引用符の直後を返す
        std::size_t FindQuotedEnd(std::string_view source, std::size_t offset)
        {
            const char quote = source[offset++];
            while (offset < source.size())
            {
                const char c = source[offset++];
                if (c == quote)
                    return offset;
                if (c == '\\' && offset < source.size())
                    ++offset;
            }
            return std::string_view::npos;
        }

        // offset 以降から次の字句を1個返す。コメントは元のソースに残し、読み取り位置だけを進める
        Token ReadToken(std::string_view source, std::size_t offset)
        {
            // コメントと空白を読み飛ばして次のトークンの開始位置を求める
            while (offset < source.size())
            {
                // 空白や改行はすべて読み飛ばす
                if (std::string_view(" \t\r\n\f\v").find(source[offset]) != std::string_view::npos)
                {
                    ++offset;
                    continue;
                }

                // コメントは行コメントとブロックコメントの両方を読み飛ばす
                const auto rest = source.substr(offset);
                if (rest.starts_with("//"))
                {// 行コメントは改行まで読み飛ばす
                    const auto lineEnd = source.find_first_of("\r\n", offset + 2);
                    offset = lineEnd == std::string_view::npos ? source.size() : lineEnd;
                    continue;
                }

                if (!rest.starts_with("/*")) // ブロックコメントは閉じるまで読み飛ばす
                    break;

                // ブロックコメントの終了位置を探索
                const auto commentEnd = source.find("*/", offset + 2);

                if (commentEnd == std::string_view::npos)
                    return { TokenKind::Invalid, rest, offset };

                offset = commentEnd + 2; // */を含めて読み飛ばす
            }

            // EOFまで到達した場合はEndを返す
            if (offset == source.size())
                return { TokenKind::End, {}, offset };

            // 引用符の中身はまとめて読み、エスケープされた引用符では閉じない
            const char first = source[offset];
            if (first == '"' || first == '\'')
            {
                const auto end = FindQuotedEnd(source, offset);
                if (end == std::string_view::npos)
                    return { TokenKind::Invalid, source.substr(offset), offset };
                return { TokenKind::Quoted, source.substr(offset, end - offset), offset };
            }

            // 符号・小数点・指数表記を含めて Word の候補を読み取る
            if (IsWordCharacter(first))
            {
                // 先頭文字はすでに判定済みなので2文字目以降を読み進める
                std::size_t end = offset + 1;   
                while (end < source.size() && IsWordCharacter(source[end]))
                    ++end;

                return { TokenKind::Word, source.substr(offset, end - offset), offset };
            }

            // 1文字だけの記号はそのまま返す
            return { TokenKind::Symbol, source.substr(offset, 1), offset };
        }

        // State類のkey=valueの設定を保持する
        struct StateAssignment
        {
            Token m_key;
            Token m_value;
        };

        // Shader 全体の設定を読み終えるまで、Pass 固有の上書きと診断位置を保持
        struct PendingPass
        {
            ParsedMaterialShaderPass m_result;
            std::vector<StateAssignment> m_overrides;
            Token m_start;
        };

        // 次の字句を1個保持し、DSL の各ブロックを読み進める
        class Parser
        {
        public:
            Parser(std::string_view source, std::string& error)
                : m_source(source), m_error(error), m_token(ReadToken(source, 0))
            {
            }

            bool Parse(ParsedMaterialShader& shader);

        private:
            bool Fail(const Token& token, std::string_view reason);             // エラーを報告して false を返す
            void Advance() { m_token = ReadToken(m_source, m_token.End()); }    // 次の字句を読み込む
            bool Consume(std::string_view text);    // text と一致すれば読み飛ばす。失敗してもエラーは報告しない
            bool Expect(std::string_view text);     // text と一致すれば読み飛ばす。失敗したらエラーを報告して false を返す
            bool ReadString(std::string& value);    // 引用符で囲まれた文字列を読み込む。エスケープされた引用符は閉じない

            // 数値の読み取り
            template<class T>
            bool ReadNumber(T& value);

            // タプルの読み取り。Count個の float をカンマ区切りで読み込む
            template<std::size_t Count>
            bool ReadTuple(std::array<float, Count>& values);

            // Propertyの読み取り
            bool ReadProperties(std::vector<ParsedMaterialShaderProperty>& properties);
            bool ReadPropertyDefault(std::string_view type, ParsedMaterialShaderProperty& property);

            // key = value の設定の読み取り
            bool ReadSetting(std::vector<StateAssignment>& settings);
            // key = value の設定を Pass に適用する。型の不一致はエラー
            bool ApplySetting(const StateAssignment& setting, ParsedMaterialShaderPass& pass);

            // 対応表から設定値を引き、一致した値を出力する
            // Tは出力型、Choicesはstd::pair<std::string_view, T>の配列
            template<class T, class Choices>
            bool SetChoice(const StateAssignment& setting, T& out, const Choices& choices);

            // Passの読み取り
            bool ReadPass(PendingPass& pass);
            // Passの設定をShader全体の設定で上書きする。型の不一致はエラー
            bool ResolvePass(const ParsedMaterialShaderPass& defaults, PendingPass& pass);

            // startからendまでの字句が同じ行にあるか判定する。改行が含まれる場合はfalse
            bool SameLine(std::size_t start, std::size_t end) const;

            // HLSLコードを読み取り、endMarkerまでの範囲をoutに格納する。endMarkerは字句単位で判定する
            bool ReadHlsl(std::string_view endMarker, std::string& out, ParsedMaterialShaderPass* pass);

            // 独自の #pragma を読み取り、Pass の固定描画状態を設定する。失敗時はエラーを報告して false を返す
            bool ReadPragma(std::size_t hashOffset, ParsedMaterialShaderPass* pass, std::size_t& stripEnd);

            // DSL で受け付ける表記と、Pass に格納する値の対応
            static constexpr std::pair<std::string_view, RenderType> RENDER_TYPE_CHOICES[] =
            {
                { "GBuffer", RenderType::GBuffer },
                { "Forward", RenderType::Forward },
            };

            static constexpr std::pair<std::string_view, RenderPhase> RENDER_PHASE_CHOICES[] =
            {
                { "Opaque", RenderPhase::Opaque },
                { "Masked", RenderPhase::Masked },
                { "Transparent", RenderPhase::Transparent },
            };

            static constexpr std::pair<std::string_view, FillMode> FILL_CHOICES[] =
            {
                { "Solid", FillMode::Solid },
                { "Wireframe", FillMode::Wireframe },
            };

            static constexpr std::pair<std::string_view, CullMode> CULL_CHOICES[] =
            {
                { "None", CullMode::None },
                { "Front", CullMode::Front },
                { "Back", CullMode::Back },
            };

            static constexpr std::pair<std::string_view, BlendMode> BLEND_CHOICES[] =
            {
                { "None", BlendMode::None },
                { "Alpha", BlendMode::Alpha },
                { "Additive", BlendMode::Additive },
            };

            static constexpr std::pair<std::string_view, PrimitiveTopologyType> TOPOLOGY_CHOICES[] =
            {
                { "Triangle", PrimitiveTopologyType::Triangle },
                { "Line", PrimitiveTopologyType::Line },
                { "Point", PrimitiveTopologyType::Point },
            };

            static constexpr std::pair<std::string_view, bool> ON_OFF_CHOICES[] =
            {
                { "On", true },
                { "Off", false },
            };

            static constexpr std::pair<std::string_view, CompareFunc> DEPTH_COMPARE_CHOICES[] =
            {
                { "Less", CompareFunc::Less },
                { "LessEqual", CompareFunc::LessEqual },
                { "Greater", CompareFunc::Greater },
                { "GreaterEqual", CompareFunc::GreaterEqual },
                { "Equal", CompareFunc::Equal },
                { "NotEqual", CompareFunc::NotEqual },
                { "Always", CompareFunc::Always },
            };

            std::string_view m_source;
            std::string& m_error;
            Token m_token;
        };

        bool Parser::Fail(const Token& token, std::string_view reason)
        {
            // 行番号は offset までの改行の数 + 1。字句が閉じていない場合は EOF までの行数になる
            // TokenKind::Invalid はコメントや引用符が閉じていない場合に返されるため、reasonを使わずに固定メッセージを返す
            const auto line = std::ranges::count(m_source.substr(0, token.m_offset), '\n') + 1;
            m_error = "Line " +
                std::to_string(line) + ": " +
                (token.m_kind == TokenKind::Invalid ? "unterminated comment or quoted string" : std::string(reason));

            return false;
        }

        bool Parser::Consume(std::string_view text)
        {
            if (m_token.m_text != text)
                return false;
            Advance();
            return true;
        }

        bool Parser::Expect(std::string_view text)
        {
            // 左から評価されるためConsumeが先に呼ばれ、失敗時にエラーを報告する
            return Consume(text) || Fail(m_token, "expected '" + std::string(text) + "'");
        }

        bool Parser::ReadString(std::string& value)
        {
            if (m_token.m_kind != TokenKind::Quoted || m_token.m_text.front() != '"')
                return Fail(m_token, "expected a double-quoted string");

            // 引用符を除いた中身を返す取得
            const auto text = m_token.m_text.substr(1, m_token.m_text.size() - 2);

            // DSLの文字列は1行に収める必要がある。改行を含む場合はエラー
            if (text.find_first_of("\r\n") != std::string_view::npos)
                return Fail(m_token, "a DSL string must be on one line");

            value.clear();
            // エスケープを除いて文字列を構築する
            for (std::size_t index = 0; index < text.size(); ++index)
            {
                if (text[index] == '\\' &&
                    index + 1 < text.size() &&
                    (text[index + 1] == '\\' || text[index + 1] == '"'))
                    ++index;

                value.push_back(text[index]);
            }
            Advance();
            return true;
        }

        template<class T>
        bool Parser::ReadNumber(T& value)
        {
            auto text = m_token.m_text;
            if (text.starts_with('+'))
                text.remove_prefix(1);
            if constexpr (std::is_same_v<T, float>)
            {
                if (text.ends_with('f') || text.ends_with('F'))
                    text.remove_suffix(1);
            }
            if (m_token.m_kind != TokenKind::Word || text.empty() ||
                (m_token.m_text.starts_with('+') && text.starts_with('-')))
                return Fail(m_token, "expected a numeric literal");

            // リテラル全体が対象型に変換できる場合だけ採用
            const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
            if (result.ec != std::errc{} || result.ptr != text.data() + text.size())
                return Fail(m_token, "invalid numeric literal");
            if constexpr (std::is_same_v<T, float>)
            {
                if (!std::isfinite(value))
                    return Fail(m_token, "expected a finite numeric literal");
            }
            Advance();
            return true;
        }

        template<std::size_t Count>
        bool Parser::ReadTuple(std::array<float, Count>& values)
        {
            if (!Expect("("))
                return false;

            for (std::size_t index = 0; index < Count; ++index)
            {
                if ((index != 0 && !Expect(",")) || !ReadNumber(values[index]))
                    return false;
            }
            return Expect(")");
        }

        bool Parser::ReadPropertyDefault(std::string_view type, ParsedMaterialShaderProperty& property)
        {
            auto& value = property.m_desc.m_defaultValue;
            auto& hlslType = property.m_hlslType;

            // DSL 名を CPU の値型と HLSL の宣言型へ対応付ける
            if (type == "Bool" || type == "bool")
            {
                hlslType = "bool";
                if (Consume("true"))
                    value = true;
                else if (Consume("false"))
                    value = false;
                else
                    return Fail(m_token, "expected 'true' or 'false'");
            }
            else if (type == "Int" || type == "int")
            {
                hlslType = "int";
                return ReadNumber(value.emplace<int32_t>());
            }
            else if (type == "Float" || type == "float" || type == "Range")
            {
                hlslType = "float";
                return ReadNumber(value.emplace<float>());
            }
            else if (type == "Vector2" || type == "Float2" || type == "float2")
            {
                hlslType = "float2";
                std::array<float, 2> values;
                if (!ReadTuple(values))
                    return false;
                value.emplace<Vector2>(values[0], values[1]);
            }
            else if (type == "Vector3" || type == "Float3" || type == "float3")
            {
                hlslType = "float3";
                std::array<float, 3> values;
                if (!ReadTuple(values))
                    return false;
                value.emplace<Vector3>(values[0], values[1], values[2]);
            }
            else if (type == "Vector4" || type == "Float4" || type == "float4" ||
                type == "Color" || type == "color")
            {
                hlslType = "float4";
                std::array<float, 4> values;
                if (!ReadTuple(values))
                    return false;
                if (type == "Color" || type == "color")
                    value.emplace<Color>(values[0], values[1], values[2], values[3]);
                else
                    value.emplace<Vector4>(values[0], values[1], values[2], values[3]);
            }
            else if (type == "Matrix3x3" || type == "matrix3x3" || type == "Float3x3" || type == "float3x3")
            {
                hlslType = "float3x3";
                std::array<float, 9> values;
                if (!ReadTuple(values))
                    return false;
                value.emplace<Matrix3x3>(
                    values[0], values[1], values[2],
                    values[3], values[4], values[5],
                    values[6], values[7], values[8]);
            }
            else if (type == "Matrix4x4" || type == "matrix4x4" || type == "Float4x4" || type == "float4x4")
            {
                hlslType = "float4x4";
                std::array<float, 16> values;
                if (!ReadTuple(values))
                    return false;
                value.emplace<Matrix4x4>(
                    values[0], values[1], values[2], values[3],
                    values[4], values[5], values[6], values[7],
                    values[8], values[9], values[10], values[11],
                    values[12], values[13], values[14], values[15]);
            }
            else if (type == "Texture2D" || type == "2D")
            {
                hlslType = "Texture2D";
                value.emplace<AssetHandle<Texture>>();
                return ReadString(property.m_textureName);
            }
            else
                return Fail(m_token, "unsupported property type '" + std::string(type) + "'");

            return true;
        }

        bool Parser::ReadProperties(std::vector<ParsedMaterialShaderProperty>& properties)
        {
            if (!Expect("{"))
                return false;

            while (!Consume("}"))
            {
                // Property の識別子と表示用の宣言を取得
                const Token name = m_token;
                if (!IsIdentifier(name.m_text))
                    return Fail(name, "expected a property identifier");
                if (std::ranges::any_of(properties, [&](const auto& property) { return property.m_desc.m_name == name.m_text; }))
                    return Fail(name, "duplicate property name");
                Advance();

                std::string displayName;
                if (!Expect("(") || !ReadString(displayName) || !Expect(","))
                    return false;
                if (m_token.m_kind != TokenKind::Word)
                    return Fail(m_token, "expected a property type");
                const auto type = m_token.m_text;
                Advance();

                // Range の端点を検証し、値型は float として保持
                if (type == "Range")
                {
                    const Token rangeStart = m_token;
                    std::array<float, 2> range;
                    if (!ReadTuple(range))
                        return false;
                    if (range[0] > range[1])
                        return Fail(rangeStart, "Range minimum exceeds its maximum");
                }

                ParsedMaterialShaderProperty property;
                property.m_desc.m_name = name.m_text;
                if (!Expect(")") || !Expect("=") || !ReadPropertyDefault(type, property))
                    return false;
                Consume(";");
                properties.push_back(std::move(property));
            }
            return true;
        }

        bool Parser::ReadSetting(std::vector<StateAssignment>& settings)
        {
            // key = value; の構文と同一スコープ内の重複を確認し、型への変換は設定の適用時に行う
            const Token key = m_token;
            if (!IsIdentifier(key.m_text))
                return Fail(key, "expected a render-state setting");
            if (std::ranges::any_of(settings, [&](const auto& setting) { return setting.m_key.m_text == key.m_text; }))
                return Fail(key, "duplicate setting '" + std::string(key.m_text) + "'");
            Advance();
            if (!Expect("="))
                return false;

            const Token value = m_token;
            if (value.m_kind != TokenKind::Word)
                return Fail(value, "expected a setting value");
            Advance();
            if (!Expect(";"))
                return false;

            settings.push_back({ key, value });
            return true;
        }

        template<class T, class Choices>
        bool Parser::SetChoice(const StateAssignment& setting, T& out, const Choices& choices)
        {
            for (const auto& [text, value] : choices)
            {
                if (setting.m_value.m_text == text)
                {
                    out = value;
                    return true;
                }
            }
            return Fail(setting.m_value, "unsupported value for '" + std::string(setting.m_key.m_text) + "'");
        }

        bool Parser::ApplySetting(const StateAssignment& setting, ParsedMaterialShaderPass& pass)
        {
            const auto key = setting.m_key.m_text;
            const auto value = setting.m_value.m_text;

            auto& info = pass.m_passDesc.m_info;
            auto& pipeline = pass.m_passDesc.m_pipeline;

            // RenderTypeの設定
            if (key == "RenderType")
                return SetChoice(setting, info.m_renderType, RENDER_TYPE_CHOICES);

            // RenderPhaseの設定
            if (key == "RenderPhase")
                return SetChoice(setting, info.m_renderPhase, RENDER_PHASE_CHOICES);

            // ShadingModelの設定
            if (key == "ShadingModel")
            {
                if (!IsIdentifier(value))
                    return Fail(setting.m_value, "expected a ShadingModel identifier");
                pass.m_shadingModel = value;
                return true;
            }

            // PipelineStateの設定
            if (key == "Fill")
                return SetChoice(setting, pipeline.m_rasterizer.m_fill, FILL_CHOICES);

            if (key == "Cull")
                return SetChoice(setting, pipeline.m_rasterizer.m_cull, CULL_CHOICES);

            if (key == "Blend")
                return SetChoice(setting, pipeline.m_blend, BLEND_CHOICES);

            if (key == "Topology")
                return SetChoice(setting, pipeline.m_topology, TOPOLOGY_CHOICES);

            if (key == "ZWrite" || key == "Zwrite")
                return SetChoice(setting, pipeline.m_depthStencil.m_depthWrite, ON_OFF_CHOICES);

            if (key == "ZTest")
            {
                pipeline.m_depthStencil.m_depthTest = value != "Off";
                if (value == "Off" || value == "On")
                    return true;

                return SetChoice(setting, pipeline.m_depthStencil.m_depthFunc, DEPTH_COMPARE_CHOICES);
            }

            return Fail(setting.m_key, "unsupported setting '" + std::string(key) + "'");
        }

        bool Parser::SameLine(std::size_t start, std::size_t end) const
        {
            return m_source.substr(start, end - start).find_first_of("\r\n") == std::string_view::npos;
        }

        bool Parser::ReadPragma(std::size_t hashOffset, ParsedMaterialShaderPass* pass, std::size_t& stripEnd)
        {
            // 行頭の '#' から、DSL が扱う pragma の部分だけを字句解析する
            const auto hashEnd = hashOffset + 1;
            m_token = ReadToken(m_source, hashEnd);
            if (!SameLine(hashEnd, m_token.m_offset) || !Consume("pragma"))
                return true;

            if (!SameLine(hashEnd, m_token.m_offset))
                return true;

            // pragma の種類を判定
            const Token directive = m_token;
            if (directive.m_text != "vertex" &&
                directive.m_text != "pixel" &&
                directive.m_text != "surface")
                return true;

            if (pass == nullptr)
                return Fail(directive, "shader entry pragmas belong inside a Pass");

            Advance();

            // 重複判定。ここで初めてEntryが設定されるため、emptyかどうかで重複を判定できる
            std::string* entry = &pass->m_vertexEntry;
            if (directive.m_text == "pixel")
                entry = &pass->m_pixelEntry;
            else if (directive.m_text == "surface")
                entry = &pass->m_surfaceEntry;

            if (!entry->empty())
                return Fail(directive, "duplicate shader entry pragma");

            // エントリーポイントの識別子を取得し、同じ行に続く字句がないことを確認する
            if (!SameLine(directive.End(), m_token.m_offset) || !IsIdentifier(m_token.m_text))
                return Fail(m_token, "expected a shader entry identifier on the pragma line");

            // エントリーポイントを設定し、空白化する範囲の終端を返す
            *entry = m_token.m_text;
            stripEnd = m_token.End();

            Advance();

            // 入力末尾と HLSL 終了マーカーは ReadHlsl 側で処理する
            if (m_token.m_kind == TokenKind::End || m_token.m_text == "HLSL_END")
                return true;

            // 同じ行に続く字句がある場合はエラー
            if (SameLine(stripEnd, m_token.m_offset))
                return Fail(m_token, "unexpected token after shader entry");

            return true;
        }

        bool Parser::ReadHlsl(std::string_view endMarker, std::string& out, ParsedMaterialShaderPass* pass)
        {
            // 本文の先頭を行頭として扱い、終了マーカーと pragma の候補まで読み飛ばす
            const auto bodyStart = m_token.End();
            const std::string scanCharacters{ endMarker.front(), '#', '/', '"', '\'', '\r', '\n' };

            std::vector<std::pair<std::size_t, std::size_t>> pragmaRanges; // pragmaの空白化範囲を記録する
            auto cursor = bodyStart;
            auto lineStart = bodyStart;

            // 本文の終了マーカーまで読み進める。コメントや引用符の内部にあるマーカーは無視する
            while (cursor < m_source.size())
            {
                // 次の候補文字まで読み飛ばす。見つからなければ本文の終端
                const auto offset = m_source.find_first_of(scanCharacters, cursor);

                // 見つからなければ本文の終端
                if (offset == std::string_view::npos)
                    break;

                const char c = m_source[offset];
                const auto rest = m_source.substr(offset);
                cursor = offset + 1;

                // コメントと引用符の内部にあるマーカーや pragma は無視する
                if (rest.starts_with("//"))
                {
                    cursor = m_source.find_first_of("\r\n", offset + 2);
                    continue;
                }
                if (rest.starts_with("/*"))
                {
                    const auto commentEnd = m_source.find("*/", offset + 2);
                    if (commentEnd == std::string_view::npos)
                        return Fail({ TokenKind::Invalid, {}, offset }, {});

                    cursor = commentEnd + 2;
                    // 改行をまたいだコメントの閉じ位置から、その行のコードを探す
                    if (!SameLine(offset, commentEnd))
                        lineStart = cursor;
                    continue;
                }
                if (c == '"' || c == '\'')
                {
                    cursor = FindQuotedEnd(m_source, offset);

                    if (cursor == std::string_view::npos)
                        return Fail({ TokenKind::Invalid, {}, offset }, {});

                    continue;
                }
                if (c == '\r' || c == '\n')
                {
                    lineStart = cursor;
                    continue;
                }

                // 行頭の独自 pragma はエントリー名と、後で空白化する範囲を記録する
                if (c == '#')
                {
                    // ReadTokenでコメントなどを読み飛ばし、offsetが一致する場合行頭にあると判定する
                    if (ReadToken(m_source, lineStart).m_offset != offset)
                        continue;

                    std::size_t stripEnd = 0;
                    if (!ReadPragma(offset, pass, stripEnd))
                        return false;

                    if (stripEnd != 0)
                    {
                        // HLSL先頭からの位置と、空白化する範囲の長さを記録
                        pragmaRanges.emplace_back(offset - bodyStart, stripEnd - offset);
                        cursor = stripEnd;
                    }
                }

                // 終了マーカーで始まるか判定
                if (!rest.starts_with(endMarker))
                    continue;

                // 終了マーカーの前後が単語の一部でないことを確認する
                const auto markerEnd = offset + endMarker.size();
                if ((offset != bodyStart && IsWordCharacter(m_source[offset - 1])) ||
                    (markerEnd < m_source.size() && IsWordCharacter(m_source[markerEnd])))
                    continue;

                // 本文を一括コピーし、改行を維持したまま抽出済みの pragma を空白にする
                out.assign(m_source.substr(bodyStart, offset - bodyStart));

                for (const auto& [start, count] : pragmaRanges)
                    std::fill_n(out.data() + start, count, ' ');

                m_token = ReadToken(m_source, markerEnd);   // 終了マーカーの直後から次の字句を読み込む
                return true;
            }

            return Fail({ TokenKind::End, {}, m_source.size() }, "expected '" + std::string(endMarker) + "'");
        }

        bool Parser::ReadPass(PendingPass& pass)
        {
            pass.m_start = m_token;
            Advance();

            // Pass 名は任意。名前付きの場合は識別子または文字列で受ける
            auto& name = pass.m_result.m_passDesc.m_info.m_name;
            if (m_token.m_kind == TokenKind::Quoted)
            {
                if (!ReadString(name))
                    return false;
            }
            else if (IsIdentifier(m_token.m_text))
            {
                name = m_token.m_text;
                Advance();
            }

            // Passの本体を読み取る
            if (!Expect("{"))
                return false;

            // HLSLとStateの明示指定を収集し、Shader全体の既定値は後で適用
            bool hasHlsl = false;
            while (!Consume("}"))
            {
                // PassにはHLSLブロックとStateのみが入る

                // Pass内のHLSLは1つだけ許可する
                if (m_token.m_text == "HLSL_BEGIN")
                {
                    if (hasHlsl)
                        return Fail(m_token, "duplicate HLSL block in Pass");

                    hasHlsl = true;

                    if (!ReadHlsl("HLSL_END", pass.m_result.m_hlsl, &pass.m_result))
                        return false;
                }
                else if (!ReadSetting(pass.m_overrides))
                    return false;
            }

            return hasHlsl || Fail(pass.m_start, "Pass requires an HLSL block");
        }

        bool Parser::ResolvePass(const ParsedMaterialShaderPass& defaults, PendingPass& pass)
        {
            // Shader 共通の既定値をコピーし、Pass 固有の値を優先
            auto& result = pass.m_result;
            result.m_passDesc.m_pipeline = defaults.m_passDesc.m_pipeline;
            result.m_passDesc.m_info.m_renderType = defaults.m_passDesc.m_info.m_renderType;
            result.m_passDesc.m_info.m_renderPhase = defaults.m_passDesc.m_info.m_renderPhase;
            result.m_shadingModel = defaults.m_shadingModel;

            for (const auto& setting : pass.m_overrides)
            {
                if (!ApplySetting(setting, result))
                    return false;
            }

            // 出力先に応じて必要なエントリーポイントを検証

            // VertexShaderは必須
            if (result.m_vertexEntry.empty())
                return Fail(pass.m_start, "Pass requires '#pragma vertex'");

            // GBufferはSurfaceShaderが必須で、PixelShaderは不可
            // ForwardはPixelShaderが必須でSurfaceShaderは不可
            if (result.m_passDesc.m_info.m_renderType == RenderType::GBuffer)
            {
                if (result.m_surfaceEntry.empty() || !result.m_pixelEntry.empty())
                    return Fail(pass.m_start, "GBuffer Pass requires '#pragma surface' as its pixel entry");
            }
            else if (result.m_pixelEntry.empty() || !result.m_surfaceEntry.empty())
                return Fail(pass.m_start, "Forward Pass requires '#pragma pixel' as its pixel entry");
            return true;
        }

        bool Parser::Parse(ParsedMaterialShader& shader)
        {
            // ファイル全体を囲む MaterialShader 宣言と名前を読み取る
            /* 構文例
            * 
               MaterialShader "MyShader"
               {
                   Properties
                   {
                       MyFloat("My Float", Float) = 1.0;
                       MyColor("My Color", Color) = (1.0, 1.0, 1.0, 1.0);
                   }
                   Fill = Solid;
                   Cull = Back;
                   Blend = None;


                   SHARED_HLSL_BEGIN
                   // 共通の HLSL コード
                   // include や struct 宣言など、Pass 間で共有するコードを記述する
                   // ここで宣言した関数や構造体は、コンパイル時に先頭に挿入されるため、Pass 内の HLSL から参照できる
                   SHARED_HLSL_END

                   Pass "MyPass"
                   {
                       RenderType = GBuffer;
                       RenderPhase = Opaque;
                       ShadingModel = MyShadingModel;

                       Topology = Triangle;
                       ZWrite = On;
                       ZTest = LessEqual;

                       HLSL_BEGIN
                       // Pass 固有の HLSL コード
                       HLSL_END
                   }
               }

            */

            // MaterialShader "name" の解析
            if (!Expect("MaterialShader") || !ReadString(shader.m_name))
                return false;

            if (shader.m_name.empty())
                return Fail(m_token, "MaterialShader name is empty");

            // MaterialShader の本体を読み取る
            if (!Expect("{"))
                return false;

            // 全体設定は宣言位置によらず全 Pass に適用できるよう別に収集
            bool hasProperties = false;
            bool hasSharedHlsl = false;

            std::vector<StateAssignment> settings;  // Shader 全体の設定を収集
            std::vector<PendingPass> passes;        // Passの情報を保持し、後でShader全体の設定を適用する

            // 最後の } まで読み進め、Properties, SHARED_HLSL, Pass, RenderState の順序は問わない
            while (!Consume("}"))
            {
                const Token token = m_token;
                if (Consume("Properties"))  // Propertiesブロック
                {
                    // 1つだけ許可する
                    if (hasProperties)
                        return Fail(token, "duplicate Properties block");
                    hasProperties = true;
                    if (!ReadProperties(shader.m_properties))
                        return false;
                }
                else if (m_token.m_text == "SHARED_HLSL_BEGIN") // 共通HLSLブロック
                {
                    // 1つだけ許可する
                    if (hasSharedHlsl)
                        return Fail(token, "duplicate shared HLSL block");
                    hasSharedHlsl = true;
                    if (!ReadHlsl("SHARED_HLSL_END", shader.m_sharedHlsl, nullptr))
                        return false;
                }
                else if (m_token.m_text == "Pass")  // Passブロック
                {
                    PendingPass pass;
                    if (!ReadPass(pass))
                        return false;

                    passes.push_back(std::move(pass));
                }
                else if (!ReadSetting(settings))    // 全体のState設定
                    return false;
            }

            // }の後は空白・改行・タブかコメントしか許可しない
            if (m_token.m_kind != TokenKind::End)
                return Fail(m_token, "unexpected token after MaterialShader block");

            // Pass が1つもない場合はエラー
            if (passes.empty())
                return Fail(m_token, "MaterialShader requires at least one Pass");

            // 既定値と個別指定を統合し、宣言順で Pass を格納
            ParsedMaterialShaderPass defaults;
            defaults.m_shadingModel = "None";

            // 全体設定を既定値に適用
            for (const auto& setting : settings)
            {
                if (!ApplySetting(setting, defaults))
                    return false;
            }

            // Pass の個別指定を上書きし、最終的な Pass を格納
            for (auto& pass : passes)
            {
                if (!ResolvePass(defaults, pass))
                    return false;

                shader.m_passes.push_back(std::move(pass.m_result));
            }
            return true;
        }
    }

    bool ParseMaterialShader(std::string_view source, ParsedMaterialShader& out, std::string& error)
    {
        // BOM の有無を吸収し、解析完了後にだけ結果を公開
        if (source.starts_with("\xef\xbb\xbf"))
            source.remove_prefix(3);

        error.clear();
        ParsedMaterialShader parsed;

        if (!Parser(source, error).Parse(parsed))
            return false;

        out = std::move(parsed);
        return true;
    }
}
