/*=============================================================================

 File   : ComponentRegistry.cpp
 Desc   : Component型IDの生成とRegistryの終了処理を実装する。

------------------------------------------------------------------------------

 Date   : 2026/10/16
 Author : Yokoyama Haruki

=============================================================================*/

#include "pch.h"
#include "ECS/ComponentRegistry.h"

namespace Hestia
{
    ComponentTypeId ComponentRegistry::HashName(std::string_view name) noexcept
    {
        // 登録名のUTF-8 byte列にFNV-1a 64bitを適用する
        constexpr ComponentTypeId offsetBasis = 14695981039346656037ull;
        constexpr ComponentTypeId prime = 1099511628211ull;
        ComponentTypeId hash = offsetBasis;

        for (const unsigned char value : name)
        {
            hash ^= value;
            hash *= prime;
        }

        return hash;
    }

    const ComponentInfo* ComponentRegistry::GetInfo(ComponentTypeId id) const
    {
        const auto it = m_components.find(id);
        if (it == m_components.end())
            return nullptr;

        return &it->second;
    }

    void ComponentRegistry::Finalize()
    {
        m_typeIds.clear();
        m_components.clear();
    }
}
