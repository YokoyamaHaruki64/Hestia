HEADER_BEGIN
#include "Game/Include/World.h"
#include "Engine/Include/Game/Transform.h"

namespace HestiaGame
{
    class RotateTransformScript
    {
    public:
        void OnStart(Hestia::EntityHandle self);
        void Update(Hestia::EntityHandle);

    private:
        inline static const Quaternion ROTATION_PER_UPDATE = Quaternion::EulerToQuat({ 0.05f, 0.0f, 0.08f });
        Transform m_transform;
        float time = 0.0f;
    };
}
HEADER_END

SOURCE_BEGIN
#include "Game/Include/GameTime.h"
namespace HestiaGame
{
    void RotateTransformScript::OnStart(Hestia::EntityHandle self)
    {
        // Facadeを保持して毎フレームの解決を省く
        m_transform = World::GetProcess<Transform>(self);
    }

    void RotateTransformScript::Update(Hestia::EntityHandle)
    {
        m_transform.SetLocalRotation(ROTATION_PER_UPDATE * m_transform.GetLocalRotation());
    }
}
SOURCE_END
