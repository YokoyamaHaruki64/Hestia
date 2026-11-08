/*=============================================================================

 File   : DXMaterialShaderCompiler.cpp
 Desc   : DXC で MaterialShader をコンパイルし、DX12 Reflection を統合する。

------------------------------------------------------------------------------

 Date   : 2026/11/07
 Author : Yokoyama Haruki

=============================================================================*/

#include "pch.h"
#include "Graphics/DXMaterialShaderCompiler.h"

#include <d3d12shader.h>
#include <dxcapi.h>

#include "Asset/MaterialShaderParser.h"
#include "Graphics/DX12Object.h"
#include "Log/Logger.h"

#pragma comment(lib, "dxcompiler.lib")

namespace Hestia
{
    namespace
    {
        using Microsoft::WRL::ComPtr;

        // CPU Asset へ Bytecode をコピーするまで DXC の出力を保持
        struct CompiledShaderStage
        {
            ComPtr<IDxcBlob> m_bytecode;
            ComPtr<IDxcBlob> m_reflectionData;
            ComPtr<ID3D12ShaderReflection> m_reflection;
        };

        class DXCCompiler
        {
        public:
            bool Initialize()
            {
                if (m_initialized)
                    return true;

                // DXCUtilsはコンパイルオプションの構築とReflectionの生成に使用する
                if (FAILED(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(m_utils.GetAddressOf()))))
                {
                    Logger::Error("Failed to create DXC Utils instance");
                    return false;
                }

                // DXCCompiler3はIDxcCompilerの後継で、より柔軟なコンパイルオプションを提供する
                if (FAILED(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(m_compiler.GetAddressOf()))))
                {
                    Logger::Error("Failed to create DXC Compiler instance");
                    return false;
                }

                // デフォルトのIncludeHandlerを構築する
                if (FAILED(m_utils->CreateDefaultIncludeHandler(m_includeHandler.GetAddressOf())))
                {
                    Logger::Error("Failed to create DXC Include Handler");
                    return false;
                }

                m_initialized = true;
                return true;

            }

            bool Compile(
                const std::filesystem::path& path,
                std::string_view source,
                const std::string& entry,
                const wchar_t* profile, // "vs_6_0" or "ps_6_0"
                CompiledShaderStage& stage
            )
            {

                // entrypointとincludeDirをワイド文字列に変換
                const std::wstring entryPoint(entry.begin(), entry.end());
                const std::wstring includeDir = path.parent_path().native();

                // コンパイルフラグ
                // -Zpr : 行列をrow-majorにする
                // -I   : includeDir を指定する
                LPCWSTR flags[] = { L"-Zpr", L"-I", includeDir.c_str() };

                // コンパイルオプションを構築
                ComPtr<IDxcCompilerArgs> args;
                if (FAILED(m_utils->BuildArguments(
                    path.c_str(),
                    entryPoint.c_str(),
                    profile,                                        
                    flags, static_cast<UINT32>(std::size(flags)),   // flags
                    nullptr, 0,                                     // defines
                    args.GetAddressOf())))
                    return false;

                const DxcBuffer input{ source.data(), source.size(), DXC_CP_UTF8 };
                ComPtr<IDxcResult> result;

                // コンパイル実行
                if (FAILED(m_compiler->Compile(
                    &input,
                    args->GetArguments(),
                    args->GetCount(),
                    m_includeHandler.Get(),
                    IID_PPV_ARGS(result.GetAddressOf()))))
                {
                    Logger::Error("Failed to invoke DXC Compiler");
                    return false;
                }

                // コンパイル結果のステータスと診断メッセージを取得
                HRESULT status;
                if (FAILED(result->GetStatus(&status)))
                {
                    Logger::Error("Failed to get DXC compilation status");
                    return false;
                }

                // 診断メッセージをログに出力する。エラーがあれば失敗として返す
                ComPtr<IDxcBlobUtf8> diagnostics;
                if (SUCCEEDED(
                    result->GetOutput(DXC_OUT_ERRORS,
                    IID_PPV_ARGS(diagnostics.GetAddressOf()), nullptr)) &&
                    diagnostics && diagnostics->GetStringLength() > 0)
                {
                    const std::string_view message(diagnostics->GetStringPointer(), diagnostics->GetStringLength());
                    if (FAILED(status))
                        Logger::Error(message);
                    else
                        Logger::Warning(message);
                }

                if (FAILED(status))
                    return false;

                // DXIL と DX12 Reflection を取り出し、Reflection はロード中だけ使用
                if (FAILED(result->GetOutput(DXC_OUT_OBJECT,
                    IID_PPV_ARGS(stage.m_bytecode.GetAddressOf()), nullptr)) ||
                    !stage.m_bytecode)
                {
                    Logger::Error("Failed to get DXC compiled bytecode");
                    return false;
                }

                if (FAILED(result->GetOutput(DXC_OUT_REFLECTION,
                    IID_PPV_ARGS(stage.m_reflectionData.GetAddressOf()), nullptr)) || !stage.m_reflectionData)
                {
                    Logger::Error("Failed to get DXC reflection data");
                    return false;
                }

                const DxcBuffer reflectionData{
                    stage.m_reflectionData->GetBufferPointer(), stage.m_reflectionData->GetBufferSize(), 0 };

                if (FAILED(m_utils->CreateReflection(
                    &reflectionData, IID_PPV_ARGS(stage.m_reflection.GetAddressOf()))))
                {
                    Logger::Error("Failed to create DX12 reflection from DXC output");
                    return false;
                }

                return true;
            }

