HEADER_BEGIN
#include "Game/Include/World.h"

namespace HestiaGame
{
    class RotateTransformScript
    {
    public:
        void OnStart(Hestia::EntityHandle self);
        void Update(Hestia::EntityHandle);

    private:
        inline static const Quaternion ROTATION_PER_UPDATE = Quaternion::EulerToQuat({ 0.0f, 0.05f, 0.05f });
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
        time += Time::DeltaTime();
        const Vector3 deltaPos = Vector3 { 3.0f,3.0f, 3.0f } * Math::Sin(time * Math::PI_2);
        m_transform.SetLocalRotation(ROTATION_PER_UPDATE * m_transform.GetLocalRotation());
        m_transform.SetLocalPosition(deltaPos + m_transform.GetLocalPosition());
    }
}
SOURCE_END
