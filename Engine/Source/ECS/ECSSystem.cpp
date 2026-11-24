/*=============================================================================

 File   : ECSSystem.cpp
 Desc   : Engine内のECS初期化とWorld操作のstatic入口を実装する。

------------------------------------------------------------------------------

 Date   : 2026/10/16
 Author : Yokoyama Haruki

=============================================================================*/

#include "pch.h"
#include "ECS/ECSSystem.h"

#include <algorithm>
#include "API/ECSProcess/TransformAPI.h"
#include "Asset/AssetSystem.h"
#include "ECS/Component.h"
#include "Graphics/MeshRenderer.h"
#include "Log/Logger.h"
#include "Transform/TransformData.h"
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

    EntityHandle ECSSystem::CreateEntity(std::span<const ComponentTypeId> componentTypes)
    {
        if (s_instance == nullptr)
        {
            Logger::Error("ECSSystem::CreateEntity called while ECS is unavailable");
            return {};
        }

        return s_instance->m_world.CreateEntity(componentTypes);
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

    std::optional<SerializedEntity> ECSSystem::SerializeEntity(EntityHandle handle)
    {
        // HandleベースのフィールドGetterと同じWorldから取得する
        if (s_instance == nullptr || !s_instance->m_isInitialized)
            return std::nullopt;

        return s_instance->m_world.SerializeEntity(handle);
    }

    std::optional<SerializedEntity> ECSSystem::SaveEntity(EntityHandle root)
    {
        if (s_instance == nullptr || !s_instance->m_isInitialized)
        {
            Logger::Error("ECSSystem::SaveEntity requires an initialized ECS");
            return std::nullopt;
        }

        auto& world = s_instance->m_world;
        // Worldの親子関係を、独立した保存用のツリーへ移す
        const auto capture = [&](this const auto& self, EntityHandle handle) -> std::optional<SerializedEntity>
        {
            auto snapshot = world.SerializeEntity(handle);
            if (!snapshot)
            {
                Logger::Error("Could not serialize Entity " + std::to_string(handle.m_index));
                return std::nullopt;
            }

            const auto* hierarchy = world.GetComponent<const TransformHierarchy>(handle);
            snapshot->m_children.reserve(hierarchy->m_children.size());
            for (const auto child : hierarchy->m_children)
            {
                const auto* entity = world.GetEntity(child);
                if (entity != nullptr && entity->IsDestroy())
                    continue;

                auto childSnapshot = self(child);
                if (!childSnapshot)
                    return std::nullopt;
                snapshot->m_children.push_back(std::move(*childSnapshot));
            }
            return snapshot;
        };
        return capture(root);
    }

    const ComponentRegistry* ECSSystem::GetRegistry() noexcept
    {
        return s_instance && s_instance->m_isInitialized ? &s_instance->m_registry : nullptr;
    }

    EntityHandle ECSSystem::Instantiate(const Prefab& prefab)
    {
        return s_instance ? s_instance->InstantiateImpl(prefab, nullptr, nullptr, nullptr) : EntityHandle{};
    }

    EntityHandle ECSSystem::Instantiate(AssetHandle<Prefab> handle)
    {
        const auto* prefab = AssetSystem::Get(handle);
        return prefab ? Instantiate(*prefab) : EntityHandle{};
    }

    EntityHandle ECSSystem::Instantiate(const Prefab& prefab,
        const Vector3& position, const Quaternion& rotation, const Vector3& scale)
    {
        return s_instance ? s_instance->InstantiateImpl(prefab, &position, &rotation, &scale) : EntityHandle{};
    }

    EntityHandle ECSSystem::Instantiate(AssetHandle<Prefab> handle,
        const Vector3& position, const Quaternion& rotation, const Vector3& scale)
    {
        const auto* prefab = AssetSystem::Get(handle);
        return prefab ? Instantiate(*prefab, position, rotation, scale) : EntityHandle{};
    }

    EntityHandle ECSSystem::InstantiateImpl(const Prefab& prefab,
        const Vector3* position, const Quaternion* rotation, const Vector3* scale)
    {
        if (!m_isInitialized || m_world.m_isRunningProcess || m_world.m_isApplyingChanges || m_world.m_isEndingFrame)
        {
            Logger::Error("Prefab instantiation requires immediate Entity creation");
            return {};
        }

        const auto root = InstantiateNode(prefab.m_root, {}, position, rotation, scale);
        if (!root.IsValid())
            Logger::Error("Could not instantiate Prefab: " + prefab.m_name);
        return root;
    }

    EntityHandle ECSSystem::InstantiateNode(const SerializedEntity& source, EntityHandle parent,
        const Vector3* position, const Quaternion* rotation, const Vector3* scale)
    {
        // 保存した構成のArchetypeへ直接生成する
        std::vector<ComponentTypeId> types;
        std::vector<const ComponentInfo*> infos;
        for (const auto& component : source.m_components)
        {
            const auto* info = m_registry.FindInfo(component.m_typeName);
            if (info == nullptr)
            {
                Logger::Error("Unknown Prefab Component: " + component.m_typeName);
                return {};
            }
            types.push_back(info->id);
            infos.push_back(info);
        }

        // Entityを生成
        const auto handle = m_world.CreateEntity(types);
        if (!handle.IsValid())
            return {};

        // 親が有効ならTransformHierarchyを通じて接続する
        if (parent.IsValid() && !TransformAPIImpl::SetParent(handle, parent, ParentMode::KeepLocal))
            return FailPrefabNode(handle, "could not attach child");

        // コンポーネントのフィールドを保存値で上書きする
        auto* slot = m_world.FindAliveSlot(handle);
        for (size_t index = 0; index < infos.size(); ++index)
        {
            const auto& info = *infos[index];
            const auto column = slot->m_archetype->FindColumn(info.id);
            auto* destination = slot->m_chunk->GetElement(column, slot->m_row);
            const auto& fields = source.m_components[index].m_fields;

            size_t applied = 0;
            for (const auto& field : info.serializeFields)
            {
                const auto value = fields.find(std::string(field.name));
                if (value == fields.end())
                    continue;

                // Setterの呼出
                if (!field.write(destination, handle, value->second))
                    return FailPrefabNode(handle, "could not apply " + info.name + "." + value->first);

                ++applied;
            }
            if (applied != fields.size())
                return FailPrefabNode(handle, "unregistered fields in " + info.name);
        }

        // PrefabのルートにLocal TRSが保存されていれば、Transformを上書きする
        if (position &&
            !TransformProcess::SetLocalTRS(*m_world.GetComponent<Transform>(handle),
                *m_world.GetComponent<TransformHierarchy>(handle), *position, *rotation, *scale))
            return FailPrefabNode(handle, "invalid root Local TRS");

        // 子を再帰的に生成する
        for (const auto& child : source.m_children)
        {
            if (!InstantiateNode(child, handle).IsValid())
            {
                m_world.DestroyEntity(handle);
                return {};
            }
        }
        return handle;
    }

    EntityHandle ECSSystem::FailPrefabNode(EntityHandle handle, std::string_view reason)
    {
        Logger::Error(reason);
        // 接続済みの子孫も破棄予約する
        m_world.DestroyEntity(handle);
        return {};
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
