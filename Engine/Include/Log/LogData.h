/*=============================================================================

 File   : LogData.h
 Desc   : LogSystem が共有する設定とログ記録データを定義する。

------------------------------------------------------------------------------

 Date   : 2026/10/08
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _LOG_DATA_H_
#define _LOG_DATA_H_

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <thread>

namespace Hestia
{
    /// @brief ログ記録の重要度
    enum class LogLevel
    {
        Log,
        Warning,
        Error,
        Fatal
    };

    /// @brief LogSystem の初期化設定
    struct LogSettings
    {
        std::filesystem::path m_filePath;
        std::size_t m_storageCapacity = 0;
        std::size_t m_drainCount = 0;
    };

    /// @brief 受付時の本文・呼び出し情報を所有するログ記録
    /// @note source_location や呼び出し元の文字列への参照を保持しない。
    struct LogEntry
    {
        LogLevel m_level = LogLevel::Log;
        std::string m_message;
        std::chrono::system_clock::time_point m_timestamp;
        std::string m_fileName;
        std::string m_functionName;
        std::uint_least32_t m_line = 0;
        std::thread::id m_threadId;
    };
}

#endif // _LOG_DATA_H_
