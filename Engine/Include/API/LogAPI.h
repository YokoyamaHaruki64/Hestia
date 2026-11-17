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

namespace Hestia
{
    class LogSystem;

    /// @brief Logger の static 記録窓口へ委譲する Game 向け API。
    /// @note 接続の変更は Game の記録元が停止している間に行う。未接続時の各ポインタは nullptr。
    class LogAPI
    {
    public:
        /// @brief Engine 側の Logger に接続する。
        /// @param system Game の利用期間を通して生存する LogSystem。
        void Initialize(LogSystem* system);

        /// @brief Logger への接続を解除する。
        void Finalize();

        /// @brief Logger::Log へ本文と位置をそのまま渡す。
        /// @param message UTF-8 の本文。
        /// @param source Game の呼び出し元で取得した位置。
        void (*Log)(std::string_view message, const std::source_location& source) = nullptr;

        /// @brief Logger::Warning へ本文と位置をそのまま渡す。
        /// @param message UTF-8 の本文。
        /// @param source Game の呼び出し元で取得した位置。
        void (*Warning)(std::string_view message, const std::source_location& source) = nullptr;

        /// @brief Logger::Error へ本文と位置をそのまま渡す。
        /// @param message UTF-8 の本文。
        /// @param source Game の呼び出し元で取得した位置。
        void (*Error)(std::string_view message, const std::source_location& source) = nullptr;

        /// @brief Logger::Fatal へ本文と位置をそのまま渡す。
        /// @param message UTF-8 の本文。
        /// @param source Game の呼び出し元で取得した位置。
        void (*Fatal)(std::string_view message, const std::source_location& source) = nullptr;
    };
}

#endif // _LOG_API_H_
