/*=============================================================================

 File   : EntityHandle.h
 Desc   : Engine と Game で共有する Entity 識別子を定義する。

------------------------------------------------------------------------------

 Date   : 2026/11/13
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _ENTITY_HANDLE_H_
#define _ENTITY_HANDLE_H_

#include <cstdint>

namespace Hestia
{
    /// @brief 生成元World内でEntityを識別するHandle。
    /// @note indexは30bit、generation 0は無効値。全世代周回後の古いHandle再一致は許容する。
    struct EntityHandle
    {
        uint32_t m_index = 0;
        uint32_t m_generation = 0;

        /// @brief Handle の形式が有効かを返す。Entity の生存は別途確認する。
        bool IsValid() const noexcept
        {
            return m_generation != 0 && (m_index & 0xc0000000u) == 0;
        }

        bool operator==(const EntityHandle&) const noexcept = default;
    };
}

#endif // _ENTITY_HANDLE_H_
