/*=============================================================================

 File   : EntityAPI.cpp
 Desc   : GameRuntime へ公開する Entity 操作を ECS に接続する。

------------------------------------------------------------------------------

 Date   : 2026/11/17
 Author : Yokoyama Haruki

=============================================================================*/

#include "pch.h"
#include "API/EntityAPI.h"
#include "ECS/ECSSystem.h"

namespace Hestia
{
    EntityAPI ECSSystem::CreateEntityAPI() noexcept
    {
        return {
            .CreateEntity = &ECSSystem::CreateEntity,
            .DestroyEntity = &ECSSystem::DestroyEntity,
            .Exists = [](EntityHandle handle)
            {
                const Entity* entity = s_instance == nullptr ? nullptr : s_instance->m_world.GetEntity(handle);
                return entity != nullptr && !entity->IsDestroy();
            },
            .IsActive = [](EntityHandle handle)
            {
                const Entity* entity = s_instance == nullptr ? nullptr : s_instance->m_world.GetEntity(handle);
                return entity != nullptr && entity->IsActive();
            },
            .SetActive = &ECSSystem::SetActive,
            .SetName = [](EntityHandle handle, const char* name, size_t size)
            {
                if (s_instance == nullptr || (name == nullptr && size != 0))
                    return false;

                const auto view = size == 0 ? std::string_view{} : std::string_view(name, size);
                return s_instance->m_world.SetName(handle, view);
            },
            .SetTag = [](EntityHandle handle, uint32_t tag)
            {
                return s_instance != nullptr && s_instance->m_world.SetTag(handle, tag);
            },
            .FindFirstEntityByName = [](const char* name, size_t size)
            {
                if (s_instance == nullptr || (name == nullptr && size != 0))
                    return EntityHandle{};

                const auto view = size == 0 ? std::string_view{} : std::string_view(name, size);
                return s_instance->m_world.FindFirstEntityByName(view);
            },
            .FindEntitiesByTag = [](uint32_t tag)
            {
                return s_instance == nullptr ? EntitySearchResult{} : s_instance->m_world.FindEntitiesByTag(tag);
            },
            .AddProcess = [](ProcessHandle process, EntityHandle handle)
            {
                return s_instance != nullptr && s_instance->m_world.AddProcess(handle, s_instance->ResolveProcess(process));
            },
            .FindEntitiesWithProcess = [](ProcessHandle process)
            {
                return s_instance == nullptr ? EntitySearchResult{} :
                    s_instance->m_world.FindEntitiesWithProcess(s_instance->ResolveProcess(process));
            },
            .ClearEntitySearchResults = []
            {
                if (s_instance != nullptr)
                    s_instance->m_world.ClearEntitySearchResults();
            },
            .TryGetPendingDestroy = [](size_t index, EntityHandle* handle)
            {
                return s_instance != nullptr && handle != nullptr &&
                    s_instance->m_world.TryGetPendingDestroy(index, *handle);
            },
            .SetActiveChangedCallback = &ECSSystem::SetEntityActiveChangedCallback,
        };
    }

    ECSProcessInvoke ECSSystem::ResolveProcess(ProcessHandle process) const noexcept
    {
        return process.m_index < m_world.m_processes.size() ?
            m_world.m_processes[process.m_index]->GetInvoke() : nullptr;
    }
}
