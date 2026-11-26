# InputSystem の設計

InputSystem はキーボードとマウスを Game Script に公開する。キーボードは WASD による Player の移動、マウスはカメラ操作とクリック判定を初版の利用先とする。

[全体設計](../Architecture.md) ・ [Engine と Game の API 構成](GameRuntime/APIArchitecture.md)

## 所有と入力経路

`InputSystem` は App のキーボード・マウス状態とカーソル設定を静的に保持する。Engine が初期化、フレーム末尾のクリア、終了のタイミングを管理する。状態の更新と参照は App の更新スレッドで行う。

Application は `EngineAPI::ProcessMessage` でウィンドウメッセージを渡す。`Engine::ProcessMessage()` は静的な `InputSystem::KeyboardHook(UINT message, WPARAM wParam, LPARAM lParam)` と `InputSystem::MouseHook()` に委譲する。各 Hook が対象メッセージを判定し、状態を更新する。 [Application のメッセージ配送](../../Application/Source/Application.cpp)

Engine は `InputSystem` の状態照会とカーソル設定の操作を `InputAPI` にまとめる。GameRuntime は `GameEngineAPI` で受け取った `InputAPI` を `HestiaGame::Input` に接続する。 [GameRuntime の接続と寿命](GameRuntime/GameDLL.md)

## キーの定義

