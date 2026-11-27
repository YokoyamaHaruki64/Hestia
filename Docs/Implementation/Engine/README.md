# Engineの処理・構造

[全体索引](../README.md) / [型](Types.md)

根拠: [Engine.cpp](../../../Engine/Source/Engine.cpp)、[EngineAPI.cpp](../../../Engine/Source/API/EngineAPI.cpp)、[DllMain.cpp](../../../Engine/Source/DllMain.cpp)

## 生成と初期化

GetEngineAPIはDLL内の固定APIテーブルを返す。Createはnull ApplicationAPIを拒否し、nothrow newでEngineを生成する。Initialize失敗時はdeleteしnullを返す。

EngineはLogの初期化後にApplicationAPIをコピーし、Input → Time → Graphics（1600×900）→ Asset → API接続 → ECSの順に初期化する。InputとGraphicsにはApplicationAPIから取得したHWNDを渡す。途中の失敗ではFinalizeで準備済みのSystemを解放する。 [InputSystem の入力経路と寿命](Input.md#入力経路と寿命)

ECS の初期化後に Game.dll をロードし、`GetGameRuntimeAPI`、必須関数、Runtime の生成結果を順に確認する。型登録を終えてから固定のビュー・射影を設定し、`Scenes/Startup.scene` の Entity と Script を復元する。Editor 構成では復元の成功後に File メニューのコマンドを登録する。DLL の接続や起動 Scene の復元に失敗した場合は Engine の初期化も失敗する。 [GameRuntime の初期化](../GameRuntime/README.md)

## 更新

固定更新は Time の固定時間を更新してから、ECS と GameRuntime の `FixedUpdate` を順に呼ぶ。通常の `FrameExecute` は Graphics・Time の準備を確認してから次の順で処理する。

1. Time の通常時間を更新し、ECS の `FrameBegin` を実行する。
2. ECS の `Update`、GameRuntime の `Update`、ECS の `LateUpdate`、GameRuntime の `LateUpdate` を順に実行する。
3. ECS の `PreRender` で Transform を更新し、MeshRenderer が描画データを提出する。続いて Graphics の `Render` を呼ぶ。
4. GameRuntime の `EndFrame` で Script を除去・反映してから、ECS の `EndFrame` で Entity の実体を破棄する。
5. `SceneSystem::SerializeScene()` で保存要求を処理してから、`CheckScene()` で切り替え要求を処理する。
6. `InputSystem::EndFrame()` でキーとボタンの遷移、マウスの移動・スクロール量をクリアする。

`ProcessMessage` はキーボードとマウスの Hook に委譲し、マウス側の処理結果を Application に返す。 [Engine の更新処理](../../../Engine/Source/Engine.cpp) ・ [InputSystem の実装](Input.md)

## Scene の保存と切り替え

要求の受付は Engine 内部の `SceneSystem` が担当する。`SaveScene(path)` は保存要求を保持し、空パスでは現在の Scene、最後に保存したパス、自動命名の順で保存先を決める。`LoadScene(path)` は `.scene` の相対パスを保持する。処理前の複数要求は種類ごとに最後の指定が残る。現在と同じパスを指定した場合も再読込する。 [SceneSystem.h](../../../Engine/Include/Scene/SceneSystem.h) ・ [Scene の設計](../../Design/SceneSystem.md)

Engine は GameRuntime の生成時に `SceneAPI` の表を渡す。表の関数ポインタは `SceneSystem` の静的関数を指す。 [SceneAPI.cpp](../../../Engine/Source/API/SceneAPI.cpp) ・ [Game 向けの共通 Facade](../GameRuntime/Types.md#game-向けの共通-facade)

### Editor の File メニュー

| コマンド | 処理 |
| --- | --- |
| `File/OpenScene...` | 開くダイアログで選んだ `.scene` の相対パスを `SceneSystem::LoadScene()` に渡す。切り替えには AssetDB への登録が必要。 |
| `File/SaveScene` | `GetCurrentScenePath()` に保存を予約する。現在のパスが空なら `SaveSceneAs()` を呼ぶ。 |
| `File/SaveSceneAs...` | 保存ダイアログで選んだ `.scene` の相対パスを `SceneSystem::SaveScene()` に渡す。新規保存と上書きに対応し、現在の Scene パスは次のロードが成功したときに更新する。 |

Engine 内の `ShowFileDialog` は App の HWND を親にし、指定されたルートと拡張子一覧で Windows の開く・保存ダイアログを構成する。選択したパスをルート相対の UTF-8 に変換し、キャンセルやルート外の選択では空を返す。Scene コマンドのルートには AssetDB の Asset ルートを渡す。 [Engine.cpp](../../../Engine/Source/Engine.cpp) ・ [App のメニュー配送](../Application/README.md#editor-のメニュー)

### フレーム末尾での処理

1. 保存要求があれば、`TransformHierarchy` を持つ Archetype の Chunk からルートと照会用 Handle を集める。GameRuntime に Script の型参照を一括照会し、各ルートの `SerializedEntity` を作る。
2. `AssetSystem::Save` が YAML の書き出しと AssetDB 登録を行う。保存成功時は確定したパスを `GetLastSavedScenePath()` で取得できる。保存失敗時はログを残し、保留中の切り替え処理は続ける。 [Asset の保存](../Asset/README.md#yaml-asset-の保存)
3. 切り替え要求があれば、AssetDB の索引を引いて `.scene` を読み直す。読み込みと構造検証の後、旧 Entity が有効な間に Script の `OnDestroy` を呼び、旧 Entity を破棄する。型・Component・Process の登録は維持する。
4. 新しい Entity を展開し、親子関係と Active を適用した後、型 GUID から Script を追加する。未登録 GUID はログに出して該当 Script を省く。読み込み・展開・追加反映の失敗は Engine に返し、実行中の切り替えでは `RequestQuit()` を呼ぶ。復元と Script の反映が成功してから現在のパスを更新する。 [SceneSystem.cpp](../../../Engine/Source/Scene/SceneSystem.cpp) ・ [Scene の保存形式](../../Design/Serialize/SceneSerialization.md)

保存後の再読込、Script の復元、Game 側からの切り替えは GameRuntime テストで扱う。 [GameRuntimeTests.cpp](../../../Test/Hestia_Tests/Source/GameRuntimeTests.cpp)

## 終了

GameRuntime を破棄して Game.dll をアンロードした後、Scene → Input → ECS → AssetAPI → Graphics → Asset → TimeAPI → Time → LogAPI → Log の順で終了する。Input は Window が有効な間にカーソル制限と Raw Input 登録を解除する。Graphics の描画提出が借用する Asset は Graphics の終了後に破棄する。デストラクタも `Finalize` を呼び、解放済み部分を除く。`Destroy` は null を許容し、`Finalize` と `delete` を DLL 内部で実行する。

Application の終了順序は [Application の Window Message と終了](../Application/README.md#window-messageと終了) を参照。
