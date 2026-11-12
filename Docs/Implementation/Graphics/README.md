# Graphicsの処理・構造

[全体索引](../README.md) / [型](Types.md) / [MaterialShader と Forward 描画](MaterialShader.md)

根拠: [GraphicsSystem.cpp](../../../Engine/Source/Graphics/GraphicsSystem.cpp)、[DX12Backend.cpp](../../../Engine/Source/Graphics/DX12Backend.cpp)

## 所有と初期化

GraphicsSystemはunique_ptrでDX12Backendを所有し、RenderDataのqueueとView/Projectionを保持する。単一instanceを接続し、初期化・資源登録・描画・終了は同じスレッドで行う。null HWND、正でない寸法、二重初期化を拒否する。初期Viewは単位行列、ProjectionはWindow寸法の左手系正射影（near 0.1、far 100）。Resizeは未対応。

BackendはDevice、Direct Queue、SwapChain、二つのFrameContext、描画先、depth、descriptor heap、RootSignature、PSO cache、定数Buffer、GPU Asset Storage、UploadContextPool、Fenceを管理する。RTV/DSVは各64、master SRVは2048、MaterialのTextureは1テーブル16枠。

## CPU AssetからGPUへ

MeshはStream別Vertex Bufferとindex Bufferに変換する。TextureはRGBA8画素をUpload Resourceへコピーし、同じDirect Queueへ転送命令を積む。CPU AssetとGPU StorageはAssetHandleのindex/generationを共有する。無効・不一致・重複Handle、不正なMeshやpitch、資源生成失敗はfalseとなる。

転送用ResourceはWaitingResourceとしてFence値と共に保持し、Updateで完了したものから解放する。UploadContextもFence完了後に再利用する。APIが戻る前に入力画素はコピーされるため、Loaderの一時画像をGPU完了まで保持する必要はない。

## フレームと描画

SubmitはRenderDataを値で受け取りqueueに追加する。未初期化、容量超過、Meshがnullの場合はfalse。提出上限は1,000,000件。GPU Meshの有無はBuildで調べ、未登録ならその提出を飛ばす。 [提出上限の定義](../../../Engine/Include/Graphics/DX12Backend.h)

RenderはUpdate → BeginDraw → Draw → EndDrawを呼ぶ。BeginDraw失敗時は以降を呼ばない。BeginDrawはフレーム資源を準備し、PerFrame定数を書き、提出queueをBackend側へswapする。DrawはForwardRenderPassのBuild → Sort → Recordを実行する。Shader Passが要求する任意頂点Streamが欠ける場合は既定Bufferを使う。 [RenderPassの設計](../../Design/RenderPass.md)

描画記録で通常Drawが使うObject定数は、フレームごとのUploadページに追加する。Material定数とSRVテーブルはフレーム内の同じMaterialで共有する。提出キュー、Upload領域、Materialテーブルにはそれぞれ別の制約がある。 [定数とSRVの割り当て](MaterialShader.md)

EndDrawはscene colorをBackBufferへコピーし、状態遷移、コマンド実行、PresentとFenceの進行を行う。宣言された二つのscene colorだけから一般的なRenderGraphやポストエフェクト実装があると解釈しない。

## Shaderとの対応

`Common.hlsl` はFrame定数のb0、Object定数のb1、四つのsamplerを宣言する。Compilerはb2にMaterialのProperty宣言を生成する。Pass用PSOでは要求Streamに応じたInput Layoutを作る。 [Shaderの実装仕様](MaterialShader.md)

FinalizeはGPU待機後にBufferのmap、GPU Storage、転送待ち、Context等を解放する。GPU待機に失敗した経路では途中でreturnするため、全解放成功を戻り値で通知する仕様にはなっていない。Forward描画の確認範囲は[検証記録](MaterialShader.md#検証記録)を参照。
