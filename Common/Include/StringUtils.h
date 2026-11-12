/*=============================================================================

 File   : StringUtils.h
 Desc   : 文字列に関する共通処理を定義する。

------------------------------------------------------------------------------

 Date   : 2026/11/06
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _STRING_UTILS_H_
#define _STRING_UTILS_H_

#include <cstdint>
#include <string_view>

namespace Hestia
{
    /// @brief UTF-8 byte列を FNV-1a 64bit でハッシュする
    /// @param value ハッシュする UTF-8 byte列
    /// @return 64bit ハッシュ値
    inline constexpr uint64_t StringToHash(std::string_view value) noexcept
    {
        constexpr uint64_t offsetBasis = 14695981039346656037ull;
        constexpr uint64_t prime = 1099511628211ull;
        uint64_t hash = offsetBasis;

        for (const unsigned char byte : value)
        {
            hash ^= byte;
            hash *= prime;
        }

        return hash;
    }
}

#endif // _STRING_UTILS_H_
