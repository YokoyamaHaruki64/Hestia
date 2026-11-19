/*=============================================================================

 File   : GameLogger.h
 Desc   : GameRuntime から Engine のログを利用する Facade を定義する。

------------------------------------------------------------------------------

 Date   : 2026/11/17
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _HESTIA_GAME_LOGGER_H_
#define _HESTIA_GAME_LOGGER_H_

#include "Engine/Include/API/LogAPI.h"

namespace HestiaGame
{
    namespace Detail { class GameRuntime; }

    /// @brief Game の呼び出し位置を添えて Engine のログへ記録する。
    /// @note GameRuntime が未接続の場合は記録しない。
    class Logger
    {
    public:
        /// @brief Log レベルで記録する。
        static void Log(std::string_view message,
            const std::source_location& source = std::source_location::current())
        {
            if (s_api != nullptr && s_api->Log != nullptr)
                s_api->Log(message, source);
        }

        /// @brief Warning レベルで記録する。
        static void Warning(std::string_view message,
            const std::source_location& source = std::source_location::current())
        {
            if (s_api != nullptr && s_api->Warning != nullptr)
                s_api->Warning(message, source);
        }

        /// @brief Error レベルで記録する。
        static void Error(std::string_view message,
            const std::source_location& source = std::source_location::current())
        {
            if (s_api != nullptr && s_api->Error != nullptr)
                s_api->Error(message, source);
        }

        /// @brief Fatal レベルで記録する。
        static void Fatal(std::string_view message,
            const std::source_location& source = std::source_location::current())
        {
            if (s_api != nullptr && s_api->Fatal != nullptr)
                s_api->Fatal(message, source);
        }

    private:
        friend class Detail::GameRuntime;

        static void Bind(const Hestia::LogAPI* api) noexcept { s_api = api; }
        static void Unbind() noexcept { s_api = nullptr; }

        inline static const Hestia::LogAPI* s_api = nullptr;
    };
}

#endif // _HESTIA_GAME_LOGGER_H_
