/*=============================================================================

 File   : SceneAPI.cpp
 Desc   : Scene の共有 API を Engine の SceneSystem へ接続する。

------------------------------------------------------------------------------

 Date   : 2026/11/26
 Author : Yokoyama Haruki

=============================================================================*/

#include "pch.h"
#include "API/SceneAPI.h"
#include "Scene/SceneSystem.h"

namespace Hestia
{
    const SceneAPI& SceneAPI::Get() noexcept
    {
        static constexpr SceneAPI api
        {
            &SceneSystem::GetCurrentSceneName,
            &SceneSystem::GetCurrentScenePath,
            &SceneSystem::LoadScene,
        };
        return api;
    }
}
