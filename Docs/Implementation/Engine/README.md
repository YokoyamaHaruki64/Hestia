# Engineの処理・構造

[全体索引](../README.md) / [型](Types.md)

根拠: [Engine.cpp](../../../Engine/Source/Engine.cpp)、[EngineAPI.cpp](../../../Engine/Source/API/EngineAPI.cpp)、[DllMain.cpp](../../../Engine/Source/DllMain.cpp)

## 生成と初期化

GetEngineAPIはDLL内の固定APIテーブルを返す。Createはnull ApplicationAPIを拒否し、nothrow newでEngineを生成する。Initialize失敗時はdeleteしnullを返す。

EngineはApplicationAPIをコピーし、Log → Time → Graphics（1600×900）→ Asset → API接続 → ECSの順に初期化する。GraphicsはApplicationAPIからHWNDを取得する。初期化済みフラグを持ち、途中の失敗ではFinalizeで準備済みのSystemを解放する。

ECS の初期化後にデバッグシーンを生成する。立方体の Mesh とインスタンス描画用の Material を登録し、固定のビュー・射影を設定する。続けて Game.dll をロードし、`GetGameRuntimeAPI`、必須関数、Runtime の生成結果を順に確認する。準備に失敗すると Engine の初期化も失敗する。 [デバッグシーン](../ECSProcess/MeshRenderer.md) ・ [GameRuntime の初期化](../GameRuntime/README.md)

## 更新

固定更新は Time の固定時間を更新してから、ECS と GameRuntime の `FixedUpdate` を順に呼ぶ。通常の `FrameExecute` は Graphics・Time の準備を確認してから次の順で処理する。

1. Time の通常時間を更新し、ECS の `FrameBegin` を実行する。
2. ECS の `Update`、GameRuntime の `Update`、ECS の `LateUpdate`、GameRuntime の `LateUpdate` を順に実行する。
3. ECS の `PreRender` で Transform を更新し、MeshRenderer が描画データを提出する。続いて Graphics の `Render` を呼ぶ。
4. GameRuntime の `EndFrame` で Script を除去・反映してから、ECS の `EndFrame` で Entity の実体を破棄する。

`ProcessMessage` の本体は空。 [Engine の更新処理](../../../Engine/Source/Engine.cpp)

## 終了

GameRuntime を破棄して Game.dll をアンロードした後、ECS → AssetAPI → Graphics → Asset → TimeAPI → Time → LogAPI → Log の順で解放する。Graphics の描画提出が借用する Asset は Graphics の終了後に破棄する。デストラクタも `Finalize` を呼び、フラグで解放済み部分を除く。`Destroy` は null を許容し、`Finalize` と `delete` を DLL 内部で実行する。

ApplicationからこのDestroy経路が実際に呼ばれるかは [Applicationの終了処理](../Application/README.md) を参照。2026-11-05にEngineのDebug_Editorビルドは成功したが、Engine生成を前提とするテスト4件はnullで失敗している。ビルド成立と生成・描画経路の実行確認を区別する。詳細は[ECSの検証記録](../ECS/README.md#検証記録2026-11-05)を参照する。
