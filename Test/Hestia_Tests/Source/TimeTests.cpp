/*=============================================================================

 File   : TimeTests.cpp
 Desc   : TimeSystem と TimeAPI の公開契約を検証する。

------------------------------------------------------------------------------

 Date   : 2026/10/07
 Author : Yokoyama Haruki

=============================================================================*/

#include <gtest/gtest.h>

#include <limits>
#include <memory>
#include <type_traits>

#include "Time/TimeSystem.h"
#include "Time/TimeAPI.h"
#include "EngineAPI.h"

namespace
{
    TEST(TimeSystem, AccumulatesScaledAndUnscaledFrameTime)
    {
        RecordProperty("target", "TimeSystem::Update / TimeSystem::SetTimeScale");
        Hestia::TimeSystem time;
        time.Initialize();

        time.Update(0.5f);
        time.SetTimeScale(2.0f);
        time.Update(0.25f);

        EXPECT_FLOAT_EQ(Hestia::TimeSystem::DeltaTime(), 0.5f);
        EXPECT_FLOAT_EQ(Hestia::TimeSystem::UnscaledDeltaTime(), 0.25f);
        EXPECT_FLOAT_EQ(Hestia::TimeSystem::TotalTime(), 1.0f);
        EXPECT_FLOAT_EQ(Hestia::TimeSystem::UnscaledTotalTime(), 0.75f);
        time.Finalize();
    }

    TEST(TimeSystem, FixedUpdatesDoNotAccumulateOrReplaceFrameTime)
    {
        RecordProperty("target", "TimeSystem::UpdateFixed");
        Hestia::TimeSystem time;
        time.Initialize();
        time.SetTimeScale(0.5f);
        time.Update(0.5f);
        time.UpdateFixed(0.125f);
        time.UpdateFixed(0.125f);

        EXPECT_FLOAT_EQ(Hestia::TimeSystem::FixedDeltaTime(), 0.0625f);
        EXPECT_FLOAT_EQ(Hestia::TimeSystem::UnscaledFixedDeltaTime(), 0.125f);
        EXPECT_FLOAT_EQ(Hestia::TimeSystem::DeltaTime(), 0.25f);
        EXPECT_FLOAT_EQ(Hestia::TimeSystem::UnscaledDeltaTime(), 0.5f);
        EXPECT_FLOAT_EQ(Hestia::TimeSystem::TotalTime(), 0.25f);
        EXPECT_FLOAT_EQ(Hestia::TimeSystem::UnscaledTotalTime(), 0.5f);
        time.Finalize();
    }

    TEST(TimeSystem, ZeroScalePausesAndCanResume)
    {
        RecordProperty("target", "TimeSystem::SetTimeScale / TimeSystem::Update / TimeSystem::UpdateFixed");
        Hestia::TimeSystem time;
        time.Initialize();
        time.Update(0.5f);
        time.SetTimeScale(0.0f);
        time.Update(0.25f);
        time.UpdateFixed(0.125f);

        EXPECT_FLOAT_EQ(Hestia::TimeSystem::DeltaTime(), 0.0f);
        EXPECT_FLOAT_EQ(Hestia::TimeSystem::FixedDeltaTime(), 0.0f);
        EXPECT_FLOAT_EQ(Hestia::TimeSystem::UnscaledDeltaTime(), 0.25f);
        EXPECT_FLOAT_EQ(Hestia::TimeSystem::UnscaledFixedDeltaTime(), 0.125f);
        EXPECT_FLOAT_EQ(Hestia::TimeSystem::TotalTime(), 0.5f);
        EXPECT_FLOAT_EQ(Hestia::TimeSystem::UnscaledTotalTime(), 0.75f);

        time.SetTimeScale(1.0f);
        time.Update(0.25f);
        time.UpdateFixed(0.125f);

        EXPECT_FLOAT_EQ(Hestia::TimeSystem::TotalTime(), 0.75f);
        EXPECT_FLOAT_EQ(Hestia::TimeSystem::UnscaledTotalTime(), 1.0f);
        EXPECT_FLOAT_EQ(Hestia::TimeSystem::FixedDeltaTime(), 0.125f);
        time.Finalize();
    }

