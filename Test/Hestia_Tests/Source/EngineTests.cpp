/*=============================================================================

 File   : EngineTests.cpp
 Desc   : Engine API の生成・更新・破棄契約を検証する。

------------------------------------------------------------------------------

 Date   : 2026/10/07
 Author : Yokoyama Haruki

=============================================================================*/

#include <gtest/gtest.h>

#include "EngineAPI.h"

namespace
{
    TEST(EngineAPI, ExposesEveryLifecycleAndUpdateFunction)
    {
        const Hestia::EngineAPI* engineAPI = GetEngineAPI();

        ASSERT_NE(engineAPI, nullptr);
        EXPECT_NE(engineAPI->Create, nullptr);
        EXPECT_NE(engineAPI->Destroy, nullptr);
        EXPECT_NE(engineAPI->ProcessMessage, nullptr);
        EXPECT_NE(engineAPI->FixedUpdate, nullptr);
        EXPECT_NE(engineAPI->FrameExecute, nullptr);
    }

    TEST(EngineAPI, CreateRejectsNullApplicationAPI)
    {
        const Hestia::EngineAPI* engineAPI = GetEngineAPI();
        ASSERT_NE(engineAPI, nullptr);
        EXPECT_EQ(engineAPI->Create(nullptr), nullptr);
    }

    TEST(EngineAPI, CreatedEngineAcceptsCallbacksAndCanBeDestroyed)
    {
        Hestia::ApplicationAPI applicationAPI;
        const Hestia::EngineAPI* engineAPI = GetEngineAPI();
        ASSERT_NE(engineAPI, nullptr);
        Hestia::EngineHandle engine = engineAPI->Create(&applicationAPI);
        ASSERT_NE(engine, nullptr);

        engineAPI->ProcessMessage(engine, nullptr, WM_NULL, 0, 0);
        engineAPI->FixedUpdate(engine, 1.0f / 60.0f);
        engineAPI->FrameExecute(engine, 1.0f / 60.0f);
        engineAPI->Destroy(engine);
    }

    TEST(EngineAPI, NullHandleIsIgnoredByNonCreatingFunctions)
    {
        const Hestia::EngineAPI* engineAPI = GetEngineAPI();
        ASSERT_NE(engineAPI, nullptr);
        engineAPI->ProcessMessage(nullptr, nullptr, WM_NULL, 0, 0);
        engineAPI->FixedUpdate(nullptr, 0.0f);
        engineAPI->FrameExecute(nullptr, 0.0f);
        engineAPI->Destroy(nullptr);
    }

    TEST(EngineAPI, EngineCanBeRecreatedAfterDestruction)
    {
        Hestia::ApplicationAPI applicationAPI;
        const Hestia::EngineAPI* engineAPI = GetEngineAPI();
        ASSERT_NE(engineAPI, nullptr);
        Hestia::EngineHandle firstEngine = engineAPI->Create(&applicationAPI);
        ASSERT_NE(firstEngine, nullptr);

        engineAPI->Destroy(firstEngine);

        Hestia::EngineHandle secondEngine = engineAPI->Create(&applicationAPI);
        ASSERT_NE(secondEngine, nullptr);
        engineAPI->FixedUpdate(secondEngine, 1.0f / 60.0f);
        engineAPI->Destroy(secondEngine);
    }
}
