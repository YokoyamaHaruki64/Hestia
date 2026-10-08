/*=============================================================================

 File   : LogAPI.cpp
 Desc   : Game の記録要求と呼び出し位置を Engine の Logger へ渡す。

------------------------------------------------------------------------------

 Date   : 2026/10/08
 Author : Yokoyama Haruki

=============================================================================*/

#include "pch.h"
#include "Log/LogAPI.h"
#include "Log/Logger.h"

namespace Hestia
{
    void LogAPI::Initialize(LogSystem* system)
    {
        assert(system != nullptr);
        m_system = system;
    }

    void LogAPI::Finalize()
    {
        m_system = nullptr;
    }

    void LogAPI::Log(std::string_view message, const std::source_location& source)
    {
        Logger::Log(message, source);
    }

    void LogAPI::Warning(std::string_view message, const std::source_location& source)
    {
        Logger::Warning(message, source);
    }

    void LogAPI::Error(std::string_view message, const std::source_location& source)
    {
        Logger::Error(message, source);
    }

    void LogAPI::Fatal(std::string_view message, const std::source_location& source)
    {
        Logger::Fatal(message, source);
    }
}
