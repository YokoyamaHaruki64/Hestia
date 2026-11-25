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
            // 型登録を完了し、初期配置はEngineの起動Sceneに委ねる
            return GameRuntime::Create(engineAPI, &HestiaGame::Setup::RegisterScripts);
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
        .QueryScriptAttachments = [](void* handle, const Hestia::EntityHandle* entities, size_t count,
            void* context, Hestia::ScriptAttachmentVisitor visitor)
        {
            return static_cast<GameRuntime*>(handle)->QueryScriptAttachments(entities, count, context, visitor);
        },
        .AttachSceneScript = [](void* handle, Hestia::EntityHandle entity, Hestia::AssetGuid guid)
        {
            return static_cast<GameRuntime*>(handle)->AttachSceneScript(entity, guid);
        },
        .FlushScripts = [](void* handle) { return static_cast<GameRuntime*>(handle)->FlushScripts(); },
        .ClearSceneScripts = [](void* handle) { static_cast<GameRuntime*>(handle)->ClearSceneScripts(); },
    };
    return &api;
}
