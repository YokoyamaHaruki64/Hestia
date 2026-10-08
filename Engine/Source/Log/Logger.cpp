/*=============================================================================

 File   : Logger.cpp
 Desc   : 時刻・位置・本文を所有する記録を生成し、保持先へ配送する。

------------------------------------------------------------------------------

 Date   : 2026/10/08
 Author : Yokoyama Haruki

=============================================================================*/

#include "pch.h"
#include "Log/Logger.h"
#include "Log/LogStorage.h"

namespace Hestia
{
    Logger* Logger::s_instance = nullptr;

    void Logger::Log(std::string_view message, const std::source_location& source)
    {
        if (s_instance != nullptr)
            s_instance->Write(LogLevel::Log, message, source);
    }

    void Logger::Warning(std::string_view message, const std::source_location& source)
    {
        if (s_instance != nullptr)
            s_instance->Write(LogLevel::Warning, message, source);
    }

    void Logger::Error(std::string_view message, const std::source_location& source)
    {
        if (s_instance != nullptr)
            s_instance->Write(LogLevel::Error, message, source);
    }

    void Logger::Fatal(std::string_view message, const std::source_location& source)
    {
        if (s_instance != nullptr)
            s_instance->Write(LogLevel::Fatal, message, source);
    }

    bool Logger::Initialize(LogStorage* storage)
    {
        if (storage == nullptr || m_storage != nullptr || s_instance != nullptr)
            return false;

        m_storage = storage;
        s_instance = this;
        return true;
    }

    void Logger::Finalize()
    {
        if (s_instance == this)
            s_instance = nullptr;

        m_storage = nullptr;
    }

    void Logger::Write(LogLevel level, std::string_view message,
        const std::source_location& source)
    {
        LogEntry entry;
        entry.m_level = level;
        entry.m_timestamp = std::chrono::system_clock::now();
        entry.m_threadId = std::this_thread::get_id();

        entry.m_message = std::string(message);
        entry.m_fileName = source.file_name();
        entry.m_functionName = source.function_name();
        entry.m_line = source.line();
        m_storage->Push(std::move(entry));
    }
}
