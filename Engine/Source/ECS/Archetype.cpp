/*=============================================================================

 File   : Archetype.cpp
 Desc   : Component配置、容量計算、Chunk所有を実装する。

------------------------------------------------------------------------------

 Date   : 2026/10/16
 Author : Yokoyama Haruki

=============================================================================*/

#include "pch.h"
#include "ECS/Archetype.h"

#include <algorithm>
#include <cassert>
#include <limits>

#include "ECS/Chunk.h"
#include "Log/Logger.h"

namespace Hestia
{
    namespace
    {
        bool AlignUp(size_t value, size_t alignment, size_t& result) noexcept
        {
            const size_t remainder = value % alignment;
            if (remainder == 0)
            {
                result = value;
                return true;
            }

            const size_t padding = alignment - remainder;
            if (value > std::numeric_limits<size_t>::max() - padding)
                return false;

            result = value + padding;
            return true;
        }

        bool Fits(size_t byteSize, size_t capacity,
            const ChunkLayout& layout) noexcept
        {
            if (capacity > std::numeric_limits<size_t>::max() / sizeof(Entity))
                return false;

            size_t end = capacity * sizeof(Entity);
            for (const ComponentLayout& component : layout.m_components)
            {
                size_t offset = 0;
                if (!AlignUp(end, component.m_info->alignment, offset))
                    return false;

                if (capacity > std::numeric_limits<size_t>::max() / component.m_info->size)
                    return false;

                const size_t bytes = capacity * component.m_info->size;
                if (offset > byteSize || bytes > byteSize - offset)
                    return false;

                end = offset + bytes;
            }

            return true;
        }
    }

    Archetype::Archetype(const ComponentRegistry& registry,
        ArchetypeSignature signature, size_t chunkByteSize)
        : m_registry(&registry),
          m_signature(std::move(signature))
    {
        if (chunkByteSize == 0 || m_signature.m_types.empty())
        {
            Logger::Error("Invalid Archetype layout settings");
            return;
        }

        // Signatureを正規化し、検索とLayout列の順序を統一する
        std::sort(m_signature.m_types.begin(), m_signature.m_types.end());
        if (std::adjacent_find(m_signature.m_types.begin(),
            m_signature.m_types.end()) != m_signature.m_types.end())
        {
            Logger::Error("Archetype contains duplicate ComponentTypeId");
            return;
        }

        m_layout.m_byteSize = chunkByteSize;
        m_layout.m_alignment = alignof(Entity);
        m_layout.m_components.reserve(m_signature.m_types.size());

        // Registryの型情報をLayoutへ並べ、Chunk全体で必要な最大アラインメントを求める
        for (ComponentTypeId type : m_signature.m_types)
        {
            const ComponentInfo* info = registry.GetInfo(type);
            if (info == nullptr || info->size == 0 || info->alignment == 0)
            {
                Logger::Error("Archetype contains an unknown ComponentTypeId");
                return;
            }

            m_layout.m_components.push_back({ type, 0, info });
            m_layout.m_alignment = std::max(m_layout.m_alignment, info->alignment);
        }

        // Entity列と各SoA列のpaddingを含め、Chunkに収まる最大row数を探す
        // row数が増えるほど必要bytesも増えるため、Fitsの単調性を使って二分探索する
        size_t lower = 0;
        size_t upper = chunkByteSize / sizeof(Entity);
        while (lower < upper)
        {
            const size_t candidate = lower + (upper - lower + 1) / 2;
            if (Fits(chunkByteSize, candidate, m_layout))
                lower = candidate;
            else
                upper = candidate - 1;
        }

        if (lower == 0)
        {
            Logger::Error("Chunk layout cannot contain an Entity");
            return;
        }

        m_layout.m_capacity = lower;
        size_t end = lower * sizeof(Entity);

        // 決定したrow数で各SoA列の最終offsetを確定する
        for (ComponentLayout& component : m_layout.m_components)
        {
            if (!AlignUp(end, component.m_info->alignment, component.m_offset))
            {
                Logger::Error("Chunk layout overflow");
                return;
            }

            end = component.m_offset + lower * component.m_info->size;
        }

        m_isValid = true;
    }


    Chunk& Archetype::GetOrCreateWritableChunk()
    {
        assert(m_isValid);

        // 既存Chunkを先に再利用し、満杯の場合だけ保存領域を追加する
        for (const auto& chunk : m_chunks)
        {
            if (chunk->GetEntityCount() < chunk->GetCapacity())
                return *chunk;
        }

        auto chunk = std::make_unique<Chunk>(m_layout);
        Chunk& result = *chunk;
        m_chunks.push_back(std::move(chunk));
        return result;
    }

    size_t Archetype::FindColumn(ComponentTypeId id) const noexcept
    {
        const auto it = std::lower_bound(m_layout.m_components.begin(),
            m_layout.m_components.end(), id,
            [](const ComponentLayout& component, ComponentTypeId value)
            {
                return component.m_type < value;
            });

        if (it == m_layout.m_components.end() || it->m_type != id)
            return m_layout.m_components.size();

        return static_cast<size_t>(it - m_layout.m_components.begin());
    }
}
