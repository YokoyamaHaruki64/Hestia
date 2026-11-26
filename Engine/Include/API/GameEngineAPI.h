/*=============================================================================

 File   : GameEngineAPI.h
 Desc   : Engine が GameRuntime に渡す共有 API を定義する。

------------------------------------------------------------------------------

 Date   : 2026/11/16
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _GAME_ENGINE_API_H_
#define _GAME_ENGINE_API_H_

#include "Common/Include/GameRuntimeAPI.h"
#include "ECSProcessAPI.h"

namespace Hestia
{
    class TimeAPI;
    class LogAPI;
    class AssetAPI;
    struct SceneAPI;
    struct InputAPI;

    /// @brief GameRuntime の生存中に利用できる Engine 所有 API の借用。
    struct GameEngineAPI
    {
        const TimeAPI* m_time = nullptr;
        const EntityAPI* m_entity = nullptr;
        const ECSProcessAPI* m_ecsProcess = nullptr;
        const LogAPI* m_log = nullptr;
        const AssetAPI* m_asset = nullptr;
        const SceneAPI* m_scene = nullptr;
        const InputAPI* m_input = nullptr;
    };
}

#endif // _GAME_ENGINE_API_H_
