/*=============================================================================

 File   : LogProcess.cpp
 Desc   : ログの時刻・位置・本文を整形し、UTF-8 の追記ファイルへ書き込む。

------------------------------------------------------------------------------

 Date   : 2026/10/08
 Author : Yokoyama Haruki

=============================================================================*/

#include "pch.h"
#include "Log/LogProcess.h"

#include <ctime>
#include <iomanip>

namespace Hestia
{
    bool LogProcess::Initialize(const std::filesystem::path& filePath)
    {
        if (filePath.empty() || m_stream.is_open())
            return false;

        m_stream.clear();
        m_stream.open(filePath, std::ios::out | std::ios::app | std::ios::binary);
        return m_stream.is_open() && m_stream.good();
    }

    void LogProcess::Finalize()
    {
        if (m_stream.is_open())
        {
            m_stream.flush();
            m_stream.close();
        }
    }

    void LogProcess::WriteText(std::string_view text)
    {
        for (char character : text)
        {
            switch (character)
            {
            case '\n': m_stream << "\\n"; break;
            case '\r': m_stream << "\\r"; break;
            case '\t': m_stream << "\\t"; break;
            case '\\': m_stream << "\\\\"; break;
            case '\0': m_stream << "\\0"; break;
            default: m_stream.put(character); break;
            }
        }
    }

    void LogProcess::WriteMessage(std::string_view text)
    {
        m_stream << "  ";
        bool endsWithNewline = false;

        for (std::size_t index = 0; index < text.size(); ++index)
        {
            const char character = text[index];
            if (character == '\r' || character == '\n')
            {
                if (character == '\r' && index + 1 < text.size()
                    && text[index + 1] == '\n')
                {
                    ++index;
                }

                m_stream.put('\n');
                endsWithNewline = true;
                if (index + 1 < text.size())
                {
                    m_stream << "  ";
                    endsWithNewline = false;
                }
                continue;
            }

            if (character == '\0')
                m_stream << "\\0";
            else
                m_stream.put(character);

            endsWithNewline = false;
        }

        if (!endsWithNewline)
            m_stream.put('\n');
    }

    void LogProcess::Process(std::span<const LogEntry> entries)
    {
        if (entries.empty() || !m_stream.good() || !m_stream.is_open())
            return;

        for (const auto& entry : entries)
        {
            // 時刻変換
            const auto seconds = std::chrono::floor<std::chrono::seconds>(entry.m_timestamp);
            const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
                entry.m_timestamp - seconds).count();

            const std::time_t time = std::chrono::system_clock::to_time_t(seconds);

            // UTC に変換
            std::tm utc = {};
            if (gmtime_s(&utc, &time) != 0)
            {
                m_stream.setstate(std::ios::failbit);
                return;
            }

            const char* level = "Log";
            switch (entry.m_level)
            {
            case LogLevel::Warning: level = "Warning"; break;
            case LogLevel::Error: level = "Error"; break;
            case LogLevel::Fatal: level = "Fatal"; break;
            case LogLevel::Log: break;
            }

            // ヘッダーを出力
            m_stream << '[' << std::put_time(&utc, "%Y-%m-%dT%H:%M:%S")
                << '.' << std::setfill('0') << std::setw(3) << milliseconds
                << "Z] [" << level << "] [thread:" << entry.m_threadId << "] (";

            WriteText(entry.m_fileName);
            m_stream << ':' << entry.m_line << " ";

            WriteText(entry.m_functionName);
            m_stream << ")\n";

            // 本文を出力
            WriteMessage(entry.m_message);

            if (!m_stream.good())
                return;
        }
    }
}
