/*=============================================================================

 File   : World.cpp
 Desc   : Game 側からの Entity と Process の操作を Engine API に委譲する。

------------------------------------------------------------------------------

 Date   : 2026/11/13
 Author : Yokoyama Haruki

=============================================================================*/

#include "pch.h"
#include "World.h"

namespace HestiaGame
{
    const Hestia::EntityAPI* World::GetEntityAPI() noexcept
    {
        return s_runtime != nullptr ? s_runtime->GetEngineAPI().m_entity : nullptr;
    }

    Hestia::EntityHandle World::CreateEntity()
    {
        const auto* api = GetEntityAPI();
        return api != nullptr ? api->CreateEntity() : Hestia::EntityHandle{};
    }

    Hestia::EntityHandle World::Instantiate(Hestia::AssetHandle<Hestia::Prefab> prefab)
    {
        const auto* api = GetEntityAPI();
        return api != nullptr && api->Instantiate != nullptr ? api->Instantiate(prefab) : Hestia::EntityHandle{};
    }

    void World::DestroyEntity(Hestia::EntityHandle entity)
    {
        if (const auto* api = GetEntityAPI())
            api->DestroyEntity(entity);
    }

    bool World::IsActive(Hestia::EntityHandle entity)
    {
        const auto* api = GetEntityAPI();
        return api != nullptr && api->IsActive(entity);
    }

    bool World::SetActive(Hestia::EntityHandle entity, bool active)
    {
        const auto* api = GetEntityAPI();
        return api != nullptr && api->SetActive(entity, active);
    }

    bool World::SetName(Hestia::EntityHandle entity, std::string_view name)
    {
        const auto* api = GetEntityAPI();
        return api != nullptr && api->SetName(entity, name.data(), name.size());
    }

    bool World::SetTag(Hestia::EntityHandle entity, std::uint32_t tag)
    {
        const auto* api = GetEntityAPI();
        return api != nullptr && api->SetTag(entity, tag);
    }

    Hestia::EntityHandle World::FindFirstEntityByName(std::string_view name)
    {
        const auto* api = GetEntityAPI();
        return api != nullptr ? api->FindFirstEntityByName(name.data(), name.size()) : Hestia::EntityHandle{};
    }

    bool World::AddProcess(Hestia::ProcessHandle process, Hestia::EntityHandle entity)
    {
        const auto* api = GetEntityAPI();
        return api != nullptr && api->AddProcess(process, entity);
    }

    std::vector<Hestia::EntityHandle> World::FindEntitiesByTag(std::uint32_t tag)
    {
        const auto* api = GetEntityAPI();
        if (api == nullptr)
            return {};

        // 同じスレッドで検索結果を借り、結果を受け取る配列は Game 側で確保する
        const auto result = api->FindEntitiesByTag(tag);
        std::vector<Hestia::EntityHandle> entities;
        if (result.size != 0)
            entities.assign(result.data, result.data + result.size);

        api->ClearEntitySearchResults();
        return entities;
    }

    std::vector<Hestia::EntityHandle> World::FindEntitiesWithProcess(
        Hestia::ProcessHandle process)
    {
        const auto* api = GetEntityAPI();
        if (api == nullptr)
            return {};

        // 同じスレッドで連続して照会し、結果を受け取る配列は Game 側で確保する
        const auto result = api->FindEntitiesWithProcess(process);
        std::vector<Hestia::EntityHandle> entities;
        if (result.size != 0)
            entities.assign(result.data, result.data + result.size);

        api->ClearEntitySearchResults();
        return entities;
    }
}
