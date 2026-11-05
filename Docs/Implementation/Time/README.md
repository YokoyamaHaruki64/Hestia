# Timeの処理・構造

[全体索引](../README.md) / [型](Types.md)

根拠: [TimeSystem.cpp](../../../Engine/Source/Time/TimeSystem.cpp)、[TimeAPI.cpp](../../../Engine/Source/Time/TimeAPI.cpp)

TimeSystemは時計を直接読まず、ApplicationからEngine経由で渡された秒数を保持する。InitializeはTimeDataを既定値へ戻し、単一のstatic instanceを接続する。Finalizeは接続を解除する。初期化・終了・参照は同一スレッドで行う。

`Update(dt)` はunscaledDeltaTimeにdt、deltaTimeにdt×timeScaleを設定し、それぞれのtotalTimeへ加算する。`UpdateFixed(dt)` は固定更新の二つのdeltaだけを設定し、通常更新のtotalTimeを加算しない。入力dtは有限かつ非負であることをassertする。

SetTimeScaleは有限の非負値だけを採用する。負数、NaN、無限大は無視する。0ではscaled時間が進まないがunscaled時間は進む。固定更新の呼び出し頻度自体はApplicationの蓄積時間で決まり、timeScaleでは変更しない。

TimeAPIはSystemとそのTimeDataを非所有参照し、getterはデータを直接読む。setterだけSystemへ委譲する。Finalizeで参照を解除し、その後はgetterを利用しない。
