/*=============================================================================

 File   : GameRuntimeTests.cpp
 Desc   : GameRuntime、Script、World の公開契約を検証する。

------------------------------------------------------------------------------

 Date   : 2026/11/13
 Author : Yokoyama Haruki

=============================================================================*/

#include <gtest/gtest.h>

#include <algorithm>
#include <memory>
#include <string_view>
#include <vector>

#include "Common/Include/GameRuntimeAPI.h"
#include "Engine/Include/Asset/AssetSystem.h"
#include "Engine/Include/ECS/ECSSystem.h"
#include "Engine/Include/Game/MeshRenderer.h"
#include "Engine/Include/Graphics/GraphicsSystem.h"
#include "Engine/Include/Graphics/MeshRenderer.h"
#include "Engine/Include/API/TimeAPI.h"
#include "Engine/Include/Time/TimeSystem.h"
#include "Game/Include/GameTime.h"
#include "Game/Include/World.h"

namespace
{
    using Hestia::ECSSystem;
    using Hestia::EntityHandle;
    using HestiaGame::Time;
    using HestiaGame::World;

    struct LifecycleScript
    {
        std::vector<std::string_view>* m_events;

        void OnEnable(EntityHandle) { m_events->push_back("enable"); }
        void OnDisable(EntityHandle) { m_events->push_back("disable"); }
        void OnStart(EntityHandle) { m_events->push_back("start"); }
        void FixedUpdate(EntityHandle) { m_events->push_back("fixed"); }
        void Update(EntityHandle) { m_events->push_back("update"); }
        void LateUpdate(EntityHandle) { m_events->push_back("late"); }
        void OnDestroy(EntityHandle) { m_events->push_back("destroy"); }
    };

    struct SpawnScript
    {
        EntityHandle m_target;
        EntityHandle m_victim;
        std::vector<std::string_view>* m_events;

        void FixedUpdate(EntityHandle self)
        {
            EXPECT_TRUE(World::AddScript<LifecycleScript>(m_target, m_events));
            EXPECT_TRUE(World::RemoveScript<LifecycleScript>(m_victim));
            EXPECT_TRUE(World::RemoveScript<SpawnScript>(self));
            m_events->push_back("spawned");
        }
    };

    struct ValueScript
    {
        int m_value;
    };

    struct UnregisteredScript
    {
    };

    struct RelocationScript
    {
        RelocationScript(int value, int& constructions) noexcept
            : m_value(value), m_constructions(&constructions)
        {
        }

        RelocationScript(RelocationScript&& other) noexcept
            : m_value(other.m_value), m_constructions(other.m_constructions)
        {
            ++*m_constructions;
        }

        RelocationScript& operator=(RelocationScript&&) noexcept = default;

        int m_value;
        int* m_constructions;
    };

    struct ReactivateScript
    {
        std::vector<std::string_view>* m_events;

        void OnDisable(EntityHandle self)
        {
            m_events->push_back("reactivating");
            EXPECT_TRUE(World::SetActive(self, true));
        }
        void OnEnable(EntityHandle) { m_events->push_back("reactivated"); }
    };

    struct DisableOnStartScript
    {
        int m_starts = 0;
        int m_updates = 0;

        void OnStart(EntityHandle self)
        {
            ++m_starts;
            EXPECT_TRUE(World::SetScriptEnabled<DisableOnStartScript>(self, false));
        }
        void Update(EntityHandle) { ++m_updates; }
    };

    struct DestroyScript
    {
        EntityHandle m_next;
        std::vector<EntityHandle>* m_destroyed;

        void Update(EntityHandle) { ADD_FAILURE() << "Destroy-reserved script was updated"; }

        void OnDestroy(EntityHandle self)
        {
            EXPECT_NE(ECSSystem::GetEntity(self), nullptr);
            m_destroyed->push_back(self);
            if (m_next.IsValid())
                World::DestroyEntity(m_next);
        }
    };

