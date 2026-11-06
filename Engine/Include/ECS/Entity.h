/*=============================================================================

 File   : Entity.h
 Desc   : Chunk内のEntityと、外部で保持する識別Handleを定義する。

------------------------------------------------------------------------------

 Date   : 2026/10/16
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _ENTITY_H_
#define _ENTITY_H_

#include <cstddef>
#include <cstdint>

namespace Hestia
{
    class ECSWorld;

    /// @brief 生成元World内でEntityを識別するHandle
    /// @note indexは30bit、generation 0は無効値とし、全世代周回後の古いHandle再一致は許容する
    struct EntityHandle
    {
        uint32_t m_index = 0;
        uint32_t m_generation = 0;

        bool IsValid() const noexcept
        {
            // Handle内ではindexの上位2bitは使用せず、generation 0は無効値とする
            return m_generation != 0 && (m_index & 0xc0000000u) == 0;
        }

        bool operator==(const EntityHandle&) const noexcept = default;
    };

    class Entity
    {
    public:
        Entity(EntityHandle handle, bool active) noexcept 
            : m_indexAndFlags(handle.m_index | (active ? ACTIVE_MASK : 0)),
              m_generation(handle.m_generation)
        {
        }
        Entity(const Entity&) noexcept = default;
        Entity& operator=(const Entity&) noexcept = default;

        /// @brief Entityの長期識別子を取得する。
        EntityHandle GetHandle() const noexcept { return { GetIndex(), m_generation }; }

        /// @brief Slot表で使用するindexを取得する。
        uint32_t GetIndex() const noexcept { return m_indexAndFlags & INDEX_MASK; }

        /// @brief activeかつ破棄予約されていないかを返す。
        bool IsActive() const noexcept { return (m_indexAndFlags & ACTIVE_MASK) != 0 && !IsDestroy(); }

        /// @brief Worldが破棄予約を設定したかを返す。
        bool IsDestroy() const noexcept { return (m_indexAndFlags & DESTROY_MASK) != 0; }

        /// @brief active bitだけを変更する。
        void SetActive(bool active) noexcept
        {
            if (active)
                m_indexAndFlags |= ACTIVE_MASK;
            else
                m_indexAndFlags &= ~ACTIVE_MASK;
        }

    private:
        friend class ECSWorld;
        friend class Chunk;

        Entity() noexcept = default;

        void MarkDestroy() noexcept
        {
            m_indexAndFlags |= DESTROY_MASK;
        }

        // 下位30bitがindex、上位bitがactiveと破棄予約
        uint32_t m_indexAndFlags = 0;
        // 0を無効値として使い、Slot解放時に更新する
        uint32_t m_generation = 0;

        static constexpr uint32_t INDEX_MASK = 0x3fffffffu;
        static constexpr uint32_t ACTIVE_MASK = 0x40000000u;
        static constexpr uint32_t DESTROY_MASK = 0x80000000u;

    };

    static_assert(sizeof(Entity) == sizeof(uint64_t));

    /// @brief Handle予約から実体破棄までのSlot状態
    enum class EntitySlotState
    {
        Free,           // Handleを割り当てていない
        PendingCreate,  // Handle予約済みで、Chunk内のEntityは未構築
        Alive,          // Chunk内にEntityとComponentが構築済み
    };

    class Archetype;
    class Chunk;

    struct EntitySlot
    {
        uint32_t m_generation = 1;
        EntitySlotState m_state = EntitySlotState::Free;
        Archetype* m_archetype = nullptr;
        Chunk* m_chunk = nullptr;
        size_t m_row = 0;
    };
}

#endif // _ENTITY_H_
