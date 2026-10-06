# RenderItem提出とNativeRender
2026-10-04の初期構成更新：現在は[[SimpleArchitecture/Overview#初期は通常経路を動かす]]を優先する。ApplicationがEngine / Editorを所有し、EngineがGame.dllを所有する。CPUのUI・更新・描画記録はMain Threadで直接呼ぶ。削除は参照利用の終わりまで保留し、GPU再利用・解放のFence待機は残す。以下のFramePacer・NativeRender Thread・FramePacket転送・完了通知・高度な失敗処理は後続の比較案で、初期実装の必須条件ではない。

[[HestiaArchitecture]]の描画境界を詳しく見る草案。GameLogicやECSは描画したいものを`RenderItem`として提出し、DirectX 12を扱う`NativeRender`は専用のRender Threadで動く。

NativeRender内のPass順と各中間Textureの役割は[[RenderingPipeLine]]、Pass依存とResource State管理は[[RenderGraph]]、画面効果のShader契約は[[ScreenEffectShaderDSL]]にまとめる。

## 基本方針
- Main ThreadはWorldを更新し、描画に必要な値をFrame単位で集める。`RenderItem`は「何を描くか」を表し、DX12 Command ListやDeviceを公開しない。
- NativeRender ThreadはD3D12のDevice、Command Queue、SwapChain、Descriptor、Command記録・送信、Presentを担当する。両Threadの間は有界なFramePacketでつなぐ。
- 提出済みFramePacketは変更しない。ECS Component、GameLogic.dll内のオブジェクト、寿命の短い配列への生ポインタを入れない。
- Game.dllはPublic Graphics Facadeを使い、その実装をGameAPI.libへリンクする。API libはEngine::Graphics::SubmitRenderItemを直接呼び、Public RenderItemをそのまま渡す。Engine内部はEngine::Graphics実体を使う。Graphics実装 / Storage / DX12内部型をGameへ公開・別リンクせず、その下に中継Serviceを置かない。詳細は[[FacadeBoundarySketch]]。
- 公開操作にはRenderItem提出、動的Meshの生成・更新などを含める。ただしMain ThreadからGPU資源を直接書き換えず、データをコピーした資源要求としてNativeRenderへ渡す。

## RenderItemの提出
```cpp
// 契約の形を示すスケッチ。型・配置・ABIは未確定。
struct RenderItem
{
    MeshHandle mesh;
    MaterialHandle material;
    Matrix4x4 worldTransform;
    Bounds worldBounds;
    RenderLayer layer;
    …
};

class Graphics // Public Facade、実装はAPI lib
{
public:
    static bool SubmitRenderItem(const RenderItem& item);
};
```

Game.dllはGraphics::SubmitRenderItem、Engine内の描画SystemはEngine::Graphics::SubmitRenderItemを呼ぶ。API libは別の内部Draw型へ再構築せず、Subsystemの現在FrameCollectorへ同じRenderItemを渡す。
`Submit`はItemをコピーし、呼出し後にGameLogicが元データを変更・破棄してもよい。Frame外からの提出は受け付けない。Mesh / MaterialはEngineが管理する世代付きHandleとして参照する。
Camera、Light、Viewport、描画補間値などItem以外のフレーム情報は同じPacketにまとめる。NativeRenderはHandleを解決し、必要なCulling・Sort・Pass構築を行う。

各exe内ではまずEngineインスタンスを1つとし、`Graphics::`はそのEngineが保持するGraphicsへ委譲する。呼出しはEngine初期化後だけ有効。複数Engineを支えるための仕組みは必要になってから検討する。動的Mesh更新もCPU側の頂点データを呼出し中にコピーし、NativeRenderが資源作成・Uploadを行う。返すHandleの有効化時点と失敗通知は別途設計する。

```text
GameLogic / ECS System ── Submit(RenderItem) ──> FrameCollector
                                                    ↓ copy / freeze
                                         RenderFramePacket [immutable]
                                                    ↓ bounded queue
                                         NativeRender Thread [DX12]
                                                    ↓
                                             GPU Submit / Present
```

## NativeRenderの1フレーム
初月は固定Pass / Barrierを明示する[[RenderingSketch]]の経路を使う。以下のRender Graphを使う流れは、固定経路でゲームが動いた後の拡張候補であり初月の必須項目ではない。

1. FramePacketを取り出し、Resourceの生成・更新・破棄要求を順序付ける。
2. Mesh / Material Handleを解決し、ViewごとにCulling、Sort、Render Graphへ必要なPassを登録する。
3. Graphを検証・Compileし、Resourceの割当とBarrierを確定する。構成に変更がなければCompile済みGraphを再利用する。
4. Graph順にCommand ListへScene / Game Viewや製品画面を記録する。EditorならOffscreen TargetをSRVとして読める状態にしてからUIを記録する。
5. Command ListをQueueへ送信し、SwapChainをPresentする。
6. GPU Fenceの完了を追い、Frame資源、Descriptor、古いRender Targetを安全になってから再利用・破棄する。

DX12はCommand ListとCommand Queueを使って描画を提出し、資源状態とCPU/GPU同期をアプリケーション側で管理する。専用Threadに集めてもGPU完了待ちは消えない。[MicrosoftのCommand提出](https://learn.microsoft.com/en-us/windows/win32/direct3d12/command-queues-and-command-lists)、[Fenceによる資源管理](https://learn.microsoft.com/en-us/windows/win32/direct3d12/fence-based-resource-management)

初期構成のFrame送信はNativeRenderが行う。Editorの公式backend内部Uploadも同じRender Thread / Direct Queueへ直列化する。追加の描画記録ThreadやCopy Queueは性能上必要になってから検討する。Asset読込ThreadやMain ThreadはGPU資源を直接更新せず、Resource要求をNativeRenderへ渡す。

## FramePacketの寿命と遅延
CPU上の未消費Packetを無制限に積むと入力から表示までの遅延とメモリが増える。まず「実行中1件＋待機1件」程度の有限なキューを起点にし、満杯時は新しいFrameを無言で捨てず、Main ThreadへBackpressureを返すか空きを待つ。Windowメッセージの応答を長時間止めない待機方法は[[RuntimeFrameLoop]]で検討する。

Game.exeのFrameExecute成功はPacket受付までを意味する。初月Editor接続時はOverlay / GPU完了まで待つ別の実行条件を加える。Device喪失などはRender Threadから状態として返し、Main Threadが次の安全な地点で停止・復旧を判断する。GPUが参照するResourceやDescriptorは、CPU側のPacket消費だけでなくFence完了まで保持する。

RenderItemにはGameLogic.dll内の関数ポインタも入れない。これによりPacketをNativeRenderが消費した後は、旧DLLのコード実行を止めてReloadしやすくなる。ただしGameのAsset登録等からEngineが作ったGPU資源の解放はFenceを待つ。GameはDX12資源を直接所有しない。旧DLL由来のCPUデータ・callbackがPacket以外に残らないことも確認する。

## exe内のEditorの公式ImGui描画との接続
exe内のEditorは同じビルド設定のC++利用者としてEngineのEditorGraphicsContextを借りる。Device / Direct Queue / Global Heap / SwapChain / Presentの正本はEngine、ImGui Context・両backend・Editor用Allocator / ListはEditor。AssetDBへD3D12詳細を返さず、GraphicsのTextureHandle / TextureViewを表示へ使う。

NativeRenderがSceneColorをSRVへ遷移させ、EditorRendererが公式imgui_impl_dx12でEditor Listを記録する。EngineがScene / Editor Listを同じQueueへ順に送信し、最終BarrierとPresentを管理する。公式backend内部Uploadも同じRender Thread / Queueへ直列化する。

初月Editorは1 Frameずつ進め、FrameExecuteがOverlay / GPU完了まで待ってから次のMessage Pump / ImGui Frameへ戻る。DrawDataと同じContextをMain / Renderが並行利用しないため、独自UiFrameData変換・Stage Ticketは不要。Main先行が必要になった時に、公式DrawDataとTexture要求の保持方法を別途検討する。詳細は[[EditorGraphicsIntegration]]・[[EditorArchitecture]]。

## 次に詰める点
RenderItemの必須フィールド、Handleの世代管理、FramePacketの容量とメモリ再利用、CullingとSortの場所、満杯時の待機、GPU資源要求の順序、Editorの公式backend版・Context直列化・GPU借用、非同期エラーの伝播を個別設計する。Render Thread分離だけを理由にScene/ECS/Assetの公開APIを増やさない。