    class GameRuntimeTests : public testing::Test
    {
    protected:
        void SetUp() override
        {
            m_timeSystem.Initialize();
            m_time.Initialize(&m_timeSystem);
            ASSERT_TRUE(m_ecs.Initialize({ 1024 }));
            m_ecsInitialized = true;

            // Engine が Runtime に渡すものと同じ API で、描画環境なしに境界を検証する
            m_entityAPI = ECSSystem::CreateEntityAPI();
            m_engineAPI = { &m_time, &m_entityAPI, &m_ecs.GetProcessAPI() };
            m_runtimeAPI = GetGameRuntimeAPI();
            ASSERT_NE(m_runtimeAPI, nullptr);
            m_runtime = HestiaGame::Detail::GameRuntime::Create(&m_engineAPI, []
            {
                return World::RegisterScript<ReactivateScript>() &&
                    World::RegisterScript<SpawnScript>() &&
                    World::RegisterScript<LifecycleScript>() &&
                    World::RegisterScript<DisableOnStartScript>() &&
                    World::RegisterScript<ValueScript>() &&
                    World::RegisterScript<RelocationScript>() &&
                    World::RegisterScript<DestroyScript>();
            });
            ASSERT_NE(m_runtime, nullptr);
        }

        void TearDown() override
        {
            if (m_runtime != nullptr)
                m_runtimeAPI->Destroy(m_runtime);
            if (m_ecsInitialized)
                m_ecs.Finalize();
            m_time.Finalize();
            m_timeSystem.Finalize();
        }

        void EndFrame()
        {
            m_runtimeAPI->EndFrame(m_runtime);
            ECSSystem::EndFrame();
        }

        ECSSystem m_ecs;
        Hestia::TimeSystem m_timeSystem;
        Hestia::TimeAPI m_time;
        Hestia::EntityAPI m_entityAPI;
        Hestia::GameEngineAPI m_engineAPI;
        const Hestia::GameRuntimeAPI* m_runtimeAPI = nullptr;
        void* m_runtime = nullptr;
        bool m_ecsInitialized = false;
        std::vector<std::string_view> m_events;
        std::vector<EntityHandle> m_destroyed;
    };

    TEST(GameRuntimeAPI, RejectsMissingRequiredEngineAPIs)
    {
        const auto* api = GetGameRuntimeAPI();
        ASSERT_NE(api, nullptr);
        Hestia::GameEngineAPI incomplete{};
        EXPECT_EQ(api->Create(nullptr), nullptr);
        EXPECT_EQ(api->Create(&incomplete), nullptr);
    }

    TEST_F(GameRuntimeTests, TimeReadsEngineClockAndChangesScaleThroughCallback)
    {
        Time::SetTimeScale(2.0f);
        m_timeSystem.Update(0.25f);
        m_timeSystem.UpdateFixed(0.125f);

        EXPECT_FLOAT_EQ(Time::DeltaTime(), 0.5f);
        EXPECT_FLOAT_EQ(Time::FixedDeltaTime(), 0.25f);
        EXPECT_FLOAT_EQ(Time::TimeScale(), 2.0f);
    }

