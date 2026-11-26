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
        time += Time::DeltaTime();
        const float speed = Math::PI_DIV4;

        Vector3 cameraOffset = { std::sin(time * speed) * 5.0f, 1.0f, std::cos(time * speed) * 5.0f };
        Vector3 playerPosition = m_transform.GetLocalPosition(); 
        m_camera.SetLocalPosition(m_transform.GetLocalPosition() + cameraOffset);

        // プレイヤーへ向かう方向を向くようにカメラを回転させる
        Quaternion lookPlayer = Quaternion::LookRotation(playerPosition + Vector3(0.0f,1.5f, 0.0f) - m_camera.GetLocalPosition(), { 0.0f, 1.0f, 0.0f });
        m_camera.SetLocalRotation(lookPlayer);
    }
}
SOURCE_END
