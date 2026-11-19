/*=============================================================================

 File   : GameEntry.cpp
 Desc   : Game.dll の公開入口と GameRuntime の起動・終了を定義する。

------------------------------------------------------------------------------

 Date   : 2026/11/18
 Author : Yokoyama Haruki

=============================================================================*/

#include "pch.h"

#include "GameRuntime.h"
#include "GameSetup.h"

extern "C" HESTIA_GAME_API const Hestia::GameRuntimeAPI* GetGameRuntimeAPI()
{
    using HestiaGame::Detail::GameRuntime;
    using HestiaGame::Detail::ScriptPhase;

    static const Hestia::GameRuntimeAPI api{
        .Create = [](const Hestia::GameEngineAPI* engineAPI) -> void*
        {
            auto* runtime = GameRuntime::Create(engineAPI, &HestiaGame::Setup::RegisterScripts);
            if (runtime == nullptr)
                return nullptr;

            // 初期配置に失敗した場合は、追加済みの Script を含めて解放する
            if (!HestiaGame::Setup::SetupScene([runtime] { runtime->EndFrame(); }))
            {
                runtime->Finalize();
                delete runtime;
                return nullptr;
            }
            // 最後に追加した分も最初のフレームより前に反映する
            runtime->EndFrame();
            return runtime;
        },
        .Destroy = [](void* handle)
        {
            auto* runtime = static_cast<GameRuntime*>(handle);
            runtime->Finalize();
            delete runtime;
        },
        .FixedUpdate = [](void* handle) { static_cast<GameRuntime*>(handle)->Run(ScriptPhase::FixedUpdate); },
        .Update = [](void* handle) { static_cast<GameRuntime*>(handle)->Run(ScriptPhase::Update); },
        .LateUpdate = [](void* handle) { static_cast<GameRuntime*>(handle)->Run(ScriptPhase::LateUpdate); },
        .EndFrame = [](void* handle) { static_cast<GameRuntime*>(handle)->EndFrame(); },
    };
    return &api;
}
