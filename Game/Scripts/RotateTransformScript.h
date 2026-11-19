/*=============================================================================

 File   : RotateTransformScript.h
 Desc   : デバッグシーンの Entity を毎 Update で回転させる。

------------------------------------------------------------------------------

 Date   : 2026/11/19
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _ROTATE_TRANSFORM_SCRIPT_H_
#define _ROTATE_TRANSFORM_SCRIPT_H_

#include "Game/Include/World.h"

namespace HestiaGame
{
    class RotateTransformScript
    {
    public:
        void OnStart(Hestia::EntityHandle self)
        {
            // Facade を保持して毎フレームの取得を省く
            m_transform = World::GetProcess<Transform>(self);
        }

        void Update(Hestia::EntityHandle)
        {
            // MoveTransform と同じ順序で Z 軸の回転を合成する
            m_transform.SetLocalRotation(ROTATION_PER_UPDATE * m_transform.GetLocalRotation());
        }

    private:
        inline static const Quaternion ROTATION_PER_UPDATE = Quaternion::EulerToQuat({ 0.0f, 0.0f, 0.01f });

        Transform m_transform;
    };
}

#endif // _ROTATE_TRANSFORM_SCRIPT_H_
