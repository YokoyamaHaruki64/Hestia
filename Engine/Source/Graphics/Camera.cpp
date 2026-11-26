/*=============================================================================

 File   : Camera.cpp
 Desc   : Transform の姿勢から描画用の View を更新する。

------------------------------------------------------------------------------

 Date   : 2026/11/26
 Author : Yokoyama Haruki

=============================================================================*/

#include "pch.h"
#include "Graphics/Camera.h"

#include "Graphics/GraphicsSystem.h"
#include "Transform/TransformData.h"

namespace Hestia
{
    void CameraProcess::Run(std::size_t count, const Entity* entities,
        const TransformCache* transforms, CameraComponent* cameras)
    {
        for (std::size_t index = 0; index < count; ++index)
        {
            const auto& entity = entities[index];
            if (!entity.IsActive())
                continue;

            // World の位置と Quaternion の回転から拡縮を含まないカメラ姿勢を得る
            const auto& world = transforms[index].m_worldMatrix;
            const Vector3_SIMD eye(world[3][0], world[3][1], world[3][2]);
            const auto rotation = TransformAPIImpl::GetWorldRotation(entity.GetHandle());
            const auto up = Vector3_SIMD::Load(rotation.RotateVector(Vector3::Up()));
            auto target = eye;
            target += Vector3_SIMD::Load(rotation.RotateVector(Vector3::Forward()));

            // このフレームの描画で使用する行列を設定する
            auto& camera = cameras[index];
            camera.m_view = Matrix4x4_SIMD::LookAtLH(eye, target, up);
            GraphicsSystem::SetView(camera.m_view);
            GraphicsSystem::SetProj(camera.m_proj);
        }
    }
}
