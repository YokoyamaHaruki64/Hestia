/*=============================================================================

 File   : Chunk.h
 Desc   : Entity列とComponent列の固定サイズ保存領域を所有する。

------------------------------------------------------------------------------

 Date   : 2026/10/16
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _CHUNK_H_
#define _CHUNK_H_

#include <cstddef>
#include <span>

#include "ECS/Archetype.h"
#include "ECS/Entity.h"

namespace Hestia
{
    /// @brief EntityとComponentを同じrowで保持する固定サイズChunk。
    /// @note Entity列を保存領域の先頭に置き、Component列はLayoutのoffsetにSoA配置する
    class Chunk
    {
    public:
        explicit Chunk(const ChunkLayout& layout);
        ~Chunk();

        Chunk(const Chunk&) = delete;
        Chunk& operator=(const Chunk&) = delete;

        /// @brief 構築済みrow数を返す。
        size_t GetEntityCount() const noexcept { return m_entityCount; }

        /// @brief Layoutから計算した最大row数を返す。
        size_t GetCapacity() const noexcept { return m_layout->m_capacity; }

        /// @brief Archetypeが所有する共通Layoutを取得する。
        const ChunkLayout& GetLayout() const noexcept { return *m_layout; }

        /// @brief 構築済みEntity列を取得する。
        /// @note active状態に関係なく、構築済みの全rowを返す
        std::span<const Entity> GetEntities() const noexcept { return { reinterpret_cast<const Entity*>(m_data), m_entityCount }; }

        /// @brief 構築済みrowのEntityを取得する。
        Entity* GetEntity(size_t row) noexcept { return reinterpret_cast<Entity*>(m_data) + row; }
        const Entity* GetEntity(size_t row) const noexcept { return reinterpret_cast<const Entity*>(m_data) + row; }

        /// @brief Component列の指定rowを取得する。
        void* GetElement(size_t columnIndex, size_t row) noexcept
        {
            const ComponentLayout& column = m_layout->m_components[columnIndex];
            return m_data + column.m_offset + row * column.m_info->size;
        }
        const void* GetElement(size_t columnIndex, size_t row) const noexcept
        {
            const ComponentLayout& column = m_layout->m_components[columnIndex];
            return m_data + column.m_offset + row * column.m_info->size;
        }

        /// @brief Entity実体を未使用rowへ構築する。
        Entity* ConstructEntity(size_t row, EntityHandle handle, bool active);

        /// @brief 既存Entityの識別子と状態bitを未使用rowへ複製する。
        Entity* ConstructEntityCopy(size_t row, const Entity& source);

        /// @brief Componentの既定値を未使用rowへ構築する。
        void ConstructDefault(size_t columnIndex, size_t row);

        /// @brief Componentを未構築の移行先rowへMove構築する。
        void MoveConstruct(size_t destinationColumn, size_t destinationRow,
            Chunk& source, size_t sourceColumn, size_t sourceRow) noexcept;

        /// @brief 指定rowのComponentを破棄する。
        void DestroyComponent(size_t columnIndex, size_t row) noexcept;

        /// @brief 指定rowのEntity実体を破棄する。
        void DestroyEntity(size_t row) noexcept;

        /// @brief 成功したrowを有効行として追加する。
        void CommitRow(size_t row) noexcept;

        /// @brief 構築済み最終rowを有効数から除く。
        void RemoveLastRow() noexcept;

    private:
        const ChunkLayout* m_layout = nullptr;
        std::byte* m_data = nullptr;
        size_t m_entityCount = 0;
    };
}

#endif // _CHUNK_H_