        private:
            bool m_initialized = false;

            ComPtr<IDxcUtils> m_utils;
            ComPtr<IDxcCompiler3> m_compiler;
            ComPtr<IDxcIncludeHandler> m_includeHandler;

        };

        // --- ここまでコンパイラ ---

        bool FailMessage(std::string_view message)
        {
            Logger::Error(message);
            return false;
        }

        bool GeneratePropertyDeclarations(
            std::span<const ParsedMaterialShaderProperty> properties,
            std::string& declarations)
        {
            // 全 Stage へ同一の宣言を挿入し、空の CBuffer は生成しない
            std::string constants;
            uint32_t textureSlot = 0;

            for (const ParsedMaterialShaderProperty& property : properties)
            {
                // defaultValue が Texture の場合は SRV 宣言、そうでない場合は PerMaterial CBuffer に追加する
                if (std::holds_alternative<AssetHandle<Texture>>(property.m_desc.m_defaultValue))
                {
                    if (textureSlot >= MAX_MATERIAL_TEXTURE)
                        return false;

                    declarations += "Texture2D " + property.m_desc.m_name + " : register(t" +
                        std::to_string(textureSlot++) + ", space" +
                        std::to_string(MATERIAL_TEXTURE_REGISTER_SPACE) + ");\n";
                }
                else // CBufferに追加
                    constants += "    " + property.m_hlslType + " " + property.m_desc.m_name + ";\n";
            }

            // 定数バッファが空でなければ宣言を追加
            if (!constants.empty())
            {
                declarations += "cbuffer PerMaterial : register(b" +
                    std::to_string(MATERIAL_CONSTANT_BUFFER_REGISTER) + ")\n{\n" + constants + "};\n";
            }

            return true;
        }


        bool MergeMaterialReflection(ID3D12ShaderReflection& reflection,
            std::span<MaterialPropertyDesc> properties, uint32_t& constantBufferSize)
        {

            // Reflerction情報の取得
            D3D12_SHADER_DESC shaderDesc{};
            if (FAILED(reflection.GetDesc(&shaderDesc)))
                return FailMessage("Failed to get shader description");

            // BoundResources : リソースバインディングの数。SRV/UAV/CBuffer/Samplerなどが含まれる
            // CBufferのレジスタと、SRVのレジスタがMaterialShaderのPropertyに対応するかを検証する
            for (UINT index = 0; index < shaderDesc.BoundResources; ++index)
            {
                D3D12_SHADER_INPUT_BIND_DESC binding{};
                if (FAILED(reflection.GetResourceBindingDesc(index, &binding)))
                    return FailMessage("Failed to get resource binding description");

                const std::string_view name(binding.Name);

                // PerMaterial CBuffer のレジスタが一致するか確認
                if (binding.Type == D3D_SIT_CBUFFER)
                {
                    const bool isMaterialSlot =
                        binding.Space == 0 && binding.BindPoint == MATERIAL_CONSTANT_BUFFER_REGISTER;

                    if (isMaterialSlot != (name == "PerMaterial"))
                        return FailMessage("PerMaterial CBuffer register mismatch");
                }

                // TEXTURE以外、または MATERIAL_TEXTURE_REGISTER_SPACE 以外のSRVは無視
                if (binding.Type != D3D_SIT_TEXTURE || binding.Space != MATERIAL_TEXTURE_REGISTER_SPACE)
                    continue;

                // TexturePropertyの整合性チェック
                // チェック内容はエラーメッセージ参照
                const auto property = std::ranges::find(properties, name, &MaterialPropertyDesc::m_name);
                if (property == properties.end())
                    return FailMessage("Texture resource [ " + std::string(name) + " ] not found in MaterialShader properties");

                if (!std::holds_alternative<AssetHandle<Texture>>(property->m_defaultValue))
                    return FailMessage("Property [ " + std::string(name) + " ] is not a Texture");

                if (binding.Dimension != D3D_SRV_DIMENSION_TEXTURE2D)
                    return FailMessage("Property [ " + std::string(name) + " ] is not a 2D Texture");

                if (binding.BindCount != 1)
                    return FailMessage("Property [ " + std::string(name) + " ] is an array, which is not supported");

                if (binding.BindPoint >= MAX_MATERIAL_TEXTURE)
                    return FailMessage("Property [ " + std::string(name) + " ] has a bind point that is out of bounds");

                if (property->m_resourceSlot != UINT32_MAX && property->m_resourceSlot != binding.BindPoint)
                    return FailMessage("Property [ " + std::string(name) + " ] has a bind point that does not match the existing slot");

                property->m_resourceSlot = binding.BindPoint;
            }

            // CBufferのサイズと各PropertyのOffset/Size/行列方向を統合する

            // 未使用の CBuffer は Reflection に含まれないため、
            // GetResourceBindingDescByName("PerMaterial"...)が失敗した場合も成功扱い
            D3D12_SHADER_INPUT_BIND_DESC materialBinding{};
            if (FAILED(reflection.GetResourceBindingDescByName("PerMaterial", &materialBinding)))
                return true;

            // ConstantBufferの取得
            auto* buffer = reflection.GetConstantBufferByName("PerMaterial");
            D3D12_SHADER_BUFFER_DESC bufferDesc{};
            if (FAILED(buffer->GetDesc(&bufferDesc)))
                return FailMessage("Failed to get PerMaterial constant buffer description");

            // ConstantBufferのサイズを統合
            constantBufferSize = std::max(constantBufferSize, bufferDesc.Size);

            // 最初に使われた Stage の Offset／Size／行列方向を記録し、以降は一致を検証
            for (UINT index = 0; index < bufferDesc.Variables; ++index)
            {
                // 変数の取得
                auto* variable = buffer->GetVariableByIndex(index);
                D3D12_SHADER_VARIABLE_DESC variableDesc{};
                if (FAILED(variable->GetDesc(&variableDesc)))
                    return FailMessage("Failed to get variable description for PerMaterial constant buffer");

                // D3D_SVF_USED が立っていない変数をスキップする
                if ((variableDesc.uFlags & D3D_SVF_USED) == 0)
                    continue;

                // 整合性チェック
                // offsetやsizeをUINT32_MAXや0で比較するのは、初期値のまま=未使用と判断し、失敗とみなさないため

                const auto property = std::ranges::find(
                    properties, std::string_view(variableDesc.Name), &MaterialPropertyDesc::m_name);

                if (property == properties.end())
                    return FailMessage("Property [" + std::string(variableDesc.Name) + "] not found in MaterialShader properties");

                D3D12_SHADER_TYPE_DESC type{};
                if (FAILED(variable->GetType()->GetDesc(&type)))
                    return FailMessage("Failed to get type description for property [" + std::string(variableDesc.Name) + "]");

                if(property->m_offset != UINT32_MAX && property->m_offset != variableDesc.StartOffset)
                    return FailMessage("Property [" + std::string(variableDesc.Name) + "] has an offset that does not match the existing offset");

                if (property->m_size != 0 && property->m_size != variableDesc.Size)
                    return FailMessage("Property [" + std::string(variableDesc.Name) + "] has a size that does not match the existing size");

                const bool rowMajor = type.Class == D3D_SVC_MATRIX_ROWS;
                if (property->m_size != 0 &&
                    property->m_offset != UINT32_MAX &&
                    property->m_rowMajor != rowMajor)
                    return FailMessage("Property [" + std::string(variableDesc.Name) + "] has a row_major setting that does not match the existing setting");

                // 更新
                property->m_offset = variableDesc.StartOffset;
                property->m_size = variableDesc.Size;
                property->m_rowMajor = rowMajor;
            }

            return true;
        }

        bool CollectRequiredVertexStreams(ID3D12ShaderReflection& reflection, std::vector<VertexStream>& streams)
        {
            struct MeshInput
            {
                const char* m_name;
                BYTE m_mask;            // ComponentMaskと比較 要はいくつの数値で構成されるか。最大4つなので0b1111で表現
                VertexStream m_stream;
            };

            static constexpr MeshInput meshInputs[] = {
                { "POSITION", 0b0111, VertexStream::Position },
                { "NORMAL",   0b0111, VertexStream::NormalTangent },
                { "TANGENT",  0b1111, VertexStream::NormalTangent },
                { "TEXCOORD", 0b0011, VertexStream::PrimaryTexCoord },
                { "COLOR",    0b1111, VertexStream::Color }
            };

            // Reflection の入力を実際の Mesh Stream の形式と対応付ける
            D3D12_SHADER_DESC desc{};
            if (FAILED(reflection.GetDesc(&desc)))
                return FailMessage("Failed to get shader description for required vertex streams");

            // VS 入力を検証し、対応する Stream を集める
            for (UINT index = 0; index < desc.InputParameters; ++index)
            {
                // 入力パラメータの取得
                D3D12_SIGNATURE_PARAMETER_DESC parameter{};
                if (FAILED(reflection.GetInputParameterDesc(index, &parameter)))
                    return FailMessage("Failed to get input parameter description for required vertex streams");

                // SystemValueTypeが定義されている場合はSV_Positionなどのシステム値であり、MeshStreamとは対応しない
                if (parameter.SystemValueType != D3D_NAME_UNDEFINED)
                    continue;

                const MeshInput* input = nullptr;
                for (const auto& candidate : meshInputs)
                {
                    // SemanticNameが一致するか確認(大文字小文字を区別しない)
                    if (_stricmp(parameter.SemanticName, candidate.m_name) == 0)
                    {
                        input = &candidate;
                        break;
                    }
                }

                if (!input)
                    return FailMessage("Vertex input semantic [ " + std::string(parameter.SemanticName) + " ] is not supported");

                if (parameter.SemanticIndex != 0)
                    return FailMessage("Vertex input semantic [ " + std::string(parameter.SemanticName) + " ] has a non-zero semantic index, which is not supported");

                if (parameter.ComponentType != D3D_REGISTER_COMPONENT_FLOAT32)
                    return FailMessage("Vertex input semantic [ " + std::string(parameter.SemanticName) + " ] has a component type that is not float32, which is not supported");

                if (parameter.Mask != input->m_mask)
                    return FailMessage("Vertex input semantic [ " + std::string(parameter.SemanticName) + " ] has a mask that does not match the expected format");

                // NORMAL と TANGENT はどちらも同じ Stream を要求する
                if (std::ranges::find(streams, input->m_stream) == streams.end())
                    streams.push_back(input->m_stream);
            }

            return true;
        }

        void CopyBytecode(IDxcBlob& bytecode, std::vector<std::byte>& out)
        {
            const auto* begin = static_cast<const std::byte*>(bytecode.GetBufferPointer());
            out.assign(begin, begin + bytecode.GetBufferSize());
        }

    }

    bool DXMaterialShaderCompiler::Compile(const std::filesystem::path& sourcePath,
        const ParsedMaterialShader& parsed, Result& out, std::string& error)
    {
        // エラー報告用のラムダ。呼び出し元に false を返す
        const auto fail = [&error](std::string_view message)
        {
            error = message;
            return false;
        };

        // コンパイル前に対応している Pass の形式か確認する
        for (const ParsedMaterialShaderPass& pass : parsed.m_passes)
        {
            // GBuffer Pass はまだ実装していない
            if (pass.m_passDesc.m_info.m_renderType == RenderType::GBuffer)
                return fail("GBuffer Pass compilation is not implemented");

            // Forward PassはShadingModelをサポートしていない
            if (pass.m_shadingModel != "None")
                return fail("Forward Pass does not support ShadingModel");
        }

        // 共通Property宣言とSHARED_HLSLを各Passへ追加する
        std::string declarations;
        if (!GeneratePropertyDeclarations(parsed.m_properties, declarations))
            return fail("too many material textures for the DX12 root signature");

        const std::string sharedSource = declarations + parsed.m_sharedHlsl + "\n";
        Result compiled;
        compiled.m_properties.reserve(parsed.m_properties.size());
        for (const ParsedMaterialShaderProperty& property : parsed.m_properties)
            compiled.m_properties.push_back(property.m_desc);

        static DXCCompiler compiler;
        if (!compiler.Initialize())
            return fail("could not initialize DXC");

        // Pass ごとに VS／PS をコンパイルし、ReflectionをShader共通レイアウトに統合する
        compiled.m_passes.reserve(parsed.m_passes.size());
        for (const ParsedMaterialShaderPass& pass : parsed.m_passes)
        {
            const std::string passSource = sharedSource + pass.m_hlsl;
            CompiledShaderStage vertex;
            CompiledShaderStage pixel;
            if (!compiler.Compile(sourcePath, passSource, pass.m_vertexEntry, L"vs_6_0", vertex) ||
                !compiler.Compile(sourcePath, passSource, pass.m_pixelEntry, L"ps_6_0", pixel))
                return fail("could not compile or reflect Pass '" + pass.m_passDesc.m_info.m_name + "'");

            MaterialShaderPass passDesc = pass.m_passDesc;
            auto& pipeline = passDesc.m_pipeline;
            if (!CollectRequiredVertexStreams(*vertex.m_reflection.Get(), pipeline.m_requiredVertexStreams))
                return fail("Pass vertex inputs do not match the Mesh streams");

            if (!MergeMaterialReflection(*vertex.m_reflection.Get(), compiled.m_properties, compiled.m_constantBufferSize) ||
                !MergeMaterialReflection(*pixel.m_reflection.Get(), compiled.m_properties, compiled.m_constantBufferSize))
                return fail("incompatible material bindings or constant buffer layout");

            CopyBytecode(*vertex.m_bytecode.Get(), pipeline.m_vertexShader);
            CopyBytecode(*pixel.m_bytecode.Get(), pipeline.m_pixelShader);
            compiled.m_passes.push_back(std::move(passDesc));
        }

        // 全Passのコンパイルに成功してから結果を返す
        out = std::move(compiled);
        error.clear();
        return true;
    }
}
