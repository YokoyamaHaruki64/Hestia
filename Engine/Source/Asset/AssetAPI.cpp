/*=============================================================================

 File   : AssetAPI.cpp
 Desc   : AssetAPI の DLL 境界操作を AssetSystem へ委譲する。

------------------------------------------------------------------------------

 Date   : 2026/10/15
 Author : Yokoyama Haruki

=============================================================================*/

#include "pch.h"
#include "Asset/AssetAPI.h"

#include <memory>

#include "Asset/AssetSystem.h"

namespace Hestia
{
    void AssetAPI::Initialize(AssetSystem* system)
    {
        m_system = system;
    }

    void AssetAPI::Finalize()
    {
        m_system = nullptr;
    }

    template<class T>
    AssetHandle<T> AssetAPI::Load(std::string_view name)
    {
        if (m_system == nullptr)
            return {};

        return AssetSystem::Load<T>(name);
    }

    template<class T>
    T* AssetAPI::Get(AssetHandle<T> handle)
    {
        if (m_system == nullptr)
            return nullptr;

        return AssetSystem::Get<T>(handle);
    }

    template<class T>
    AssetHandle<T> AssetAPI::Register(std::string_view name, T* asset)
    {
        std::unique_ptr<T> owner(asset);
        if (m_system == nullptr)
            return {};

        return AssetSystem::Register<T>(name, owner.release());
    }

    template HESTIA_ENGINE_API AssetHandle<Mesh> AssetAPI::Load<Mesh>(std::string_view);
    template HESTIA_ENGINE_API AssetHandle<Texture> AssetAPI::Load<Texture>(std::string_view);
    template HESTIA_ENGINE_API Mesh* AssetAPI::Get<Mesh>(AssetHandle<Mesh>);
    template HESTIA_ENGINE_API Texture* AssetAPI::Get<Texture>(AssetHandle<Texture>);
    template HESTIA_ENGINE_API AssetHandle<Mesh> AssetAPI::Register<Mesh>(
        std::string_view, Mesh*);
    template HESTIA_ENGINE_API AssetHandle<Texture> AssetAPI::Register<Texture>(
        std::string_view, Texture*);
}
