# Applicationの型

[処理・構造](README.md)

| 型 | 責務・データ・契約 | 定義 |
| --- | --- | --- |
| `Application` | Window、DLL接続、タイマー、固定更新用の蓄積秒、フレーム上限を管理。Initialize / Run / Finalizeが入口。staticのs_instanceをWindowProcから参照する | [Application.h](../../../Application/Include/Application.h) |
| `Application::Clock` | steady_clockの別名。フレーム間隔と待機時刻に使用し、ログのカレンダー時刻とは別 | 同上 |
| `Window` | HWNDとタイトル文字列を保持。InitializeでWindowクラスとWindowを準備し、COM初期化と表示を行う。GetHandleでHWNDを借用させる | [Window.h](../../../Application/Include/Window.h) |
| `Hestia::ApplicationAPI` | context付き関数ポインタを保持する値型。FPS、無制限設定、固定刻み、終了要求、HWND取得を公開。Applicationがfriendとして接続を構成する | [ApplicationAPI.h](../../../Application/Include/ApplicationAPI.h) |

ApplicationAPIをコピーしてもApplicationの所有権は移らない。contextのApplicationはAPI利用中生存している必要がある。未接続のsetterと終了要求は何もせず、数値getterは0、無制限getterはfalse、HWNDはnullptrを返す。EngineAPIの寿命は [Engine](../Engine/Types.md) を参照。
