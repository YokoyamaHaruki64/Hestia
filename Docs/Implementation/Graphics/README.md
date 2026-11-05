# Graphicsの処理・構造

[全体索引](../README.md) / [型](Types.md)

根拠: [GraphicsSystem.cpp](../../../Engine/Source/Graphics/GraphicsSystem.cpp)、[DX12Backend.cpp](../../../Engine/Source/Graphics/DX12Backend.cpp)

## 所有と初期化

GraphicsSystemはunique_ptrでDX12Backendを所有し、RenderDataのqueueとView/Projectionを保持する。単一instanceを接続し、初期化・資源登録・描画・終了は同じスレッドで行う。null HWND、正でない寸法、二重初期化を拒否する。初期Viewは単位行列、ProjectionはWindow寸法の左手系正射影（near 0.1、far 100）。Resizeは未対応。

BackendはDevice、Direct Queue、SwapChain、二つのFrameContext、描画先、depth、descriptor heap、RootSignature、PSO cache、定数Buffer、GPU Asset Storage、UploadContextPool、Fenceを管理する。RTV/DSVは各64、master SRVは2048、material textureは16、最大描画提出数は32768。これらは現在の固定容量で、動的に無制限へ増える契約ではない。

## CPU AssetからGPUへ

MeshはStream別Vertex Bufferとindex Bufferに変換する。TextureはRGBA8画素をUpload Resourceへコピーし、同じDirect Queueへ転送命令を積む。CPU AssetとGPU StorageはAssetHandleのindex/generationを共有する。無効・不一致・重複Handle、不正なMeshやpitch、資源生成失敗はfalseとなる。

転送用ResourceはWaitingResourceとしてFence値と共に保持し、Updateで完了したものから解放する。UploadContextもFence完了後に再利用する。APIが戻る前に入力画素はコピーされるため、Loaderの一時画像をGPU完了まで保持する必要はない。

## フレームと描画

SubmitはRenderDataを値で受け取りqueueに追加する。未初期化、容量超過、GPU Mesh不在ではfalse。Materialの値はコピーされるがShader pathのwstring_viewは借用のままであり、描画まで文字列を生存させる必要がある。

RenderはUpdate → BeginDraw → Draw → EndDrawを呼ぶ。BeginDraw失敗時は以降を呼ばない。BeginDrawはフレーム資源を準備し、PerFrame定数を書き、提出queueをBackend側へswapする。Drawは提出順にGPU Meshを検索し、Object/Material、Pipeline、Textureを設定してindexed drawする。未登録Meshは除く。欠落する任意頂点Streamには既定Bufferを使う。

EndDrawはscene colorをBackBufferへコピーし、状態遷移、コマンド実行、PresentとFenceの進行を行う。宣言された二つのscene colorだけから一般的なRenderGraphやポストエフェクト実装があると解釈しない。

## Shaderとの対応

[Common.hlsl](../../../Shaders/Common.hlsl) はb0=Frame、b1=Object、b2=Materialの定数と四つのsamplerを宣言する。[VS_Sprite](../../../Shaders/VS_Sprite.hlsl) は行ベクトルをworld → view → projの順で変換し、UVと頂点色を渡す。[PS_Sprite](../../../Shaders/PS_Sprite.hlsl) はspace1のt0をWrapLinearSamplerで読み、頂点色とMaterial色を乗算する。粗さ・metallic等のデータがあっても、このPSが物理ベースのライティングを実行するわけではない。

FinalizeはGPU待機後にBufferのmap、GPU Storage、転送待ち、Context等を解放する。GPU待機に失敗した経路では途中でreturnするため、全解放成功を戻り値で通知する仕様にはなっていない。描画結果やGPU異常時の動作は今回実行確認していない。
