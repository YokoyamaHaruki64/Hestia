HEADER_BEGIN
/*=============================================================================

 File   : Player.cpp
 Desc   : 仮のプレイヤーを実装

------------------------------------------------------------------------------

 Date   : 2026/11/26
 Author : Yokoyama Haruki

=============================================================================*/

#include "Game/Include/World.h"
#include "Engine/Include/Game/Transform.h"
#include "Engine/Include/Game/MeshRenderer.h"

namespace HestiaGame
{
    class Player
    {
    public:
        void OnStart(Hestia::EntityHandle self);
        void Update(Hestia::EntityHandle);

    private:
        Transform m_transform;
        Transform m_camera;
        float time = 0.0f;
    };
}
HEADER_END

SOURCE_BEGIN
#include "Game/Include/GameInput.h"
#include "Game/Include/GameTime.h"
#include "Game/Include/GameAssets.h"
namespace HestiaGame
{
    void Player::OnStart(Hestia::EntityHandle self)
    {
        m_transform = World::GetProcess<Transform>(self);
        auto camera = World::FindFirstEntityByName("Camera");
        m_transform.SetLocalPosition({ 0.0f, 0.0f, 0.0f });

        if (camera.IsValid())
        {
            m_camera = World::GetProcess<Transform>(camera);
        }
    }

    void Player::Update(Hestia::EntityHandle)
    {
        // 斜めでも移動速度が一定になるように XZ 平面の入力を正規化する
        const float deltaTime = Time::DeltaTime();
        const Vector3 direction{
            static_cast<float>(Input::KeyHold(Key::D)) - static_cast<float>(Input::KeyHold(Key::A)),
            0.0f,
            static_cast<float>(Input::KeyHold(Key::W)) - static_cast<float>(Input::KeyHold(Key::S)),
        };

        constexpr float moveSpeed = 50.0f;
        if (direction.LengthSqr() > 0.0f)
            m_transform.SetLocalPosition(m_transform.GetLocalPosition() + direction.Normalized() * (moveSpeed * deltaTime));

        if (!m_camera.IsValid())
            return;

        time += deltaTime;
        const float speed = Math::PI_DIV4;

        Vector3 cameraOffset = m_transform.GetLocalPosition() + Vector3(0, 1.5f, -5.0f);
        Vector3 playerPosition = m_transform.GetLocalPosition(); 
        m_camera.SetLocalPosition(cameraOffset);

        // プレイヤーへ向かう方向を向くようにカメラを回転させる
        Quaternion lookPlayer = Quaternion::LookRotation(playerPosition + Vector3(0.0f,1.5f, 0.0f) - m_camera.GetLocalPosition(), { 0.0f, 1.0f, 0.0f });
        m_camera.SetLocalRotation(lookPlayer);
    }
}
SOURCE_END
