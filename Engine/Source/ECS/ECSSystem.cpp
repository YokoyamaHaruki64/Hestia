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
#include "Graphics/MeshRenderer.h"
#include "Log/Logger.h"
#include "Transform/TransformProcess.h"

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
        m_processAPI = CreateECSProcessAPI();
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
        // 簡略化用マクロとusing宣言
        using enum ECSProcessPhase;

#define REGISTER_COMPONENT(Type, TypeName) \
    if (!RegisterComponent<Type>(TypeName).has_value()) { \
        Logger::Error("ECS: Failed to Component Register: " #Type); \
        return false; \
    }

#define REGISTER_PROCESS(Func, Phase) \
    if (!m_world.RegisterProcess<Func>(Phase)) { \
        Logger::Error("ECS: Failed to Process Register: " #Func); \
        return false; \
    }

#define REGISTER_GAME_PROCESS(API, Func, Phase) \
    if (!RegisterGameProcess<API, Func>(Phase)) { \
        Logger::Error("ECS: Failed to Game Process Register: " #Func); \
        return false; \
    }

        // Component登録
        REGISTER_COMPONENT(Name, "Hestia::Name");
        REGISTER_COMPONENT(Tag, "Hestia::Tag");
        REGISTER_COMPONENT(Layer, "Hestia::Layer");
        REGISTER_COMPONENT(Transform, "Hestia::Transform");
        REGISTER_COMPONENT(TransformCache, "Hestia::TransformCache");
        REGISTER_COMPONENT(TransformHierarchy, "Hestia::TransformHierarchy");
        REGISTER_COMPONENT(MeshRenderer, "Hestia::MeshRenderer");

        // PreRenderではWorld行列を更新してから描画を収集
        REGISTER_GAME_PROCESS(TransformAPI, &TransformProcess::Run, PreRender);
        REGISTER_GAME_PROCESS(MeshRendererAPI, &MeshRendererProcess::Run, PreRender);
        // REGISTER_PROCESS(&MoveTransform, Update);

        // マクロ解除
#undef REGISTER_COMPONENT
#undef REGISTER_PROCESS
#undef REGISTER_GAME_PROCESS

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
        m_processAPI = {};
        // ArchetypeのLayoutがRegistry情報を参照するためWorldを先に解放する
        m_world.Finalize();
        m_registry.Finalize();

        if (s_instance == this)
            s_instance = nullptr;
    }

    void ECSSystem::SetEntityActiveChangedCallback(void* context,
        void (*callback)(void*, EntityHandle, bool)) noexcept
    {
        if (s_instance == nullptr)
            return;

        s_instance->m_world.m_entityActiveChangedContext = context;
        s_instance->m_world.m_entityActiveChanged = callback;
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

    bool ECSSystem::SetActive(EntityHandle handle, bool active)
    {
        return s_instance != nullptr && s_instance->m_world.SetActive(handle, active);
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
