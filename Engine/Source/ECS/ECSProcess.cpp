/*=============================================================================

 File   : ECSProcess.cpp
 Desc   : 条件に合うArchetypeのChunkを順次実行する。

------------------------------------------------------------------------------

 Date   : 2026/10/16
 Author : Yokoyama Haruki

=============================================================================*/

#include "pch.h"
#include "ECS/ECSProcess.h"

#include <algorithm>
#include <cassert>

#include "ECS/Archetype.h"

namespace Hestia
{
    bool ECSProcess::MatchesArchetype(const Archetype& archetype) const noexcept
    {
        const ArchetypeSignature& signature = archetype.GetSignature();

        // 要求ComponentがArchetypeのComponent集合にすべて含まれるか確認する
        for (const ECSProcessComponent& component : m_components)
        {
            if (!std::binary_search(signature.m_types.begin(),
                signature.m_types.end(), component.m_type))
                return false;
        }

        return true;
    }

    void ECSProcess::RegisterArchetype(Archetype& archetype)
    {
        // Processが所有しないArchetype参照と、関数引数順の列対応をキャッシュする
        ECSProcessArchetype target;
        target.m_archetype = &archetype;
        target.m_columnIndices.reserve(m_components.size());

        // 関数引数の順序に合わせて、Archetype内の列番号を保持する
        for (const ECSProcessComponent& component : m_components)
        {
            const size_t column = archetype.FindColumn(component.m_type);
            assert(column < archetype.GetSignature().m_types.size());
            target.m_columnIndices.push_back(column);
        }

        m_archetypes.push_back(std::move(target));
    }

    void ECSProcess::UnregisterArchetype(const Archetype& archetype) noexcept
    {
        // 破棄されるArchetypeへの非所有参照を実行対象から外す
        std::erase_if(m_archetypes, [&archetype](const ECSProcessArchetype& entry)
        {
            return entry.m_archetype == &archetype;
        });
    }

    void ECSProcess::Run()
    {
        // ArchetypeごとにChunkを順次処理する
        for (const ECSProcessArchetype& target : m_archetypes)
        {
            Archetype* archetype = target.m_archetype;
            for (size_t chunkIndex = 0; chunkIndex < archetype->GetChunkCount(); ++chunkIndex)
            {
                Chunk& chunk = archetype->GetChunk(chunkIndex);
                if (chunk.GetEntityCount() == 0)
                    continue;

                // Chunk単位の実行
                m_invoke(chunk, target.m_columnIndices);
            }
        }
    }
}
