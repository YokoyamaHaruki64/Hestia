/*=============================================================================

 File   : Camera.h
 Desc   : 描画用カメラの Component と更新 Process を定義する。

------------------------------------------------------------------------------

 Date   : 2026/11/26
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _CAMERA_H_
#define _CAMERA_H_

#include <cstddef>

#include "Common/Include/Math/Matrix4x4_SIMD.h"

namespace Hestia
{
    class Entity;
    struct TransformCache;

    /// @brief 描画に使う View と Projection を保持する。
    struct CameraComponent
    {
        Matrix4x4_SIMD m_view = Matrix4x4_SIMD::Identity();
        Matrix4x4_SIMD m_proj = Matrix4x4_SIMD::PerspectiveFovLH(0.8f, 1600.0f / 900.0f, 0.2f, 1600.0f);
    };

    class CameraProcess
    {
    public:
        /// @brief Transform 更新後に有効な Camera の View を更新し、Graphics へ渡す。
        static void Run(std::size_t count, const Entity* entities,
            const TransformCache* transforms, CameraComponent* cameras);
    };
}

#endif // _CAMERA_H_
