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
        m_transform.SetLocalScale({ 50.0f, 50.0f,50.0f });
    }

    void Player::Update(Hestia::EntityHandle)
    {
    }
}
SOURCE_END