    TEST_F(GameRuntimeTests, ActiveAndEnabledAreIndependentAndStartRunsOnce)
    {
        const auto entity = World::CreateEntity();
        ASSERT_TRUE(entity.IsValid());
        EXPECT_FALSE(World::RegisterScript<UnregisteredScript>());
        auto script = World::AddScript<LifecycleScript>(entity, &m_events);
        ASSERT_TRUE(script);
        EXPECT_EQ(m_events, (std::vector<std::string_view>{ "enable", "start" }));
        EndFrame();

        ASSERT_TRUE(World::SetActive(entity, false));
        EXPECT_EQ(m_events, (std::vector<std::string_view>{ "enable", "start", "disable" }));
        ASSERT_TRUE(World::SetActive(entity, true));
        EXPECT_EQ(m_events, (std::vector<std::string_view>{ "enable", "start", "disable", "enable" }));

        ASSERT_TRUE(World::SetActive(entity, false));
        ASSERT_TRUE(World::SetScriptEnabled<LifecycleScript>(entity, false));
        ASSERT_TRUE(World::SetScriptEnabled<LifecycleScript>(entity, true));
        ASSERT_TRUE(World::SetScriptEnabled<LifecycleScript>(entity, false));
        m_runtimeAPI->Update(m_runtime);
        ASSERT_TRUE(World::SetActive(entity, true));
        m_runtimeAPI->Update(m_runtime);
        ASSERT_TRUE(World::SetScriptEnabled<LifecycleScript>(entity, true));
        m_runtimeAPI->FixedUpdate(m_runtime);
        m_runtimeAPI->Update(m_runtime);
        m_runtimeAPI->LateUpdate(m_runtime);

        ASSERT_TRUE(World::RemoveScript<LifecycleScript>(entity));
        EXPECT_FALSE(script);
        EndFrame();
        const std::vector<std::string_view> expected{
            "enable", "start", "disable", "enable", "disable", "enable", "fixed", "update", "late", "destroy"
        };
        EXPECT_EQ(m_events, expected);

        m_events.clear();
        ASSERT_TRUE(World::SetActive(entity, false));
        ASSERT_TRUE(World::AddScript<LifecycleScript>(entity, &m_events));
        EXPECT_EQ(m_events, (std::vector<std::string_view>{ "start" }));
        ASSERT_TRUE(World::SetActive(entity, true));
        EXPECT_EQ(m_events, (std::vector<std::string_view>{ "start", "enable" }));
    }

    TEST_F(GameRuntimeTests, ChangesDuringFixedUpdateOnlyAddUpdatesNextFrame)
    {
        const auto spawner = World::CreateEntity();
        const auto target = World::CreateEntity();
        const auto victim = World::CreateEntity();
        ASSERT_TRUE(World::AddScript<SpawnScript>(spawner, target, victim, &m_events));
        auto removed = World::AddScript<LifecycleScript>(victim, &m_events);
        ASSERT_TRUE(removed);
        EndFrame();
        m_events.clear();

        // 固定更新が複数回あっても、追加した Script は同じフレームで実行しない
        m_runtimeAPI->FixedUpdate(m_runtime);
        EXPECT_FALSE(removed);
        EXPECT_TRUE(World::GetScript<LifecycleScript>(target));
        m_runtimeAPI->FixedUpdate(m_runtime);
        m_runtimeAPI->Update(m_runtime);
        m_runtimeAPI->LateUpdate(m_runtime);
        EXPECT_EQ(m_events, (std::vector<std::string_view>{ "enable", "start", "destroy", "spawned" }));

        EndFrame();
        m_runtimeAPI->FixedUpdate(m_runtime);
        EXPECT_EQ(m_events, (std::vector<std::string_view>{ "enable", "start", "destroy", "spawned", "fixed" }));
    }

    TEST_F(GameRuntimeTests, NestedActiveChangesAreDeliveredToEveryScriptInOrder)
    {
        const auto entity = World::CreateEntity();
        ASSERT_TRUE(World::AddScript<ReactivateScript>(entity, &m_events));
        ASSERT_TRUE(World::AddScript<LifecycleScript>(entity, &m_events));
        m_events.clear();

        ASSERT_TRUE(World::SetActive(entity, false));
        EXPECT_TRUE(World::IsActive(entity));
        EXPECT_EQ(m_events, (std::vector<std::string_view>{ "reactivating", "disable", "reactivated", "enable" }));
    }

    TEST_F(GameRuntimeTests, OnStartCanDisableTheFirstUpdateWithoutRunningAgain)
    {
        const auto entity = World::CreateEntity();
        auto script = World::AddScript<DisableOnStartScript>(entity);
        ASSERT_TRUE(script);
        EXPECT_EQ(script->m_starts, 1);
        EndFrame();

        m_runtimeAPI->Update(m_runtime);
        EXPECT_EQ(script->m_starts, 1);
        EXPECT_EQ(script->m_updates, 0);
        ASSERT_TRUE(World::SetScriptEnabled<DisableOnStartScript>(entity, true));
        m_runtimeAPI->Update(m_runtime);
        EXPECT_EQ(script->m_starts, 1);
        EXPECT_EQ(script->m_updates, 1);
    }