    TEST(TimeSystem, InvalidScalePreservesCurrentScale)
    {
        RecordProperty("target", "TimeSystem::SetTimeScale");
        Hestia::TimeSystem time;
        time.Initialize();
        time.SetTimeScale(0.5f);

        const float invalidScales[] = {
            -1.0f,
            std::numeric_limits<float>::quiet_NaN(),
            std::numeric_limits<float>::infinity(),
            -std::numeric_limits<float>::infinity()
        };

        for (float scale : invalidScales)
        {
            time.SetTimeScale(scale);
            EXPECT_FLOAT_EQ(Hestia::TimeSystem::TimeScale(), 0.5f);
        }

        time.Update(0.5f);
        EXPECT_FLOAT_EQ(Hestia::TimeSystem::DeltaTime(), 0.25f);
        time.Finalize();
    }

    TEST(TimeSystem, FinalizeAndInitializeResetTimeAndRegisterInstance)
    {
        RecordProperty("target", "TimeSystem::Finalize / TimeSystem::Initialize");
        static_assert(!std::is_copy_constructible_v<Hestia::TimeSystem>);
        static_assert(!std::is_move_constructible_v<Hestia::TimeSystem>);
        {
            Hestia::TimeSystem time;
            time.Initialize();
            time.SetTimeScale(2.0f);
            time.Update(0.5f);
            time.UpdateFixed(0.125f);
            time.Finalize();
        }

        Hestia::TimeSystem time;
        time.Initialize();
        EXPECT_FLOAT_EQ(Hestia::TimeSystem::TimeScale(), 1.0f);
        EXPECT_FLOAT_EQ(Hestia::TimeSystem::TotalTime(), 0.0f);
        EXPECT_FLOAT_EQ(Hestia::TimeSystem::FixedDeltaTime(), 0.0f);
        time.Update(0.25f);
        EXPECT_FLOAT_EQ(Hestia::TimeSystem::TotalTime(), 0.25f);
        time.Finalize();
    }

    TEST(TimeAPI, ReadsLiveSystemDataAndDelegatesScaleChanges)
    {
        RecordProperty("target", "TimeAPI::Initialize / TimeAPI getters / TimeAPI::SetTimeScale");
        Hestia::TimeSystem time;
        time.Initialize();
        Hestia::TimeAPI api;
        api.Initialize(&time);
        const Hestia::TimeData& data = time.GetData();

        api.SetTimeScale(2.0f);
        time.Update(0.25f);
        time.UpdateFixed(0.125f);

        EXPECT_EQ(&data, &time.GetData());
        EXPECT_FLOAT_EQ(api.TimeScale(), 2.0f);
        EXPECT_FLOAT_EQ(api.DeltaTime(), 0.5f);
        EXPECT_FLOAT_EQ(api.UnscaledDeltaTime(), 0.25f);
        EXPECT_FLOAT_EQ(api.TotalTime(), 0.5f);
        EXPECT_FLOAT_EQ(api.UnscaledTotalTime(), 0.25f);
        EXPECT_FLOAT_EQ(api.FixedDeltaTime(), 0.25f);
        EXPECT_FLOAT_EQ(api.UnscaledFixedDeltaTime(), 0.125f);

        time.SetTimeScale(0.5f);
        time.Update(0.5f);
        EXPECT_FLOAT_EQ(api.TimeScale(), 0.5f);
        EXPECT_FLOAT_EQ(api.TotalTime(), 0.75f);
        EXPECT_FLOAT_EQ(data.m_totalTime, 0.75f);
        api.Finalize();
        time.Finalize();
    }

    TEST(TimeIntegration, EngineCallbacksUpdateOwnedClock)
    {
        RecordProperty("target", "Engine::FrameExecute / Engine::FixedUpdate");
        Hestia::ApplicationAPI applicationAPI;
        const Hestia::EngineAPI* engineAPI = GetEngineAPI();
        ASSERT_NE(engineAPI, nullptr);
        std::unique_ptr<void, void (*)(void*)> engine(
            engineAPI->Create(&applicationAPI), engineAPI->Destroy);
        ASSERT_NE(engine.get(), nullptr);

        engineAPI->FrameExecute(engine.get(), 0.5f);
        engineAPI->FixedUpdate(engine.get(), 0.125f);
        engineAPI->FixedUpdate(engine.get(), 0.125f);

        EXPECT_FLOAT_EQ(Hestia::TimeSystem::DeltaTime(), 0.5f);
        EXPECT_FLOAT_EQ(Hestia::TimeSystem::TotalTime(), 0.5f);
        EXPECT_FLOAT_EQ(Hestia::TimeSystem::FixedDeltaTime(), 0.125f);
    }
}
