/*=============================================================================

 File   : Logger.h
 Desc   : 呼び出し情報をコピーしてログ保持先へ渡す記録窓口を定義する。

------------------------------------------------------------------------------

 Date   : 2026/10/08
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _LOGGER_H_
#define _LOGGER_H_

#include <source_location>
#include <string_view>

#include "Log/LogData.h"

namespace Hestia
{
    class LogStorage;

    /// @brief LogSystem が所有するログ受付窓口。static の記録実体は Engine.dll に置く
    /// @note 記録元が停止している間だけ接続を変更する。未接続の記録は何もしない。
    class Logger
    {
    public:
        Logger() = default;
        Logger(const Logger&) = delete;
        Logger& operator=(const Logger&) = delete;
        Logger(Logger&&) = delete;
        Logger& operator=(Logger&&) = delete;

        /// @brief Log の重要度で記録する
        /// @param message UTF-8 の本文(コピー)
        /// @param source 呼び出し元で取得した位置
        static void Log(std::string_view message,
            const std::source_location& source = std::source_location::current());

        /// @brief Warning の重要度で記録する
        /// @param message UTF-8 の本文(コピー)
        /// @param source 呼び出し元で取得した位置
        static void Warning(std::string_view message,
            const std::source_location& source = std::source_location::current());

        /// @brief Error の重要度で記録する
        /// @param message UTF-8 の本文(コピー)
        /// @param source 呼び出し元で取得した位置
        static void Error(std::string_view message,
            const std::source_location& source = std::source_location::current());

        /// @brief Fatal の重要度で記録する。強制終了や同期 flush は行わない。
        /// @param message UTF-8 の本文(コピー)
        /// @param source 呼び出し元で取得した位置。
        static void Fatal(std::string_view message,
            const std::source_location& source = std::source_location::current());

        /// @brief LogStorage への接続とinstanceを登録
        /// @param storage 接続先のLogStorage。nullptrは無効
        /// @return nullptr・接続済み・別 Logger の登録済みなら false
        bool Initialize(LogStorage* storage);

        /// @brief LogStorage への接続を解除し、instanceを解除
        /// @pre 記録元が停止している
        void Finalize();

    private:

        // 指定のLevelでLogEntryを作成し、LogStorageへ渡す
        void Write(LogLevel level, std::string_view message,
            const std::source_location& source);

        LogStorage* m_storage = nullptr;
        static Logger* s_instance;
    };
}

#endif // _LOGGER_H_
