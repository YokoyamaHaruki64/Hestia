# Applicationの型

[処理・構造](README.md)

| 型 | 責務・データ・契約 | 定義 |
| --- | --- | --- |
| `Application` | Window、DLL接続、タイマー、固定更新用の蓄積秒、フレーム上限を管理。Initialize / Run / Finalizeが入口。staticのs_instanceをWindowProcから参照する | [Application.h](../../../Application/Include/Application.h) |
| `Application::Clock` | steady_clockの別名。フレーム間隔と待機時刻に使用し、ログのカレンダー時刻とは別 | 同上 |
| `Window` | HWND、タイトル、メニューのパスとコマンドの一覧を保持する。Window の生成、COM 初期化、メニュー登録・配送、終了を行う | [Window.h](../../../Application/Include/Window.h) |
| `Window::MenuCommand` | メニュー項目の関数ポインタと借用 context の組。コマンド ID は格納先の添字に 1 を足した値 | 同上 |
| `Hestia::ApplicationAPI` | context 付き関数ポインタを保持する値型。FPS と無制限設定、固定刻み、終了要求、HWND の取得、`RegisterMenuItem` を公開する。Application が接続を構成する | [ApplicationAPI.h](../../../Application/Include/ApplicationAPI.h) |

ApplicationAPIをコピーしてもApplicationの所有権は移らない。contextのApplicationはAPI利用中生存している必要がある。未接続のsetterと終了要求は何もせず、数値getterは0、無制限getterはfalse、HWNDはnullptrを返す。EngineAPIの寿命は [Engine](../Engine/Types.md) を参照。

`RegisterMenuItem` は登録できたときに `true` を返す。呼び出し側が渡すコールバックと context はコマンド解除まで生存する必要がある。コールバックは App の UI スレッドで実行する。 [Editor のメニュー](README.md#editor-のメニュー)
