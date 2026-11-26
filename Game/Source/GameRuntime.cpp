/*=============================================================================

 File   : GameRuntime.cpp
 Desc   : Engine からの GameRuntime 呼び出しと Script の寿命を管理する。

------------------------------------------------------------------------------

 Date   : 2026/11/13
 Author : Yokoyama Haruki

=============================================================================*/

#include "pch.h"
#include "GameRuntime.h"

#include <new>

#include "GameAssets.h"
#include "GameInput.h"
#include "GameScene.h"
#include "GameTime.h"
#include "World.h"

namespace HestiaGame::Detail
{
    GameRuntime* GameRuntime::Create(const Hestia::GameEngineAPI* api,
        bool (*registerScripts)(), bool (*createInitialScripts)())
    {
        if (World::s_runtime != nullptr || api == nullptr || api->m_time == nullptr ||
            api->m_entity == nullptr || api->m_ecsProcess == nullptr)
            return nullptr;

        // 公開境界で必須操作を確認し、以降はテーブルの有効性を前提とする
        const auto& entity = *api->m_entity;
        if (!entity.CreateEntity ||
            !entity.DestroyEntity ||
            !entity.Exists ||
            !entity.IsActive || !entity.SetActive ||
            !entity.SetName || !entity.SetTag ||
            !entity.FindFirstEntityByName ||
            !entity.FindEntitiesByTag ||
            !entity.AddProcess ||
            !entity.FindEntitiesWithProcess ||
            !entity.ClearEntitySearchResults ||
            !entity.TryGetPendingDestroy || !entity.SetActiveChangedCallback)
            return nullptr;

        GameRuntime* runtime = new (std::nothrow) GameRuntime(*api);
        if (!runtime)
            return nullptr;

        World::s_runtime = runtime;
        Time::Bind(api->m_time);
        Logger::Bind(api->m_log);
        Assets::Bind(api->m_asset);
        Scene::Bind(api->m_scene);
        Input::Bind(api->m_input);

        // 初期配置のコールバックから Active を変更する場合も ECS から通知を受ける
        entity.SetActiveChangedCallback(runtime, [](void* context, Hestia::EntityHandle handle, bool active)
        {
            static_cast<GameRuntime*>(context)->OnEntityActiveChanged(handle, active);
        });

        // 型一覧を確定させてから、初期 Script の配置とコールバックを許可する
        runtime->m_initializing = true;
        const bool registered = registerScripts == nullptr || registerScripts();
        runtime->m_initializing = false;
        runtime->m_initialized = registered;
        if (!registered || (createInitialScripts != nullptr && !createInitialScripts()))
        {
            runtime->Finalize();
            delete runtime;
            return nullptr;
        }

        // 初期配置分は最初の FixedUpdate から実行できるように反映する
        runtime->EndFrame();
        return runtime;
    }

    void GameRuntime::Finalize()
    {
        // OnDestroy が既存の Facade を利用できる状態で Script の実体を先に解放する
        m_initialized = false;
        m_engineAPI.m_entity->SetActiveChangedCallback(nullptr, nullptr);
        for (auto& storage : m_storages)
            storage->DestroyAll();

        m_storageIndices.clear();
        m_guidStorageIndices.clear();
        m_storageGuids.clear();
        m_entityScripts.clear();
        m_storages.clear();
        Input::Unbind();
        Scene::Unbind();
        Assets::Unbind();
        Logger::Unbind();
        Time::Unbind();
        World::s_runtime = nullptr;
    }

    void GameRuntime::Run(ScriptPhase phase)
    {
        if (!m_initialized)
            return;
        for (auto& storage : m_storages)
            storage->Run(phase);
    }

    void GameRuntime::EndFrame()
    {
        if (!m_initialized)
            return;

        // 破棄通知が完了してから、次のフレームに向けた追加を反映する
        // OnDestroy 中にも破棄予約が増えるため、Handle を一件ずつ現在の列から読む
        Hestia::EntityHandle entity;
        for (std::size_t index = 0; m_engineAPI.m_entity->TryGetPendingDestroy(index, &entity); ++index)
        {
            for (auto& storage : m_storages)
                storage->Remove(entity);
        }

        FlushScripts();
    }

    bool GameRuntime::FlushScripts()
    {
        // フレーム境界でStorageの反映と逆引き索引を同期する
        bool success = true;
        for (std::size_t index = 0; index < m_storages.size(); ++index)
        {
            if (!m_storages[index]->Flush(m_entityScripts, index))
                success = false;
        }
        return success;
    }

    void GameRuntime::ClearSceneScripts()
    {
        // OnDestroyから旧SceneのScriptが追加されないよう、操作を一時的に閉じる
        m_initialized = false;
        for (auto& storage : m_storages)
            storage->DestroyAll();
        m_entityScripts.clear();
        m_initialized = true;
    }

    bool GameRuntime::QueryScriptAttachments(const Hestia::EntityHandle* entities, std::size_t count,
        void* context, Hestia::ScriptAttachmentVisitor visitor) const
    {
        if (!m_initialized || !visitor || (!entities && count != 0))
            return false;

        // 対象Handleから反映済みのStorageを引き、GUIDだけを返す
        for (std::size_t index = 0; index < count; ++index)
        {
            const auto entity = entities[index];
            const auto found = m_entityScripts.find(entity);
            if (found == m_entityScripts.end())
                continue;

            for (const auto storageIndex : found->second)
                visitor(context, entity, m_storageGuids[storageIndex]);
        }
        return true;
    }

    Hestia::SceneScriptResult GameRuntime::AttachSceneScript(Hestia::EntityHandle entity, Hestia::AssetGuid guid)
    {
        using Hestia::SceneScriptResult;
        if (!m_initialized || !m_engineAPI.m_entity->Exists(entity))
            return SceneScriptResult::Failed;

        const auto found = m_guidStorageIndices.find(guid);
        if (found == m_guidStorageIndices.end())
            return SceneScriptResult::UnknownType;

        // 生成に失敗した場合は、部分的なSceneを実行せず呼び出し元で処理する
        try
        {
            return m_storages[found->second]->AddDefault(entity, ++m_lastAttachment) ?
                SceneScriptResult::Attached : SceneScriptResult::Failed;
        }
        catch (...)
        {
            Logger::Error("GameRuntime failed to construct a Scene Script");
            return SceneScriptResult::Failed;
        }
    }

    void GameRuntime::OnEntityActiveChanged(Hestia::EntityHandle entity, bool active)
    {
        if (!m_initialized)
            return;

        // 通知中の再変更は、先行する変更を全 Script へ届けてから処理する
        const bool isNotifying = !m_activeChanges.empty();
        try
        {
            m_activeChanges.push_back({ entity, active, m_lastAttachment });
        }
        catch (...)
        {
            Logger::Error("GameRuntime failed to queue an Entity Active change");
            return;
        }
        if (isNotifying)
            return;

        for (std::size_t index = 0; index < m_activeChanges.size(); ++index)
        {
            const auto change = m_activeChanges[index];
            for (auto& storage : m_storages)
                storage->NotifyActive(change.m_entity, change.m_active, change.m_lastAttachment);
        }
        m_activeChanges.clear();
    }

}
