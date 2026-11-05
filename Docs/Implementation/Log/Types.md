# Logの型

[処理・構造](README.md)

| 型 | 責務・契約 | 定義 |
| --- | --- | --- |
| `LogLevel` | Log、Warning、Error、Fatalの重要度 | [LogData.h](../../../Engine/Include/Log/LogData.h) |
| `LogSettings` | 出力path、総保持容量、Drain件数。既定構築ではpath空・件数0で、そのまま明示初期化へ渡すと無効 | 同上 |
| `LogEntry` | 本文・呼出位置文字列・時刻・level・thread IDを値で保持する一件の記録 | 同上 |
| `Logger` | static記録入口と現在のStorage接続を管理。LogEntryを作りPushへ渡す | [Logger.h](../../../Engine/Include/Log/Logger.h) |
| `LogStorage` | 二つのpendingとqueue、受付状態、保持件数、drop件数を管理。記録元と処理側の受け渡しを担当 | [LogStorage.h](../../../Engine/Include/Log/LogStorage.h) |
| `LogProcess` | ofstreamを所有。spanのLogEntryを整形して追記出力し、Finalizeでflush/close | [LogProcess.h](../../../Engine/Include/Log/LogProcess.h) |
| `LogSystem` | 上記の寿命と単一threadを統括。Initialize / Finalizeは管理threadで直列に呼ぶ | [LogSystem.h](../../../Engine/Include/Log/LogSystem.h) |
| `LogAPI` | DLL境界のログ受付入口。Systemへの接続を初期化・解除し、記録をログ側へ渡す | [LogAPI.h](../../../Engine/Include/Log/LogAPI.h) |

pendingの書込と切替の排他があることと、Initialize / Finalizeと記録を自由に競合させられることは別である。終了時は記録元を先に停止する。

LogAPIのSystem参照は拡張用であり、現行のLog / Warning / Error / Fatalは接続先メンバを経由せずLoggerのstatic関数へ直接委譲する。したがってLogAPI.FinalizeだけでLogger全体の受付が停止するわけではない。
