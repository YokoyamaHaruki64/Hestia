/*=============================================================================

 File   : Chunk.cpp
 Desc   : アラインメント付き保存領域とComponent寿命を実装する。

------------------------------------------------------------------------------

 Date   : 2026/10/16
 Author : Yokoyama Haruki

=============================================================================*/

#include "pch.h"
#include "ECS/Chunk.h"
#include "Log/Logger.h"

#include <algorithm>
#include <memory>
#include <new>

namespace Hestia
{
    Chunk::Chunk(const ChunkLayout& layout)
        : m_layout(&layout)
    {
        // Layoutが要求するアラインメントで領域を確保し、Entity列の配列寿命を開始する
        const size_t alignment = std::max(layout.m_alignment, alignof(std::max_align_t));
        m_data = static_cast<std::byte*>(::operator new(
            layout.m_byteSize, std::align_val_t(alignment)));

        // 保存領域の先頭をEntity列として使い、Component列はLayoutのoffsetに置く
        ::new (m_data) Entity[layout.m_capacity];
    }

    Chunk::~Chunk()
    {
        // Commit済みrowだけを逆順に破棄し、最後にEntity列自体の寿命を終える
        while (m_entityCount > 0)
        {
            const size_t row = m_entityCount - 1;
            for (size_t column = m_layout->m_components.size(); column > 0; --column)
                DestroyComponent(column - 1, row);
            DestroyEntity(row);
            --m_entityCount;
        }

        std::destroy_n(reinterpret_cast<Entity*>(m_data), m_layout->m_capacity);

        const size_t alignment = std::max(m_layout->m_alignment, alignof(std::max_align_t));
        ::operator delete(m_data, std::align_val_t(alignment));
    }

    Entity* Chunk::ConstructEntity(size_t row, EntityHandle handle, bool active)
    {
        // Commit前の末尾rowだけにEntityを構築し、後続rowとの間に穴を作らない
        if (row != m_entityCount || row >= GetCapacity())
        {
            Logger::Error("Invalid Chunk row for ConstructEntity");
            return nullptr;
        }

        *GetEntity(row) = Entity(handle, active);
        return GetEntity(row);
    }

    Entity* Chunk::ConstructEntityCopy(size_t row, const Entity& source)
    {
        if (row != m_entityCount || row >= GetCapacity())
        {
            Logger::Error("Invalid Chunk row for ConstructEntityCopy");
            return nullptr;
        }

        *GetEntity(row) = source;
        return GetEntity(row);
    }

    void Chunk::ConstructDefault(size_t columnIndex, size_t row)
    {
        const ComponentLayout& column = m_layout->m_components[columnIndex];
        // 型ごとにRegistryへ登録したコンストラクタでComponentを構築する
        column.m_info->construct(GetElement(columnIndex, row));
    }

    void Chunk::MoveConstruct(size_t destinationColumn, size_t destinationRow,
        Chunk& source, size_t sourceColumn, size_t sourceRow) noexcept
    {
        // 移行先の型情報に登録されたMoveコンストラクタでComponentを移す
        const ComponentInfo* info = m_layout->m_components[destinationColumn].m_info;
        info->moveConstruct(GetElement(destinationColumn, destinationRow),
            source.GetElement(sourceColumn, sourceRow));
    }

    void Chunk::DestroyComponent(size_t columnIndex, size_t row) noexcept
    {
        const ComponentLayout& column = m_layout->m_components[columnIndex];
        // Componentの型に対応するデストラクタを呼び出す
        column.m_info->destroy(GetElement(columnIndex, row));
    }

    void Chunk::DestroyEntity(size_t row) noexcept
    {
        *GetEntity(row) = Entity{};
    }

    void Chunk::CommitRow(size_t row) noexcept
    {
        // 全Componentの構築が完了したrowだけを有効範囲へ加える
        assert(row == m_entityCount);
        ++m_entityCount;
    }

    void Chunk::RemoveLastRow() noexcept
    {
        assert(m_entityCount > 0);
        --m_entityCount;
    }
}
