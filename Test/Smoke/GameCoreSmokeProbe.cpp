/*=============================================================================

 File   : GameCoreSmokeProbe.cpp
 Desc   : Game.dll 内の Script 実行と Facade の接続を記録する。

------------------------------------------------------------------------------

 Date   : 2026/11/13
 Author : Yokoyama Haruki

=============================================================================*/

#include <fstream>

#include "Game/Include/GameTime.h"
#include "Game/Include/GameSetup.h"
#include "Game/Include/World.h"

namespace
{
    using Hestia::EntityHandle;
    using HestiaGame::Time;
    using HestiaGame::Transform;
    using HestiaGame::World;

    const Vector3 PROBE_POSITION{ 1.0f, 2.0f, 3.0f };

    void WriteMarker(const char* marker)
    {
        // 実行中のランナーが読めるよう、記録ごとにファイルを閉じる
        std::ofstream log("GameCoreProbe.log", std::ios::app);
        log << marker << '\n';
    }

    class FrameProbe
    {
    public:
        void OnStart(EntityHandle self)
        {
            // DLL 内の Facade から Engine が所有する Transform を変更する
            auto transform = World::GetProcess<Transform>(self);
            if (transform.IsValid() && transform.SetLocalPosition(PROBE_POSITION))
                m_stage = Stage::FixedUpdate;
        }

        void FixedUpdate(EntityHandle)
        {
            if (m_stage == Stage::FixedUpdate && Time::UnscaledFixedDeltaTime() > 0.0f)
                m_stage = Stage::Update;
        }

        void Update(EntityHandle self)
        {
            if (m_stage != Stage::Update || Time::UnscaledDeltaTime() <= 0.0f)
                return;

            // 別のフェーズで Engine 側に残った値を読み戻す
            const auto transform = World::GetProcess<Transform>(self);
            if (transform.IsValid() && transform.GetLocalPosition() == PROBE_POSITION)
                m_stage = Stage::LateUpdate;
        }

        void LateUpdate(EntityHandle self)
        {
            if (m_stage != Stage::LateUpdate)
                return;

            // 予約から戻った後に OnDestroy が呼ばれた場合だけ EndFrame を確認できる
            World::DestroyEntity(self);
            m_stage = Stage::EndFrame;
        }

        void OnDestroy(EntityHandle)
        {
            if (m_stage == Stage::EndFrame)
                WriteMarker("Frame probe passed");
        }

    private:
        enum class Stage
        {
            Start,
            FixedUpdate,
            Update,
            LateUpdate,
            EndFrame,
        };

        Stage m_stage = Stage::Start;
    };

    struct ShutdownProbe
    {
        void OnDestroy(EntityHandle self)
        {
            // GameRuntime の終了処理中も Engine の Transform が有効か確かめる
            if (World::GetProcess<Transform>(self).IsValid())
                WriteMarker("Shutdown probe passed");
        }
    };

    bool RegisterSmokeScripts()
    {
        return HestiaGame::Setup::RegisterScripts() &&
            World::RegisterScript<FrameProbe>({ { 1, 0 } }) &&
            World::RegisterScript<ShutdownProbe>({ { 2, 0 } });
    }

    bool CreateSmokeScripts()
    {
        // 実行フェーズで破棄する Entity と Runtime の終了まで残す Entity を用意する
        const auto frameEntity = World::CreateEntity();
        const auto shutdownEntity = World::CreateEntity();
        return World::AddScript<FrameProbe>(frameEntity) && World::AddScript<ShutdownProbe>(shutdownEntity);
    }

    void* CreateSmokeGameRuntime(const Hestia::GameEngineAPI* engineAPI)
    {
        return HestiaGame::Detail::GameRuntime::Create(engineAPI, &RegisterSmokeScripts);
    }
}

// PE の GetGameRuntimeAPI はこの入口を公開し、静的ライブラリの入口は DLL 内に留める
extern "C" const Hestia::GameRuntimeAPI* GetSmokeGameRuntimeAPI()
{
    static const auto api = []
    {
        auto wrapped = *GetGameRuntimeAPI();
        wrapped.Create = &CreateSmokeGameRuntime;
        wrapped.FlushScripts = [](void* runtime)
        {
            // 起動Sceneが旧Worldを空にした後で、動作確認用のScriptを配置する
            static bool placed = false;
            if (!placed)
            {
                if (!CreateSmokeScripts())
                    return false;
                placed = true;
            }
            return GetGameRuntimeAPI()->FlushScripts(runtime);
        };
        return wrapped;
    }();
    return &api;
}
