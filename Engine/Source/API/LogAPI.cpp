/*=============================================================================

 File   : LogAPI.cpp
 Desc   : Game の記録要求と呼び出し位置を Engine の Logger へ渡す。

------------------------------------------------------------------------------

 Date   : 2026/10/08
 Author : Yokoyama Haruki

=============================================================================*/

#include "pch.h"
#include "API/LogAPI.h"
#include "Log/Logger.h"

namespace Hestia
{
    void LogAPI::Initialize(LogSystem* system)
    {
        assert(system != nullptr);
        if (system == nullptr)
        {
            Finalize();
            return;
        }

        // 各レベルを Engine.dll 内の Logger に結び付ける
        Log = &Logger::Log;
        Warning = &Logger::Warning;
        Error = &Logger::Error;
        Fatal = &Logger::Fatal;
    }

    void LogAPI::Finalize()
    {
        *this = {};
    }
}
