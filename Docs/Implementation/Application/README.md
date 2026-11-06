# Applicationの処理・構造

[全体索引](../README.md) / [型](Types.md)

根拠: [Application.cpp](../../../Application/Source/Application.cpp)、[ApplicationAPI.cpp](../../../Application/Source/ApplicationAPI.cpp)、[Window.cpp](../../../Application/Source/Window.cpp)、[Main.cpp](../../../Application/Source/Main.cpp)

## 起動と所有

`wWinMain` がローカルの `Application` を生成し、Initialize成功後にRun、Finalizeを呼ぶ。初期化失敗時は -1 を返す。ApplicationはWindowを値で持ち、Engine.dllのHMODULE、APIテーブルへの非所有ポインタ、EngineHandle、Waitable Timerを保持する。

Initializeは1600×900のWindow、高精度Waitable Timer、Engineの順に準備する。`LoadLibraryW("Engine.dll")` → `GetProcAddress("GetEngineAPI")` → API取得 → `Create(&appAPI)` と進む。DLLやエントリ、API、Engineの取得失敗はfalseを返す。

## フレーム

1. steady_clockの現在時刻を取得し、Window Messageを処理する。
2. WM_QUITを受けた場合はループを抜ける。
3. 前回時刻との差をdouble秒で蓄積する。
4. 蓄積が固定刻み以上ならEngineのFixedUpdateを繰り返し、刻みを差し引く。
5. FrameExecuteを1回呼ぶ。DLLへ渡す時間はfloatへ変換する。
6. 上限ありの場合は目標時刻まで待機する。

既定値は120FPS、固定刻み1/60秒。WaitUntilは目標の2ms前までTimerとWindow Messageを待ち、残りをbusy waitする。待機中もメッセージを処理する。固定更新回数の上限はない。FPSと固定刻みのsetterは値をそのまま受け取るため、呼び出し側が有限の正値を渡す必要がある。

## Window Messageと終了

WindowProcは接続済みEngineへ先に通知し、WM_CLOSEでDestroyWindow、WM_DESTROYでPostQuitMessageを呼び、最後にDefWindowProcへ渡す。Engine側のProcessMessageは現在空である。

`RequestQuit` はPostQuitMessageを呼ぶ。`UnloadEngine` にはDestroy、API参照解除、FreeLibraryの順の処理があるが、現在の `Application::Finalize` はWindow.Finalizeだけを呼び、UnloadEngineやTimerのCloseHandleは呼ばない。終了時にDLLとTimerまで明示解放されるという仕様にはしない。

Window.FinalizeはWindowクラスの登録解除とCOMの終了を行う。Windowを閉じる経路のDestroyWindowと区別する。Editorの更新箇所はコメントだけである。
