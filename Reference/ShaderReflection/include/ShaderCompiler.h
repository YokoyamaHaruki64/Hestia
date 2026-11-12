/*=============================================================================

 File   : [ShaderCompiler.h]
 Desc   : シェーダーのコンパイルを行うクラスの宣言

 ------------------------------------------------------------------------------

 Date   : 2026/04/10
 Author : Yokoyama Haruki

===============================================================================*/

#ifndef _SHADER_COMPILER_H_
#define _SHADER_COMPILER_H_

#include <string>
#include <d3d11.h>

#pragma comment(lib, "d3dcompiler.lib")
class ShaderCompiler
{

public:
    ShaderCompiler() = default;
    ~ShaderCompiler() = default;

    bool CompileShader(
        const std::string& hlslCode,
        const std::string& sourceName,
        const std::string& entryPoint,
        const std::string& shaderModel,
        ID3DBlob** outShaderBlob);

};


#endif // _SHADER_COMPILER_H_
