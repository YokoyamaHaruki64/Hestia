/*=============================================================================

 File   : ECSSystem.cpp
 Desc   : Engine内のECS初期化とWorld操作のstatic入口を実装する。

------------------------------------------------------------------------------

 Date   : 2026/10/16
 Author : Yokoyama Haruki

=============================================================================*/

#include "pch.h"
#include "ECS/ECSSystem.h"

#include "ECS/Component.h"
#include "Log/Logger.h"

namespace Hestia
{
    ECSSystem* ECSSystem::s_instance = nullptr;

    ECSSystem::ECSSystem()
        : m_world(m_registry)
    {
    }

    ECSSystem::~ECSSystem()
    {
        Finalize();
    }

    bool ECSSystem::Initialize(const ECSSettings& settings)
    {
        // WorldのChunk容量の設定
        if (m_isInitialized || m_isInitializing || s_instance != nullptr ||
            !m_world.ConfigureChunkSize(settings.m_chunkByteSize))
        {
            Logger::Error("ECSSystem::Initialize is unavailable in the current ECS state");
            return false;
        }

        // RegisterECS中だけProcess登録を許可し、必須Component登録後にWorldを有効化する
        s_instance = this;
        m_isInitializing = true;
        m_world.m_acceptsProcessRegistration = true;
        bool registrationSucceeded = false;

        registrationSucceeded = RegisterECS();

        if (!registrationSucceeded)
        {
            Logger::Error("ECSSystem::Initialize failed to register Components");
            Finalize();
            return false;
        }

        m_world.m_acceptsProcessRegistration = false;

        // Worldの初期化
        if(!m_world.Initialize())
        {
            Logger::Error("ECSSystem::Initialize failed to initialize ECSWorld");
            Finalize();
            return false;
        }

        m_isInitializing = false;
        m_isInitialized = true;
        return true;
    }

    bool ECSSystem::RegisterECS()
    {
        // 全Archetypeで必要になる標準Componentを先にRegistryへ登録する
        std::vector<std::optional<ComponentTypeId>> ids;

        ids.push_back(RegisterComponent<Name>("Hestia::Name"));
        ids.push_back(RegisterComponent<Tag>("Hestia::Tag"));
        ids.push_back(RegisterComponent<Layer>("Hestia::Layer"));


        // IDの登録結果照合
        for (const auto& id : ids)
        {
            if (!id.has_value())
            {
                Logger::Error("ECSSystem::RegisterECS failed to register a standard Component");
                return false;
            }
        }

        return true;
    }

    void ECSSystem::Finalize()
    {
        if (m_world.m_isRunningProcess || m_world.m_isApplyingChanges ||
            m_world.m_isEndingFrame)
        {
            Logger::Error("ECSSystem::Finalize is unavailable in the current ECS state");
            return;
        }

        m_isInitializing = false;
        m_isInitialized = false;
        // ArchetypeのLayoutがRegistry情報を参照するためWorldを先に解放する
        m_world.Finalize();
        m_registry.Finalize();

        if (s_instance == this)
            s_instance = nullptr;
    }

    EntityHandle ECSSystem::CreateEntity()
    {
        if (s_instance == nullptr)
        {
            Logger::Error("ECSSystem::CreateEntity called while ECS is unavailable");
            return {};
        }

        return s_instance->m_world.CreateEntity();
    }

    void ECSSystem::DestroyEntity(EntityHandle handle)
    {
        if (s_instance == nullptr)
        {
            Logger::Error("ECSSystem::DestroyEntity called while ECS is unavailable");
            return;
        }

        s_instance->m_world.DestroyEntity(handle);
    }

    Entity* ECSSystem::GetEntity(EntityHandle handle) noexcept
    {
        if (s_instance == nullptr)
        {
            Logger::Error("ECSSystem::GetEntity called while ECS is unavailable");
            return nullptr;
        }
        return s_instance->m_world.GetEntity(handle);
    }

    void ECSSystem::RunImpl(ECSProcessPhase phase)
    {
        // Engineから受け取ったphaseの処理をWorldへ委譲する
        m_world.Run(phase);
    }

    void ECSSystem::EndFrameImpl()
    {
        // Frame中に予約されたEntity破棄をWorldに確定させる
        m_world.EndFrame();
    }
}