    TEST_F(GameRuntimeTests, ScriptRefFollowsMovesAndDoesNotReviveOnReattach)
    {
        const auto first = World::CreateEntity();
        auto removed = World::AddScript<ValueScript>(first, -1);
        ASSERT_TRUE(removed);

        // 先頭を消すと末尾が移動する。既存の参照を再取得せずに利用する
        const auto last = World::CreateEntity();
        auto moved = World::AddScript<ValueScript>(last, 7);
        ASSERT_TRUE(moved);
        auto copied = moved;
        EndFrame();
        ASSERT_TRUE(World::RemoveScript<ValueScript>(first));
        EndFrame();
        ASSERT_TRUE(moved);
        EXPECT_EQ(moved->m_value, 7);

        // 追加でページが増えても、コピーした参照が同じ実体を指す
        for (int index = 0; index < 128; ++index)
            ASSERT_TRUE(World::AddScript<ValueScript>(World::CreateEntity(), index));
        EndFrame();
        ASSERT_TRUE(copied);
        moved->m_value = 8;
        EXPECT_EQ(copied->m_value, 8);

        auto replacement = World::AddScript<ValueScript>(first, 42);
        ASSERT_TRUE(replacement);
        EXPECT_EQ(replacement->m_value, 42);
        EXPECT_FALSE(removed);
    }

    TEST_F(GameRuntimeTests, PageGrowthKeepsExistingScriptsAndCrossPageRemovalUpdatesTheirReferences)
    {
        int constructions = 0;
        const auto first = World::CreateEntity();
        auto firstScript = World::AddScript<RelocationScript>(first, 0, constructions);
        ASSERT_TRUE(firstScript);
        for (int value = 1; value < 128; ++value)
            ASSERT_TRUE(World::AddScript<RelocationScript>(World::CreateEntity(), value, constructions));
        EndFrame();

        const auto* firstAddress = firstScript.operator->();
        constructions = 0;
        const auto last = World::CreateEntity();
        auto lastScript = World::AddScript<RelocationScript>(last, 128, constructions);
        ASSERT_TRUE(lastScript);
        EndFrame();

        // 新規 Script の保留領域からの移動だけが発生する
        EXPECT_EQ(constructions, 1);
        EXPECT_EQ(firstScript.operator->(), firstAddress);

        // ページをまたいで末尾を移し、保持済みの参照と Handle の解決先を揃える
        const auto* lastAddress = lastScript.operator->();
        ASSERT_TRUE(World::RemoveScript<RelocationScript>(first));
        EXPECT_FALSE(firstScript);
        EXPECT_EQ(lastScript.operator->(), lastAddress);
        EndFrame();
        ASSERT_TRUE(lastScript);
        EXPECT_EQ(lastScript->m_value, 128);
        EXPECT_EQ(lastScript.operator->(), firstAddress);
        auto found = World::GetScript<RelocationScript>(last);
        ASSERT_TRUE(found);
        EXPECT_EQ(found.operator->(), lastScript.operator->());

        // 空になった末尾ページを再利用しても、先頭ページの実体は動かない
        constructions = 0;
        ASSERT_TRUE(World::AddScript<RelocationScript>(World::CreateEntity(), 129, constructions));
        EndFrame();
        EXPECT_EQ(constructions, 1);
        EXPECT_EQ(lastScript.operator->(), firstAddress);
    }

