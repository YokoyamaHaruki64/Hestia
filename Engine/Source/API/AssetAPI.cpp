/*=============================================================================

 File   : AssetAPI.cpp
 Desc   : AssetAPI の DLL 境界操作を AssetSystem へ委譲する。

------------------------------------------------------------------------------

 Date   : 2026/10/15
 Author : Yokoyama Haruki

=============================================================================*/

#include "pch.h"
#include "API/AssetAPI.h"

#include "Asset/AssetSystem.h"

namespace Hestia
{
    void AssetAPI::Initialize(AssetSystem* system)
    {
        if (system == nullptr)
        {
            Finalize();
            return;
        }

        // 共有ヘッダーから Engine 内の AssetSystem を呼ぶための入口を設定する
        m_mesh = { &AssetSystem::Load<Mesh>, &AssetSystem::Get<Mesh>, &AssetSystem::Register<Mesh> };
        m_texture = { &AssetSystem::Load<Texture>, &AssetSystem::Get<Texture>, &AssetSystem::Register<Texture> };
    }

    void AssetAPI::Finalize()
    {
        m_mesh = {};
        m_texture = {};
    }
}
