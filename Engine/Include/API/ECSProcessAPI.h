/*=============================================================================

 File   : ECSProcessAPI.h
 Desc   : 個別 Process の型付き公開 API と登録済みハンドルを結び付ける。

------------------------------------------------------------------------------

 Date   : 2026/11/16
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _ECS_PROCESS_API_H_
#define _ECS_PROCESS_API_H_

#include "EntityAPI.h"

namespace Hestia
{
    /// @brief 個別操作テーブルと、追加・検索に使う登録済み Process を保持する。
    template<class API>
    struct ECSProcessBinding
    {
        API m_api{};                // APIクラス
        ProcessHandle m_process;    // APIに対応するProcessのハンドル
    };
}

#include "Generated/ECSProcessAPI.generated.h"

#endif // _ECS_PROCESS_API_H_