    TEST_F(GameRuntimeTests, EndFrameDestroysScriptsBeforeEntitiesAndDrainsNewReservations)
    {
        const auto parent = World::CreateEntity();
        const auto child = World::CreateEntity();
        const auto another = World::CreateEntity();
        ASSERT_TRUE(World::GetProcess<HestiaGame::Transform>(child).SetParent(
            World::GetProcess<HestiaGame::Transform>(parent), Hestia::ParentMode::KeepLocal));
        auto parentScript = World::AddScript<DestroyScript>(parent, another, &m_destroyed);
        auto childScript = World::AddScript<DestroyScript>(child, EntityHandle{}, &m_destroyed);
        auto anotherScript = World::AddScript<DestroyScript>(another, EntityHandle{}, &m_destroyed);
        ASSERT_TRUE(parentScript && childScript && anotherScript);
        EndFrame();

        World::DestroyEntity(parent);
        EXPECT_FALSE(World::IsActive(parent));
        EXPECT_FALSE(World::IsActive(child));
        // まだ予約されていない別 Entity の Script は、このフレームの更新対象から外す
        ASSERT_TRUE(World::SetScriptEnabled<DestroyScript>(another, false));
        m_runtimeAPI->Update(m_runtime);
        EndFrame();

        EXPECT_EQ(m_destroyed, (std::vector<EntityHandle>{ parent, child, another }));
        EXPECT_FALSE(parentScript || childScript || anotherScript);
        EXPECT_EQ(ECSSystem::GetEntity(parent), nullptr);
        EXPECT_EQ(ECSSystem::GetEntity(child), nullptr);
        EXPECT_EQ(ECSSystem::GetEntity(another), nullptr);
    }

    TEST_F(GameRuntimeTests, WorldQueriesHandleCurrentEntitiesAndTransformSurvivesArchetypeMoves)
    {
        const auto entity = World::CreateEntity();
        const auto reserved = World::CreateEntity();
        ASSERT_TRUE(World::SetName(entity, "Player"));
        ASSERT_TRUE(World::SetTag(entity, 24));
        ASSERT_TRUE(World::SetTag(reserved, 24));
        ASSERT_TRUE(World::SetActive(entity, false));
        World::DestroyEntity(reserved);

        EXPECT_EQ(World::FindFirstEntityByName("Player"), entity);
        EXPECT_FALSE(World::FindFirstEntityByName("Missing").IsValid());
        const auto tagged = World::FindEntitiesByTag(24);
        EXPECT_EQ(tagged, (std::vector<EntityHandle>{ entity }));
        EXPECT_TRUE(World::FindEntitiesByTag(25).empty());
        ASSERT_TRUE(World::AddScript<ValueScript>(entity, 1));
        EXPECT_EQ(World::FindEntitiesWithScript<ValueScript>(), (std::vector<EntityHandle>{ entity }));
        ASSERT_TRUE(World::AddProcess<HestiaGame::Transform>(entity));
        EXPECT_EQ(World::FindEntitiesWithProcess<HestiaGame::Transform>(), (std::vector<EntityHandle>{ entity }));
        EXPECT_EQ(tagged, (std::vector<EntityHandle>{ entity }));

        auto transform = World::GetProcess<HestiaGame::Transform>(entity);
        ASSERT_TRUE(transform.IsValid());
        ASSERT_TRUE(transform.SetLocalPosition({ 1.0f, 2.0f, 3.0f }));
        ECSSystem::AddProcess<&Hestia::MeshRendererProcess::Run>(entity);
        EXPECT_EQ(transform.GetLocalPosition(), (Vector3{ 1.0f, 2.0f, 3.0f }));

        World::DestroyEntity(entity);
        EXPECT_FALSE(transform.IsValid());
        EXPECT_TRUE(World::FindEntitiesWithScript<ValueScript>().empty());
        EndFrame();
    }

