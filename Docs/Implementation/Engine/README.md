# Engineの処理・構造

[全体索引](../README.md) / [型](Types.md)

根拠: [Engine.cpp](../../../Engine/Source/Engine.cpp)、[EngineAPI.cpp](../../../Engine/Source/EngineAPI.cpp)、[DllMain.cpp](../../../Engine/Source/DllMain.cpp)

## 生成と初期化

GetEngineAPIはDLL内の固定APIテーブルを返す。Createはnull ApplicationAPIを拒否し、nothrow newでEngineを生成する。Initialize失敗時はdeleteしnullを返す。

EngineはApplicationAPIをコピーし、Log → Time → Graphics（1600×900）→ Asset → API接続 → ECSの順に初期化する。GraphicsはApplicationAPIからHWNDを取得する。初期化済みフラグを持ち、途中の失敗ではFinalizeで準備済みのSystemを解放する。

初期化の最後に4頂点・6indexのDebugPlaneを登録し、Default MaterialとTextureTestを用意する。これらのHandleが無効なら初期化失敗となる。TextureのHandle有効性はGPU転送成功とは別である。

## 更新

FixedUpdateはTimeの固定時間を更新し、ECSのFixedUpdate phaseを進める。FrameExecuteはGraphics・Timeの準備とdebug Meshの有効性を確認後、Time更新、ECSのFrameBegin → Update → LateUpdate → PreRender、デバッグ描画提出、Render、ECS.EndFrameの順で進む。

描画は現在Engine内のデバッグ平面が主な入口であり、GameRuntimeやECS Componentから自動でRenderDataを抽出する経路はない。ProcessMessageは空。Game.dllのロード・GameRuntime呼び出しはコメント化された予定部分である。

## 終了

ECS → AssetAPI → Asset → Graphics → TimeAPI → Time → LogAPI → Logの順で接続と所有資源を解放する。API参照より先に参照先を壊さないよう順を管理する。デストラクタもFinalizeを呼び、フラグで解放済み部分を除く。Destroyはnullを許容し、FinalizeとdeleteをDLL内部で実行する。

ApplicationからこのDestroy経路が実際に呼ばれるかは [Applicationの終了処理](../Application/README.md) を参照。2026-11-05にEngineのDebug_Editorビルドは成功したが、Engine生成を前提とするテスト4件はnullで失敗している。ビルド成立と生成・描画経路の実行確認を区別する。詳細は[ECSの検証記録](../ECS/README.md#検証記録2026-11-05)を参照する。
