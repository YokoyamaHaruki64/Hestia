/*=============================================================================

 File   : LogProcess.h
 Desc   : ログ batch の整形とファイル出力を定義する。

------------------------------------------------------------------------------

 Date   : 2026/10/08
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _LOG_PROCESS_H_
#define _LOG_PROCESS_H_

#include <filesystem>
#include <fstream>
#include <span>
#include <string_view>

#include "Log/LogData.h"

namespace Hestia
{
    /// @brief batch をファイルへ出力する。
    /// @note Process と Initialize/Finalize を並行して呼ばない。
    class LogProcess
    {
    public:
        /// @brief PathをUTF-8のテキストログの追記先として開く。
        /// @param filePath 親フォルダが存在する出力先。空パスは無効。
        /// @return 開くことができた場合: true / 稼働中の再初期化: false
        bool Initialize(const std::filesystem::path& filePath);

        /// @brief 終了処理として flush・close する。終了済みの場合は何もしない。
        void Finalize();

        /// @brief 重要度・UTC 時刻・位置をヘッダー、本文を2スペース下げて出力する。
        /// @param entries 出力するLogEntryの配列
        /// @note 本文の改行はそのまま出力する。書き込み失敗はLoggerへ再記録しない。
        void Process(std::span<const LogEntry> entries);

    private:
        void WriteText(std::string_view text);
        void WriteMessage(std::string_view text);

        std::ofstream m_stream;
    };
}

#endif // _LOG_PROCESS_H_
