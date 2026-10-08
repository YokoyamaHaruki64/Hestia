/*=============================================================================

 File   : LogAPI.h
 Desc   : Game にログ記録だけを公開する DLL 境界 API を定義する。

------------------------------------------------------------------------------

 Date   : 2026/10/08
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _LOG_API_H_
#define _LOG_API_H_

#include <source_location>
#include <string_view>

#include "Common/Include/DllAPI.h"

namespace Hestia
{
    class LogSystem;

    /// @brief Logger の static 記録窓口へ委譲する Game 向け API。
    /// @note 接続の変更は Game の記録元が停止している間に行う。
    class HESTIA_ENGINE_API LogAPI
    {
    public:
        /// @brief 初期化済み LogSystem への拡張用の非所有参照を接続する。
        /// @param system Game の利用期間を通して生存する LogSystem。
        void Initialize(LogSystem* system);

        /// @brief 拡張用の非所有参照を解除する。
        void Finalize();

        /// @brief Logger::Log へ本文と位置をそのまま渡す。
        /// @param message UTF-8 の本文。
        /// @param source Game の呼び出し元で取得した位置。
        void Log(std::string_view message,
            const std::source_location& source = std::source_location::current());

        /// @brief Logger::Warning へ本文と位置をそのまま渡す。
        /// @param message UTF-8 の本文。
        /// @param source Game の呼び出し元で取得した位置。
        void Warning(std::string_view message,
            const std::source_location& source = std::source_location::current());

        /// @brief Logger::Error へ本文と位置をそのまま渡す。
        /// @param message UTF-8 の本文。
        /// @param source Game の呼び出し元で取得した位置。
        void Error(std::string_view message,
            const std::source_location& source = std::source_location::current());

        /// @brief Logger::Fatal へ本文と位置をそのまま渡す。
        /// @param message UTF-8 の本文。
        /// @param source Game の呼び出し元で取得した位置。
        void Fatal(std::string_view message,
            const std::source_location& source = std::source_location::current());

    private:
        LogSystem* m_system = nullptr;
    };
}

#endif // _LOG_API_H_
