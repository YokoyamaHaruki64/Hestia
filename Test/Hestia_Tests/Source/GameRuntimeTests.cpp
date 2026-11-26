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
#include "Engine/Include/API/AssetAPI.h"
#include "Engine/Include/API/SceneAPI.h"
#include "Engine/Include/Asset/AssetSystem.h"
#include "Engine/Include/ECS/ECSSystem.h"
#include "Engine/Include/Game/MeshRenderer.h"
#include "Engine/Include/Graphics/GraphicsSystem.h"
#include "Engine/Include/Graphics/MeshRenderer.h"
#include "Engine/Include/API/TimeAPI.h"
#include "Engine/Include/Time/TimeSystem.h"
#include "Engine/Include/Scene/SceneSystem.h"
#include "Game/Include/GameScene.h"
#include "Game/Include/GameTime.h"
#include "Game/Include/World.h"

namespace
{
    using Hestia::ECSSystem;
    using Hestia::EntityHandle;
    using Hestia::SceneSystem;
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
            m_engineAPI.m_scene = &Hestia::SceneAPI::Get();
            m_runtimeAPI = GetGameRuntimeAPI();
            ASSERT_NE(m_runtimeAPI, nullptr);
            m_runtime = HestiaGame::Detail::GameRuntime::Create(&m_engineAPI, []
            {
                return World::RegisterScript<ReactivateScript>({ { 1, 0 } }) &&
                    World::RegisterScript<SpawnScript>({ { 2, 0 } }) &&
                    World::RegisterScript<LifecycleScript>({ { 3, 0 } }) &&
                    World::RegisterScript<DisableOnStartScript>({ { 4, 0 } }) &&
                    World::RegisterScript<ValueScript>({ { 5, 0 } }) &&
                    World::RegisterScript<RelocationScript>({ { 6, 0 } }) &&
                    World::RegisterScript<DestroyScript>({ { 7, 0 } });
            });
            ASSERT_NE(m_runtime, nullptr);
        }

        void TearDown() override
        {
            if (m_runtime != nullptr)
                m_runtimeAPI->Destroy(m_runtime);
            SceneSystem::Finalize();
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

    TEST(SceneStartup, GeneratesCubesWithSharedInstancedMaterialWithoutRendering)
    {
        using namespace Hestia;
        const auto repository = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path().parent_path();
        const auto source = repository.empty() ? std::filesystem::path("../Assets") : repository / "Assets";
        const auto directory = std::filesystem::temp_directory_path() /
            ("HestiaSceneStartup-" + std::to_string(GetCurrentProcessId()));
        const std::filesystem::path files[] = {
            "Scenes/Startup.scene", "Scenes/Startup.scene.meta", "Shaders/Common.hlsl",
            "Materials/DebugCube.material", "Materials/DebugCube.material.meta",
            "Shaders/MatShaders/Unlit_Inst.matsh", "Shaders/MatShaders/Unlit_Inst.matsh.meta"
        };
        for (const auto& file : files)
        {
            std::filesystem::create_directories((directory / file).parent_path());
            ASSERT_TRUE(std::filesystem::copy_file(source / file, directory / file,
                std::filesystem::copy_options::overwrite_existing));
        }

        // 描画デバイスなしで、起動Sceneの共有Materialをファイルから取得する
        GraphicsSystem graphics;
        AssetSystem assets;
        ECSSystem ecs;
        ASSERT_TRUE(assets.Initialize(&graphics, directory));
        const auto* sharedMaterial = AssetSystem::Get(AssetSystem::Load<Material>("Materials/DebugCube.material"));
        ASSERT_TRUE(sharedMaterial && sharedMaterial->GetShader() && sharedMaterial->GetShader()->IsInstancing());

        // 本番用の生成登録とAsset Facadeを起動Sceneの復元へ接続する
        ASSERT_TRUE(ecs.Initialize());
        Hestia::TimeSystem clock;
        clock.Initialize();
        const std::unique_ptr<TimeSystem, void (*)(TimeSystem*)> finalizeClock(
            &clock, [](TimeSystem* value) { value->Finalize(); });
        TimeAPI time;
        time.Initialize(&clock);
        auto entityAPI = ECSSystem::CreateEntityAPI();
        AssetAPI assetAPI;
        assetAPI.Initialize(&assets);
        GameEngineAPI engineAPI{ &time, &entityAPI, &ecs.GetProcessAPI(), nullptr, &assetAPI };
        const auto* runtimeAPI = GetGameRuntimeAPI();
        const auto destroyRuntime = [runtimeAPI](void* instance)
        {
            runtimeAPI->Destroy(instance);
            SceneSystem::Finalize();
        };
        std::unique_ptr<void, decltype(destroyRuntime)> runtime(runtimeAPI->Create(&engineAPI), destroyRuntime);
        ASSERT_TRUE(runtime);
        ECSSystem::BindScriptAttachmentQuery(runtime.get(), runtimeAPI->QueryScriptAttachments);

        ASSERT_TRUE(SceneSystem::LoadScene("Scenes/Startup.scene"));
        ASSERT_TRUE(SceneSystem::CheckScene(assets.GetDatabase(), *runtimeAPI, runtime.get()));
        const auto generator = World::FindFirstEntityByName("Debug Cube Generator");
        ASSERT_TRUE(generator.IsValid());
        const auto snapshot = ECSSystem::SaveEntity(generator);
        ASSERT_TRUE(snapshot);
        ASSERT_EQ(snapshot->m_scripts.size(), 1u);
        EXPECT_EQ(snapshot->m_scripts.front().m_guid, AssetGuid::Parse("feb7a48d-7059-4edb-9dd3-646b29b6f37c"));

        // 全Cubeが組み込みMeshと同じMaterialを持つことを確認する
        const auto cubes = World::FindEntitiesWithProcess<HestiaGame::MeshRenderer>();
        ASSERT_EQ(cubes.size(), 100'000u);
        const auto* mesh = AssetSystem::Get(AssetSystem::Load<Mesh>("Cube"));
        ASSERT_NE(mesh, nullptr);
        EXPECT_TRUE(std::ranges::all_of(cubes, [mesh, sharedMaterial](EntityHandle cube)
        {
            const auto* renderer = ECSSystem::GetComponent<Hestia::MeshRenderer>(cube);
            return renderer && renderer->m_mesh == mesh && renderer->m_materials.size() == 1 &&
                renderer->m_materials.front() == sharedMaterial;
        }));

        ECSSystem::BindScriptAttachmentQuery(nullptr, nullptr);
        std::filesystem::remove_all(directory);
    }

    TEST_F(GameRuntimeTests, SceneSaveCanReloadImmediatelyAndReplacesTheWorld)
    {
        const auto directory = std::filesystem::temp_directory_path() /
            ("HestiaSceneSwitch-" + std::to_string(GetCurrentProcessId()));
        std::filesystem::create_directories(directory);
        Hestia::GraphicsSystem graphics;
        Hestia::AssetSystem assets;
        ASSERT_TRUE(assets.Initialize(&graphics, directory));
        const auto& database = assets.GetDatabase();
        ECSSystem::BindScriptAttachmentQuery(m_runtime, m_runtimeAPI->QueryScriptAttachments);

        const auto original = World::CreateEntity();
        ASSERT_TRUE(World::SetName(original, "Saved spinner"));
        ASSERT_TRUE(World::AddScript<ValueScript>(original, 42));
        EndFrame();
        ASSERT_TRUE(SceneSystem::SaveScene());
        ASSERT_TRUE(SceneSystem::SaveScene());
        ASSERT_TRUE(SceneSystem::SerializeScene());
        EXPECT_EQ(SceneSystem::GetLastSavedScenePath(), "NewScene.scene");
        ASSERT_NE(database.FindByPath("NewScene.scene"), nullptr);
        ASSERT_TRUE(World::SetName(original, "Unsaved change"));
        ASSERT_TRUE(SceneSystem::SerializeScene());

        // 保存後に追加したScriptも、旧Entityの実体がある間に終了する
        ASSERT_TRUE(World::AddScript<DestroyScript>(original, EntityHandle{}, &m_destroyed));
        EndFrame();
        ASSERT_TRUE(SceneSystem::LoadScene("Missing.scene"));
        ASSERT_TRUE(SceneSystem::LoadScene("NewScene.scene"));
        ASSERT_TRUE(SceneSystem::CheckScene(database, *m_runtimeAPI, m_runtime));
        EXPECT_EQ(SceneSystem::GetCurrentScenePath(), "NewScene.scene");
        EXPECT_EQ(m_destroyed, (std::vector<EntityHandle>{ original }));
        EXPECT_EQ(ECSSystem::GetEntity(original), nullptr);
        const auto restored = World::FindFirstEntityByName("Saved spinner");
        ASSERT_TRUE(restored.IsValid());
        ASSERT_TRUE(World::GetScript<ValueScript>(restored));
        EXPECT_EQ(World::GetScript<ValueScript>(restored)->m_value, 0);

        // 同じパスでもファイルを読み直し、未登録の型だけ除いて展開する
        auto current = ECSSystem::SaveEntity(restored);
        ASSERT_TRUE(current);
        ASSERT_EQ(current->m_scripts.size(), 1u);
        current->m_active = false;
        current->m_scripts.push_back({ { { 90, 1 } }, 0 });
        ASSERT_TRUE(Hestia::AssetSystem::Save(std::span<const Hestia::SerializedEntity>{ &*current, 1 }, "NewScene.scene"));
        ASSERT_TRUE(SceneSystem::LoadScene("NewScene.scene"));
        ASSERT_TRUE(SceneSystem::CheckScene(database, *m_runtimeAPI, m_runtime));
        EXPECT_EQ(ECSSystem::GetEntity(restored), nullptr);
        const auto reopened = World::FindFirstEntityByName("Saved spinner");
        ASSERT_TRUE(reopened.IsValid());
        EXPECT_FALSE(World::IsActive(reopened));
        EXPECT_TRUE(World::GetScript<ValueScript>(reopened));
        EXPECT_FALSE(SceneSystem::LoadScene("../Saved.scene"));
        ASSERT_TRUE(SceneSystem::LoadScene("Missing.scene"));
        EXPECT_FALSE(SceneSystem::CheckScene(database, *m_runtimeAPI, m_runtime));
        EXPECT_EQ(SceneSystem::GetCurrentScenePath(), "NewScene.scene");
        ECSSystem::BindScriptAttachmentQuery(nullptr, nullptr);
        std::filesystem::remove_all(directory);
    }

    TEST_F(GameRuntimeTests, ExposesCurrentSceneAfterDeferredSwitch)
    {
        using HestiaGame::Scene;
        const auto directory = std::filesystem::temp_directory_path() /
            ("HestiaGameSceneAPI-" + std::to_string(GetCurrentProcessId()));
        std::filesystem::create_directories(directory);
        Hestia::GraphicsSystem graphics;
        Hestia::AssetSystem assets;
        ASSERT_TRUE(assets.Initialize(&graphics, directory));
        const auto& database = assets.GetDatabase();
        const std::span<const Hestia::SerializedEntity> roots;
        ASSERT_TRUE(Hestia::AssetSystem::Save(roots, "Maps/Hub.scene"));
        ASSERT_TRUE(Hestia::AssetSystem::Save(roots, "Maps/Final.Level.scene"));

        // 初回の復元が完了してから名前とパスを取得できる
        EXPECT_TRUE(Scene::GetCurrentSceneName().empty());
        EXPECT_TRUE(Scene::GetCurrentScenePath().empty());
        ASSERT_TRUE(Scene::LoadScene("Maps/./Hub.scene"));
        EXPECT_TRUE(Scene::GetCurrentSceneName().empty());
        EXPECT_TRUE(Scene::GetCurrentScenePath().empty());
        ASSERT_TRUE(SceneSystem::CheckScene(database, *m_runtimeAPI, m_runtime));
        EXPECT_EQ(Scene::GetCurrentSceneName(), "Hub");
        EXPECT_EQ(Scene::GetCurrentScenePath(), "Maps/Hub.scene");

        // 無効な要求を拒否し、有効な要求の処理までは現在の Scene を返す
        ASSERT_TRUE(Scene::LoadScene("Maps/Final.Level.scene"));
        EXPECT_FALSE(Scene::LoadScene("../Outside.scene"));
        EXPECT_FALSE(Scene::LoadScene("Maps/Final.Level.prefab"));
        EXPECT_EQ(Scene::GetCurrentSceneName(), "Hub");
        EXPECT_EQ(Scene::GetCurrentScenePath(), "Maps/Hub.scene");
        ASSERT_TRUE(SceneSystem::CheckScene(database, *m_runtimeAPI, m_runtime));
        EXPECT_EQ(Scene::GetCurrentSceneName(), "Final.Level");
        EXPECT_EQ(Scene::GetCurrentScenePath(), "Maps/Final.Level.scene");
        std::filesystem::remove_all(directory);
    }

    TEST_F(GameRuntimeTests, QueriesOnlyRequestedAttachmentsAfterFlush)
    {
        const auto selected = World::CreateEntity();
        const auto omitted = World::CreateEntity();
        ASSERT_TRUE(World::AddScript<LifecycleScript>(selected, &m_events));
        ASSERT_TRUE(World::AddScript<LifecycleScript>(omitted, &m_events));

        std::vector<Hestia::AssetGuid> results;
        const auto collect = [](void* output, EntityHandle, Hestia::AssetGuid guid)
        {
            static_cast<std::vector<Hestia::AssetGuid>*>(output)->push_back(guid);
        };
        ASSERT_TRUE(m_runtimeAPI->QueryScriptAttachments(m_runtime, &selected, 1, &results, collect));
        EXPECT_TRUE(results.empty());
        EndFrame();

        ASSERT_TRUE(m_runtimeAPI->QueryScriptAttachments(m_runtime, &selected, 1, &results, collect));
        EXPECT_EQ(results, (std::vector<Hestia::AssetGuid>{ { { 3, 0 } } }));
        results.clear();
        ASSERT_TRUE(World::RemoveScript<LifecycleScript>(selected));
        EndFrame();
        ASSERT_TRUE(m_runtimeAPI->QueryScriptAttachments(m_runtime, &selected, 1, &results, collect));
        EXPECT_TRUE(results.empty());
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
        EXPECT_FALSE(World::RegisterScript<UnregisteredScript>({ { 8, 0 } }));
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
