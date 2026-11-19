/*=============================================================================

 File   : GameEntry.cpp
 Desc   : Game.dll の公開入口とデバッグシーンの Script 初期配置を定義する。

------------------------------------------------------------------------------

 Date   : 2026/11/18
 Author : Yokoyama Haruki

=============================================================================*/

#include "pch.h"

#include "GameRuntime.h"
#include "World.h"

namespace
{
    const Quaternion ROTATION_PER_UPDATE = Quaternion::EulerToQuat({ 0.0f, 0.0f, 0.01f });

    class RotateTransformScript
    {
    public:
        void OnStart(Hestia::EntityHandle self)
        {
            // Facade を保持して毎フレームの取得を省く
            m_transform = HestiaGame::World::GetProcess<HestiaGame::Transform>(self);
        }

        void Update(Hestia::EntityHandle)
        {
            // MoveTransform と同じ順序で Z 軸の回転を合成する
            m_transform.SetLocalRotation(ROTATION_PER_UPDATE * m_transform.GetLocalRotation());
        }

    private:
        HestiaGame::Transform m_transform;
    };

    bool RegisterGameScripts()
    {
        return HestiaGame::World::RegisterScript<RotateTransformScript>();
    }

    bool AttachRotationScripts(HestiaGame::Detail::GameRuntime& runtime)
    {
        // 一括追加では保留中の Script の重複探索が積み上がるため、小分けに反映する
        constexpr std::size_t batchSize = 256;
        std::size_t attachedCount = 0;
        for (const auto entity : HestiaGame::World::FindEntitiesWithProcess<HestiaGame::Transform>())
        {
            if (!HestiaGame::World::AddScript<RotateTransformScript>(entity))
                return false;

            if (++attachedCount % batchSize == 0)
                runtime.EndFrame();
        }

        // 最後の端数も最初の Update より前に配置する
        runtime.EndFrame();
        return true;
    }
}

extern "C" HESTIA_GAME_API const Hestia::GameRuntimeAPI* GetGameRuntimeAPI()
{
    using HestiaGame::Detail::GameRuntime;
    using HestiaGame::Detail::ScriptPhase;

    static const Hestia::GameRuntimeAPI api{
        .Create = [](const Hestia::GameEngineAPI* engineAPI) -> void*
        {
            auto* runtime = GameRuntime::Create(engineAPI, &RegisterGameScripts);
            if (runtime == nullptr)
                return nullptr;

            // 初期配置に失敗した場合は、追加済みの Script を含めて解放する
            if (!AttachRotationScripts(*runtime))
            {
                runtime->Finalize();
                delete runtime;
                return nullptr;
            }
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
