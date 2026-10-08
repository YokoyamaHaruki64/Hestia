# コードとコメントの記述例

[CodingStandard](CodingStandard.md) の書式とコメントの付け方を示す例
LogProcess を題材に、ヘッダーと実装の一部を抜粋している
ファイルヘッダーは CodingStandard の例を参照

## ヘッダーの例

公開 API の概要・引数・戻り値・前提は Doxygen で記載し、内部処理は private に配置
クラス名や関数名は日本語へ言い換えず、コードの識別子をそのまま使用

```cpp
#ifndef _LOG_PROCESS_H_
#define _LOG_PROCESS_H_

#include <filesystem>
#include <fstream>
#include <span>
#include <string_view>

#include "Log/LogData.h"

namespace Hestia
{
    /// @brief LogEntry の batch をファイルへ出力
    /// @note Process と Initialize/Finalize は並行して呼ばない
    class LogProcess
    {
    public:
        /// @brief filePath をテキストログの追記先として開く
        /// @param filePath 親フォルダが存在する出力先。空パスは無効
        /// @return 初期化成功: true / ファイルを開けない場合や初期化済み: false
        bool Initialize(const std::filesystem::path& filePath);

        /// @brief flush・close して終了
        /// @note 終了済みの場合は何もしない
        void Finalize();

        /// @brief LogEntryをヘッダーと本文に分けて出力
        /// @param entries 出力する LogEntry の配列
        /// @pre Initialize に成功している
        /// @note 本文の改行はそのまま出力。書き込み失敗はLoggerへ再記録しない
        void Process(std::span<const LogEntry> entries);

    private:
        // 制御文字をエスケープして出力
        void WriteText(std::string_view text);
        void WriteMessage(std::string_view text);

        std::ofstream m_stream;
    };
}

#endif // _LOG_PROCESS_H_
```

実コード: [LogProcess.h](../Engine/Include/Log/LogProcess.h)

## 実装の例

単文の if は波括弧を省略し、処理できない条件は早期 return
処理単位を空行と見出しコメントで区切り、時刻変換・重要度の変換・出力の順序を示す
複数行の処理を持つ分岐には波括弧を使用

```cpp
#include "pch.h"
#include "Log/LogProcess.h"

#include <ctime>
#include <iomanip>

namespace Hestia
{
    void LogProcess::Process(std::span<const LogEntry> entries)
    {
        if (entries.empty() || !m_stream.good() || !m_stream.is_open())
            return;

        for (const auto& entry : entries)
        {
            // 時刻の秒とミリ秒を取得
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

            // LogLevel を出力文字列に変換
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
```

実コード: [LogProcess.cpp](../Engine/Source/Log/LogProcess.cpp)

見出しだけでは分からない前提や理由は、該当する処理の近くへ短く補足
例えば LogStorage の保持件数更新では、単なる「件数を加算」ではなく順序の理由を記載

```cpp
m_pending[m_writeIndex].push(std::move(entry));

// index の交換前に保持件数を反映し、LogStorage の処理側への公開と整合させる
m_bufferedCount.fetch_add(1);
```

実コード: [LogStorage.cpp](../Engine/Source/Log/LogStorage.cpp)