`Key` は `std::uint8_t` を基底型とする列挙型。公開ヘッダーには Hestia の列挙名と対応する数値を定義する。仮想キーコードが `0–255` の範囲にあることを確認し、左右の修飾キーを識別したあと、キーコードをそのままビット列の添字に使う。たとえば `Key::W = 0x57`、`Key::Space = 0x20`、`Key::F1 = 0x70` とする。 [Win32 の仮想キーコード一覧](https://learn.microsoft.com/en-us/windows/win32/inputdev/virtual-key-codes)

| 分類 | 初版で定義する名前 |
| --- | --- |
| 英数字 | `A`–`Z`、`Digit0`–`Digit9` |
| 基本操作 | `Backspace`、`Tab`、`Enter`、`Escape`、`Space`、`Insert`、`Delete` |
| 移動 | `Left`、`Up`、`Right`、`Down`、`Home`、`End`、`PageUp`、`PageDown` |
| 修飾・ロック | `Shift`、`Ctrl`、`Alt`、`LeftShift`、`RightShift`、`LeftCtrl`、`RightCtrl`、`LeftAlt`、`RightAlt`、`CapsLock`、`NumLock`、`ScrollLock` |
| 機能・システム | `F1`–`F24`、`Pause`、`PrintScreen`、`LeftWindows`、`RightWindows`、`ContextMenu` |
| テンキー | `Numpad0`–`Numpad9`、`NumpadMultiply`、`NumpadAdd`、`NumpadSeparator`、`NumpadSubtract`、`NumpadDecimal`、`NumpadDivide` |
| 記号 | `Oem1`–`Oem8`、`OemPlus`、`OemComma`、`OemMinus`、`OemPeriod`、`Oem102` |

仮想キーは Windows が解釈した論理キーとして扱う。記号の意味はキーボード配列で変わるため、記号キーには OEM 名を使う。

左右の Shift・Ctrl・Alt は `0xA0`～`0xA5` を使う。`KeyboardHook` は `VK_SHIFT`、`VK_CONTROL`、`VK_MENU` を受けたら、`lParam` のスキャンコードと拡張キーフラグを `MapVirtualKeyW(..., MAPVK_VSC_TO_VK_EX)` に渡して左右を識別する。左右キーのコードが得られた場合に状態を更新する。 [Win32 のキー入力](https://learn.microsoft.com/en-us/windows/win32/inputdev/about-keyboard-input) ・ [MapVirtualKeyW](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-mapvirtualkeyw)

## キー状態と遷移

`KeyboardState` は初期値がすべて `0` の `std::bitset<256>` を3本持つ。`held` が押下状態を保持し、`down` と `released` がそのフレームに起きた遷移を保持する。キーの状態は対応する添字のビットで参照する。

| ビット列 | 意味 |
| --- | --- |
| `held` | 最後に処理したキー通知の時点で押されている |
| `down` | フレーム中に押していない状態から押した状態へ変わった |
| `released` | フレーム中に押した状態から離した状態へ変わった |

| 通知 | 状態の更新 |
| --- | --- |
| `WM_KEYDOWN` / `WM_SYSKEYDOWN` | `held` が `0` のキーに対し、`held` と `down` を立てる。押下中の繰り返し通知では既存の値を使う。 |
| `WM_KEYUP` / `WM_SYSKEYUP` | `held` が `1` のキーに対し、`held` を落として `released` を立てる。 |
| `WM_KILLFOCUS` | 3本のビット列をクリアし、フォーカス復帰後のキー通知から状態を作る。 |

同じフレーム内に押して離すと `down` と `released` が両方立ち、`held` は `0` になる。同じフレーム内に再び押した場合は `held` が `1` になる。押下または解放が複数回起きても、その種類のフラグは1本として扱う。

共通の `Shift`、`Ctrl`、`Alt` の `held` は左右の `held` の OR で更新する。最初に片側を押したときに共通キーの `down` を立て、両側が離れたときに共通キーの `released` を立てる。左右それぞれの `down` と `released` は個別に記録する。

## マウス

### 位置・移動・スクロール

Engine は初期化時に App の HWND を渡し、`InputSystem` がその Window を対象にマウスの Raw Input を登録する。`MouseHook` はフォーカス中の `WM_INPUT` を読み、`RAWMOUSE` から移動量、スクロール、ボタンの遷移を得る。通常のマウスメッセージは Win32 の Window 処理に残す。フォアグラウンドの `WM_INPUT` は読み取り後も `DefWindowProc` に渡す。 [Raw Input の登録](https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-rawinputdevice) ・ [WM_INPUT](https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-input)

3つの値は `Vector2` で返す。

| 呼び出し | 値 |
| --- | --- |
| `Input::MousePosition()` | 現在のカーソル位置をクライアント領域の座標で返す。単位はピクセル、原点は左上。X は右、Y は下が正。領域外はそのまま範囲外の座標で表す。中央固定中は中央を返す。 |
| `Input::DeltaPosition()` | フレーム内の相対移動量の合計。単位は Raw Input の相対カウント。X は右、Y は下が正。全モードで同じ入力源を使う。 |
| `Input::DeltaScroll()` | フレーム内のホイール量の合計。X は水平で右、Y は垂直で奥へ回す方向が正。`WHEEL_DELTA` を `1.0` として小数も保持する。 |

位置は `GetCursorPos` と `ScreenToClient` で取得する。移動量は相対パケットから積算するため、中央固定中も視点操作に使える。絶対座標のパケットはボタンとスクロールに使う。 [RAWMOUSE](https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-rawmouse)

### ボタン

`MouseButton` は `Left`、`Right`、`Middle`、`X1`、`X2` の5値。各ボタンにキーボードと同じ `held`、`down`、`released` を持ち、`RAWMOUSE` の押下・解放フラグを受けた時点で更新する。同じフレームで押して離した場合は `held` が落ち、`down` と `released` が両方残る。 [RAWMOUSE のボタンフラグ](https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-rawmouse)

| 呼び出し | `true` を返す条件 |
| --- | --- |
| `Input::MouseHold(MouseButton::Left)` | 左ボタンが押下中。 |
| `Input::MouseDown(MouseButton::Left)` | このフレームに左ボタンの押下開始があった。 |
| `Input::MouseRelease(MouseButton::Left)` | このフレームに左ボタンの解放があった。 |

### カーソル

`Input::SetCursorLockMode(mode)` で `CursorLockMode` を指定し、`Input::SetCursorVisible(visible)` で表示の希望を指定する。`Input::GetCursorLockMode()` と `Input::GetCursorVisible()` はそれぞれの設定値を返す。初期値は `None` と表示あり。 [Unity のカーソルモード](https://docs.unity.com/en-us/engine/6000.0/script-reference/unityengine/cursor/lockstate)

| モード | フォーカス中の位置制御 | 表示 |
| --- | --- | --- |
| `None` | 自由に移動する。 | 表示設定に従う。 |
| `Confined` | クライアント領域内に制限する。 | 表示設定に従う。 |
| `Locked` | クライアント領域の中央1ピクセルに制限する。 | 非表示。 |

`Locked` の間も表示設定は保持し、解除後に反映する。制限はクライアント領域をスクリーン座標に変換して `ClipCursor` で適用し、ウィンドウの移動やサイズ変更後に更新する。 [ClipCursor](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-clipcursor)

非表示のとき、`MouseHook` は `WM_SETCURSOR` の `HTCLIENT` に対して `SetCursor(nullptr)` を呼び、処理結果 `TRUE` を返す。設定変更時はクライアント領域上の表示もその場で更新する。`EngineAPI::ProcessMessage` は処理済みかどうかと `LRESULT` を返せる形にし、Application の `WindowProc` は処理済みならその値を返す。 [WM_SETCURSOR](https://learn.microsoft.com/en-us/windows/win32/menurc/wm-setcursor)

フォーカスを失ったらカーソル制限を解除し、ボタン状態、移動量、スクロール量をクリアする。フォーカスが戻れば、その時点のカーソル設定を適用する。Engine の終了時はカーソル制限を解除し、Raw Input の登録を外す。

## フレームとの接続

1. Application が更新前のメッセージを処理する。前フレームの待機中に届いた入力も、このフレームの状態に含まれる。
2. Engine の `FixedUpdate` と通常フレームで Game が入力を読む。照会は状態を消費しない。固定更新が複数回走っても入力は共有されるため、押下・解放やマウス差分をフレームごとに一度適用する処理は通常の `Update` で行う。
3. `Engine::FrameExecute()` のフレーム末尾で、Script の終了処理と Scene の切り替え処理の後にキーとボタンの `down`・`released`、マウスの移動量とスクロール量をクリアする。`held` は次の対応する通知まで保持する。
4. Application がフレーム待機に入り、その間に処理した入力は次のフレームへ渡る。

[Application の更新順](../../Application/Source/Application.cpp) ・ [Engine のフレーム実行](../../Engine/Source/Engine.cpp)

## Game 向け API

| 呼び出し | `true` を返す条件 |
| --- | --- |
| `Input::KeyHold(Key::W)` | W が押下中。押した直後から真になる。 |
| `Input::KeyDown(Key::W)` | W の押下開始がこのフレームに届いた。 |
| `Input::KeyRelease(Key::W)` | W の解放がこのフレームに届いた。 |

GameRuntime の接続前と切断後の入力照会は `false` またはゼロを返す。入力を使う移動処理とカメラ操作は Game Script が担当する。

## 初版の確認項目

- 押下、保持、解放の各状態を区別でき、保持中のオートリピートで `KeyDown` が再発生しない。
- 左右の修飾キーを個別に照会できる。両側を押したあとに片側を離しても、共通キーは押下中を保つ。
- 更新前に押して離した場合、同じフレームに `KeyDown` と `KeyRelease` の両方を取得できる。
- フレーム末尾で遷移フラグだけが消え、待機中に受けた次の入力は次フレームに届く。フォーカスを失ったらすべての状態がクリアされる。
- Game Script が `KeyHold` で WASD を読み、Player を移動できる。
- 5ボタンの押下・保持・解放と同フレームの短いクリックを照会できる。移動量と縦横のスクロール量はフレーム内で加算される。
- `Locked` 中は中央固定と非表示のまま移動量を取得できる。`Confined` の領域は移動・サイズ変更に追従し、フォーカス喪失と終了で制限が外れる。
