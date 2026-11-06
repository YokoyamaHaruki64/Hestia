/*=============================================================================

 File   : TransformFacadeConsumer.cpp
 Desc   : Engine import libraryを使う外部consumerでFacadeのDLL境界を検証する

------------------------------------------------------------------------------

 Date   : 2026/11/05
 Author : Yokoyama Haruki

=============================================================================*/

#include <crtdbg.h>
#include <cstdlib>
#include <cstring>
#include <iostream>

#include "Engine/Include/Game/Transform.h"

int main(int count, char** arguments)
{
    HestiaGame::Transform transform;
    if (count > 1 && std::strcmp(arguments[1], "--assert") == 0)
    {
        _set_error_mode(_OUT_TO_STDERR);
        _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
        _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
        _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
        transform.GetWorldMatrix();
        return 99;
    }

    if (transform.IsValid())
        return 1;

    // SIMD引数を実際にDLLへ渡す。ECS未接続なのでSetはfalseになる
    if (transform.SetWorldMatrix(Matrix4x4_SIMD::Identity()) ||
        transform.SetLocalPosition(Vector3::Zero()) ||
        transform.SetLocalRotation(Quaternion::Identity()) ||
        transform.SetLocalScale(Vector3::One()) ||
        transform.SetWorldPosition(Vector3::Zero()) ||
        transform.SetWorldRotation(Quaternion::Identity()) ||
        transform.SetWorldScale(Vector3::One()) ||
        transform.SetParent(HestiaGame::Transform{}))
        return 2;

#ifdef NDEBUG
    if (transform.GetLocalPosition() != Vector3::Zero() ||
        transform.GetWorldPosition() != Vector3::Zero() ||
        transform.GetLocalScale() != Vector3::One() ||
        transform.GetWorldScale() != Vector3::One() ||
        transform.GetLocalRotation().w != 1.0f || transform.GetWorldRotation().w != 1.0f ||
        transform.GetParent().IsValid())
        return 3;

    // SIMD戻り値とそのalignmentをconsumer側の数学型で読み取る
    const Matrix4x4_SIMD matrix = transform.GetWorldMatrix();
    for (size_t row = 0; row < 4; ++row)
        for (size_t column = 0; column < 4; ++column)
            if (matrix[row][column] != (row == column ? 1.0f : 0.0f))
                return 4;
#endif

    std::cout << "Transform facade DLL consumer passed\n";
    return 0;
}
