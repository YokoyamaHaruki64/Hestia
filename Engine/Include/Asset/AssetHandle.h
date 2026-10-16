/*=============================================================================

 File   : AssetHandle.h
 Desc   : AssetSystem と Graphics で共有する型付き Asset 識別子を定義する。

------------------------------------------------------------------------------

 Date   : 2026/10/15
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _ASSET_HANDLE_H_
#define _ASSET_HANDLE_H_

#include <cstdint>

namespace Hestia
{
    /// @brief Asset 型と Index / Generation を組み合わせた識別子。
    template<class T>
    struct AssetHandle
    {
        /// @brief AssetStorage と GPUResourceStorage で共有する Index。
        uint32_t m_index = UINT32_MAX;

        /// @brief 将来の Index 再利用に備える世代番号。
        uint32_t m_generation = 0;

        /// @brief 無効値でないかを確認する。
        /// @return Index が無効値でない場合: true。
        bool IsValid() const
        {
            return m_index != UINT32_MAX;
        }
    };
}

#endif // _ASSET_HANDLE_H_
