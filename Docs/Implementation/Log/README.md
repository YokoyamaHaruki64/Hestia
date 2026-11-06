# Logの処理・構造

[全体索引](../README.md) / [型](Types.md)

根拠: [LogSystem.cpp](../../../Engine/Source/Log/LogSystem.cpp)、[Logger.cpp](../../../Engine/Source/Log/Logger.cpp)、[LogStorage.cpp](../../../Engine/Source/Log/LogStorage.cpp)、[LogProcess.cpp](../../../Engine/Source/Log/LogProcess.cpp)、[LogAPI.cpp](../../../Engine/Source/Log/LogAPI.cpp)

## 所有と受付

LogSystemはLogger、LogStorage、LogProcess、処理thread、Drain用vectorを所有する。設定を検査し、Storage、出力ファイル、バッチ容量、受付、thread、Logger接続を準備する。失敗時は準備した部分を解放する。同時に稼働するSystemは一つ。

既定設定は `../Logs` 以下の時刻付きtxt、保持上限4096件、Drain単位256件。ファイル名はローカル時刻で生成する。明示設定では空path、容量0、Drain 0、Drainが容量を超える設定を拒否する。

Loggerは本文、source_locationのファイル・関数・行、system_clock時刻、thread IDをLogEntryにコピーする。呼び出し元文字列の借用を継続しない。

## データの流れ

記録元 → Logger → 書込側pending → 読取側pending → queue → Drain用vector → LogProcess → ファイルの順。二つのpendingをmutex内で交換し、単一処理threadが移送・Drain・書き込みを進める。総保持件数が上限なら新しい記録を拒否し、drop件数を増やす。ログが常に無損失になるとは保証しない。

出力はUTCのミリ秒付き時刻、level、thread、ファイル・行・関数を持つヘッダーと、2空白で字下げした本文。本文のCR/LFを改行として扱い、NULはエスケープする。ヘッダーの制御文字やバックスラッシュもエスケープする。

## 終了と制約

記録元を先に停止し、Logger解除、Storage受付停止、thread join、残件処理、ファイルflush/close、Storage解放を行う。出力先が正常な場合に受付済み残件を処理する。ファイルの異常時はProcessが戻るため、永続化を保証しない。threadのRunは受付中または残件がある間ループし、空時のsleepや条件変数待ちは実装されていない。
