/*=============================================================================

 File   : ComponentRegistry.cpp
 Desc   : Component型IDの生成とRegistryの終了処理を実装する。

------------------------------------------------------------------------------

 Date   : 2026/10/16
 Author : Yokoyama Haruki

=============================================================================*/

#include "pch.h"
#include "ECS/ComponentRegistry.h"
#include "Common/Include/StringUtils.h"

namespace Hestia
{
    ComponentTypeId ComponentRegistry::HashName(std::string_view name) noexcept
    {
        return StringToHash(name);
    }

    const ComponentInfo* ComponentRegistry::GetInfo(ComponentTypeId id) const
    {
        const auto it = m_components.find(id);
        if (it == m_components.end())
            return nullptr;

        return &it->second;
    }

    const ComponentInfo* ComponentRegistry::FindInfo(std::string_view name) const
    {
        const auto* info = GetInfo(HashName(name));
        return info != nullptr && info->name == name ? info : nullptr;
    }

    void ComponentRegistry::Finalize()
    {
        m_typeIds.clear();
        m_components.clear();
    }
}
