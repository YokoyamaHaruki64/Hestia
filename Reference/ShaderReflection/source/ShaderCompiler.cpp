/*=============================================================================

File   : [ShaderCompiler.cpp]
Desc   : シェーダーのコンパイルを行うクラスの実装

------------------------------------------------------------------------------

Date   : 2026/04/10
Author : Yokoyama Haruki

===============================================================================*/

#include "Graphics/Shader/ShaderCompiler.h"
#include "System/Diagnostics/Logger.h"

#include <d3dcompiler.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

bool ShaderCompiler::CompileShader(
    const std::string& hlslCode,
    const std::string& sourceName,
    const std::string& entryPoint,
    const std::string& shaderModel,
    ID3DBlob** outShaderBlob)
{
    ComPtr<ID3DBlob> errorBlob;

    HRESULT hr = D3DCompile(
        hlslCode.data(),
        hlslCode.size(),
        sourceName.c_str(), // 標準インクルードが相対パスを解決する基準として利用する
        nullptr, // マクロ定義は今回は使用しない
        D3D_COMPILE_STANDARD_FILE_INCLUDE, // #includeディレクティブを処理するためのフラグ
        entryPoint.c_str(),
        shaderModel.c_str(),
        D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
        0,
        outShaderBlob,
        errorBlob.GetAddressOf() // エラーが発生した場合、エラーメッセージがerrorBlobに格納される
    );

    if (FAILED(hr))
    {
        if (errorBlob)
        {
            std::string errorMessage(static_cast<const char*>(errorBlob->GetBufferPointer()), errorBlob->GetBufferSize());
            Logger::Error(errorMessage);
        }
        else
        {
            Logger::Error("シェーダーコンパイル中に詳細不明のエラーが発生しました。");
        }
        return false;
    }

    return true;
}