    TEST_F(GameRuntimeTests, MeshRendererFacadeUsesAssetHandlesAndMaterialSlotsUntilDestroyReserved)
    {
        // Graphics は起動せず、CPU に登録した Asset の Handle を受け渡す
        Hestia::GraphicsSystem graphics;
        auto shader = Hestia::MaterialShader::Create("MeshRendererTestShader", {}, 0, {});
        ASSERT_NE(shader, nullptr);
        Hestia::AssetSystem assets;
        ASSERT_TRUE(assets.Initialize(&graphics));

        auto mesh = std::make_unique<Hestia::Mesh>();
        mesh->m_positions = { { { 0.0f, 0.0f, 0.0f } }, { { 1.0f, 0.0f, 0.0f } }, { { 0.0f, 1.0f, 0.0f } } };
        mesh->m_indices = { 0, 1, 2 };
        const auto meshHandle = Hestia::AssetSystem::Register("MeshRendererTestMesh", mesh.release());
        ASSERT_TRUE(meshHandle.IsValid());

        auto material = std::make_unique<Hestia::Material>();
        material->SetShader(shader.get());
        const auto materialHandle = Hestia::AssetSystem::Register("MeshRendererTestMaterial", material.release());
        ASSERT_TRUE(materialHandle.IsValid());

        // 同じ Facade で追加前後の Entity を解決し、検索結果も確認する
        const auto entity = World::CreateEntity();
        ASSERT_TRUE(entity.IsValid());
        auto renderer = World::GetProcess<HestiaGame::MeshRenderer>(entity);
        EXPECT_FALSE(renderer.IsValid());
        ASSERT_TRUE(World::AddProcess<HestiaGame::MeshRenderer>(entity));
        ASSERT_TRUE(renderer.IsValid());
        EXPECT_EQ(World::FindEntitiesWithProcess<HestiaGame::MeshRenderer>(), (std::vector<EntityHandle>{ entity }));

        EXPECT_TRUE(renderer.IsActive());
        ASSERT_TRUE(renderer.SetActive(false));
        EXPECT_FALSE(renderer.IsActive());
        EXPECT_TRUE(World::IsActive(entity));
        ASSERT_TRUE(renderer.SetMesh(meshHandle));
        EXPECT_EQ(renderer.GetMesh().m_index, meshHandle.m_index);
        EXPECT_EQ(renderer.GetMesh().m_generation, meshHandle.m_generation);

        const Vector4 color{ 0.25f, 0.5f, 0.75f, 1.0f };
        ASSERT_TRUE(renderer.SetInstanceColor(color));
        const auto storedColor = renderer.GetInstanceColor();
        EXPECT_FLOAT_EQ(storedColor.x, color.x);
        EXPECT_FLOAT_EQ(storedColor.y, color.y);
        EXPECT_FLOAT_EQ(storedColor.z, color.z);
        EXPECT_FLOAT_EQ(storedColor.w, color.w);

        // Slot 2 の設定で Slot 1 は空のまま残り、無効な書き込みでも設定値を保持する
        ASSERT_TRUE(renderer.SetMaterial(2, materialHandle));
        EXPECT_EQ(renderer.GetMaterialCount(), 3u);
        EXPECT_FALSE(renderer.GetMaterial(1).IsValid());
        EXPECT_FALSE(renderer.GetMaterial(3).IsValid());
        auto staleMaterialHandle = materialHandle;
        ++staleMaterialHandle.m_generation;
        EXPECT_FALSE(renderer.SetMaterial(2, staleMaterialHandle));
        EXPECT_EQ(renderer.GetMaterial(2).m_index, materialHandle.m_index);
        EXPECT_EQ(renderer.GetMaterial(2).m_generation, materialHandle.m_generation);
        ASSERT_TRUE(renderer.SetMaterial(2, {}));
        EXPECT_FALSE(renderer.GetMaterial(2).IsValid());
        EXPECT_EQ(renderer.GetMaterialCount(), 3u);

        // 破棄予約時点で保持済みの Facade と検索結果の両方から外れる
        World::DestroyEntity(entity);
        EXPECT_FALSE(renderer.IsValid());
        EXPECT_FALSE(renderer.SetMaterial(0, materialHandle));
        EXPECT_FALSE(renderer.GetMesh().IsValid());
        EXPECT_TRUE(World::FindEntitiesWithProcess<HestiaGame::MeshRenderer>().empty());
        EndFrame();
    }
}
