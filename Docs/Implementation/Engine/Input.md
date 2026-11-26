# InputSystem の実装

[全体索引](../README.md) ・ [Engine の更新順](README.md#更新) ・ [InputSystem の設計](../../Design/InputSystem.md)

## 入力経路と寿命

`InputSystem` はキーボード・マウスの状態とカーソル設定を静的に保持する。Application が Window を所有し、Engine は起動時に借りた HWND を `InputSystem::Initialize` に渡す。マウスの Raw Input は通常のマウスメッセージを残したままフォアグラウンド入力として登録する。HWND が空なら登録を省き、登録に失敗した場合は Engine がログを出して初期化を中断する。 [InputSystem.cpp](../../../Engine/Source/Input/InputSystem.cpp) ・ [Engine.cpp](../../../Engine/Source/Engine.cpp)

`Application::WindowProc` からのメッセージを `Engine::ProcessMessage` が受け、`KeyboardHook`、`MouseHook` の順に渡す。GameRuntime は `GameEngineAPI` から `InputAPI` を借りて `HestiaGame::Input` に接続し、Script の終了処理後に切断する。Engine はその後、Window が有効な間にカーソル制限と Raw Input の登録を解除する。 [Application.cpp](../../../Application/Source/Application.cpp) ・ [GameRuntime.cpp](../../../Game/Source/GameRuntime.cpp)

## キー・ボタンの状態

キーボードは押下中・押下開始・解放の3本の `std::bitset<256>` を持ち、仮想キーコードを添字にする。通常キーとシステムキーの押下・解放通知を受け、すでに押されているキーの繰り返し通知では押下開始を追加しない。Shift・Ctrl・Alt はスキャンコードと拡張フラグから左右を識別し、共通キーは左右のいずれかが押されている間は押下中を保つ。 [InputSystem.h](../../../Engine/Include/Input/InputSystem.h) ・ [InputSystem.cpp](../../../Engine/Source/Input/InputSystem.cpp)

マウスはフォーカス中の `WM_INPUT` から `RAWMOUSE` を読み、左・右・中・`X1`・`X2` の5ボタンを同じ3種類の状態で保持する。相対移動とホイール量をフレーム内で積算する。絶対座標のパケットからはボタンとホイールを取得する。`WM_INPUT` は読み取り後も Application の既定処理に進む。 [InputSystem.cpp](../../../Engine/Source/Input/InputSystem.cpp) ・ [Application のメッセージ処理](../Application/README.md#window-messageと終了)

同じフレーム内に押して離した場合、押下開始と解放の両方を取得できる。照会でフラグや差分は消費されないため、固定更新と通常更新は同じ値を読める。差分や遷移をフレームごとに一度使う処理は通常の `Update` に置く。Engine は Scene の保存・切り替えの後に `InputSystem::EndFrame` を呼び、押下開始・解放と移動・スクロール量を消す。押下状態は保持し、待機中に受けた入力は次フレームへ渡る。 [Engine の更新順](README.md#更新)

## Game 向け API

| `HestiaGame::Input` | 戻り値と操作 |
| --- | --- |
| `KeyHold`、`KeyDown`、`KeyRelease` | 指定した `Key` の押下中・今フレームの押下開始・解放。 |
| `MouseHold`、`MouseDown`、`MouseRelease` | 指定した `MouseButton` の押下中・今フレームの押下開始・解放。 |
| `MousePosition()` | 呼び出し時点のクライアント座標をピクセル単位の `Vector2` で返す。原点は左上、右・下が正。領域外では範囲外の座標をそのまま返す。中央固定中は中央を指す。 |
| `DeltaPosition()` | 相対移動を Raw Input のカウント単位の `Vector2` で返す。右・下が正で、中央固定中も取得できる。 |
| `DeltaScroll()` | 横・縦のスクロール量を `Vector2` で返す。横は右、縦は奥が正。1目盛りが `1.0` で、小数も保持する。 |
| `SetCursorLockMode`、`GetCursorLockMode` | 移動制限モードを設定・照会する。初期値は `None`。 |
| `SetCursorVisible`、`GetCursorVisible` | 表示設定を指定・照会する。初期値は `true`。 |

未接続時はボタン・キーの照会が `false`、座標と差分がゼロ、カーソル設定の照会が初期値を返す。Setter は未接続時に何も行わない。`Player::Update` は `KeyHold` で WASD を読む。 [GameInput.h](../../../Game/Include/GameInput.h) ・ [InputAPI.h](../../../Engine/Include/API/InputAPI.h) ・ [Player.cpp](../../../Assets/Gameplay/Player.cpp)

## カーソル制御

| モード | フォーカス中の動作 |
| --- | --- |
| `None` | 自由に移動し、クライアント領域では表示設定に従う。 |
| `Confined` | クライアント領域内に制限し、表示設定に従う。 |
| `Locked` | クライアント領域の中央1ピクセルに制限し、非表示にする。表示設定は保持する。 |

`Confined` と `Locked` は制限矩形をスクリーン座標へ変換して `ClipCursor` に渡す。`Locked` では先に中央1ピクセルの矩形を作り、その左上である中央座標を `SetCursorPos` に渡す。Window の移動やリサイズで制限を再計算する。クライアント領域の `WM_SETCURSOR` で非表示を適用した場合、Engine が処理済みと `TRUE` を返し、Application はその値を `WindowProc` の結果に使う。 [InputSystem.cpp](../../../Engine/Source/Input/InputSystem.cpp) ・ [EngineAPI.h](../../../Engine/Include/API/EngineAPI.h)

`WM_KILLFOCUS` でキー・ボタンの状態と差分をクリアし、カーソル制限を解除する。カーソル設定は保持し、`WM_SETFOCUS` で再適用する。 [InputSystem.cpp](../../../Engine/Source/Input/InputSystem.cpp)

## 検証記録

2026-11-27 に App と Game.dll の `Debug_Editor|x64` ビルド、キーボードの回帰テスト2件、擬似 Win32 入力によるマウスの状態・カーソル制御の検証が成功した。同時に実行した Engine 生成関連3件は、空の HWND を渡すテストで既存の Graphics 初期化が失敗した。実機マウスでの操作は未確認。 [キーボードの回帰テスト](../../../Test/Hestia_Tests/Source/GameRuntimeTests.cpp)
