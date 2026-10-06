# マルチスレッド描画
Main Threadが提出したFramePacketをNativeRenderで受け取り、大量のRenderItemを扱う描画PassのCPU処理をWorkerへ分担する設計草案。[[RenderingArchitecture]]のThread境界を維持し、Pass内の準備とCommand List記録を並列化する。

本資料では、初期実装にRender Graphを導入しない方針を扱う。[[RenderingArchitecture]]と[[RenderingPipeLine]]にあるGraph利用の説明は後段の構成案として参照し、初期のPass順とBarrierはハードコードする。手動管理が困難になった時点で[[RenderGraph]]の導入を検討する。既存資料のGraph部分は、この初期実装の必須条件ではない。

## 基本方針
- 主な対象はゲーム実行時のDraw数。Editorの二画面描画の高速化を、この設計の主目的にはしない。
- Main ThreadはWorldを更新し、RenderItem、Camera、Lightなどの描画値を不変なFramePacketへまとめる。WorkerはECSやGameLogicの可変状態を直接参照しない。
- Pass内の処理を`Build`、`Sort`、`Record`の三段階に分ける。Buildは入力範囲ごと、RecordはSort後に確定した描画範囲ごとにJob化する。
- PassのBuild Jobがすべて終わったら、そのPassのSort Jobを発行する。Sort完了後にRecord Jobを発行する。全PassのBuild完了を一律には待たない。
- NativeRenderがPass順、Resource State、Barrier、Queue送信、Fence、Presentを管理する。Workerは独立したCommand Listへ記録し、Queueへ直接送信しない。
- 最初はDirect Queueを一つ使う。CPUでの並列記録のためにCopy QueueやAsync Computeを追加しない。
- Item数に応じたBuildと、Batch数・Command量に応じたRecordを分割する。ScreenEffectなどCommand数の少ない処理には、同じ粒度のJob分割を強制しない。

## 三段階の描画準備
### Build：CullingとSort Key生成
Buildは、Passへ渡されたRenderItemの入力範囲を走査し、そのPassの対象Itemを選別する。視錐台Culling、MaterialShader Passや描画区分による分類、Sort Key生成をここで行う。

各Jobは可視ItemのindexとSort Keyを、自分専用の出力領域へ書く。共有配列への同時追加や、共有Material・GPU用キャッシュの更新をBuildの処理へ混ぜない。RenderItem全体をJob間で複製せず、固定済み入力へのindexで参照できる。

Camera、Bounds、Materialの描画情報など必要なCPU入力が固定され、他PassのBuild結果を使わない範囲なら、異なるPassのBuildも並行して実行できる。ShadowではLight側の視錐台など、Passに合う判定条件を使う。同じFrameにあるGPU処理の結果をCPUへ読み戻して判定するCullingは、この独立したBuildの前提に含めない。

Buildを分割しても、各Jobの末尾ではSortしない。入力範囲ごとにCullingで出力数が変わるため、Buildの入力rangeを、そのままRecordのrangeへ引き継がない。

### Sort：集約とPass全体の順序確定
そのPassの全Build Jobが終わった後、出力を一つの描画列へ集約し、Pass全体をSortする。Sort自体は最初はPassごとに一つのJobとして実行し、異なるPassのSortは並行して進められる。

Sort Keyの比較規則はPass内で統一する。不透明物ではPSO、Material、Mesh、Depthなどの優先順をPassの目的に合わせて決める。半透明物ではBlend結果に必要な描画順を優先し、MaterialやMeshによるまとめ方でその順序を壊さない。具体的なKey構成と同値時の規則は未確定だが、Workerの完了順で描画順が変わらないよう、元Itemのindexなどを使った決定的な順序にする。

```text
入力Item : [0, 1, 2, 3, 4, 5, 6, 7]
Build A  : 入力[0, 4) → 可視index [0, 2]
Build B  : 入力[4, 8) → 可視index [4, 5, 7]
集約     : [0, 2, 4, 5, 7]
Sort     : [5, 0, 7, 2, 4]  // 順序を示す例。実際はSort Keyで比較する。
```

部分Sortした列同士を比較規則に従ってMergeする方法も可能だが、単純な連結では全体のSortにならない。初期構成は集約後の全体Sortとし、部分SortとMergeはSortが実測で重くなってから比較する。

Sort後に描画Batchを構築する案を候補とする。ここでのBatchは一つのDrawにまとめられる単位を指し、単にMaterialが同じItemの集合とは区別する。Instancingを初期実装に含めるかは未確定であり、最初は一つのItemが一つのBatchでもよい。

### Record：確定した描画範囲のCommand記録
Recordは、Sort後に確定した描画列またはBatch列の範囲を受け取り、貸し出されたCommand Listへ記録する。同じPassのRecordを複数Workerで並行して呼べるようにする。

Build結果、Sort結果、Binding情報、Passの設定はRecord中に変更しない。現在のMaterial・PSOなどのBindingキャッシュ、作業領域、Upload領域は記録Contextに置き、Passの共有メンバへ書き込まない。各Command Listには、そのListの描画に必要なBindingとStateを設定する。

GPU実行に依存があるPassでも、記録に必要なResourceやDescriptor、設定が固定されていれば、CPUでのRecordは並行して行える。たとえばLightingの記録開始に、GBufferのGPU実行完了は必要ない。GBufferからLightingへの依存は、提出順とBarrierで保証する。

## Job発行と合流
```text
NativeRender：FramePacket取得、資源要求の反映、描画入力の固定
    │
    ├─ Pass A：Build(range 0, 1, 2…)
    │               ↓ 当該Passの全Build完了
    │            Sort / Batch構築
    │               ↓
    │            Record(range 0, 1…)
    │
    ├─ Pass B：Build(range 0, 1…)
    │               ↓ 当該Passの全Build完了
    │            Sort / Batch構築
    │               ↓
    │            Record(range 0, 1, 2…)
    │
    └─ Command数の少ないPass：まとめて記録する経路
                    ↓ 必要なListの記録完了
          NativeRender：GPU実行順に配列を構築
                    ↓
          ExecuteCommandLists → Fence Signal / Present
```

NativeRenderはPassごとのJob完了を管理し、完了した段階から次のJobを発行する。Pass AがRecord中でも、Pass BがBuildを続けてよい。SortもWorkerへ渡せる。

Workerの中で子Jobを発行して全完了を待つ構成は避ける。全Workerが待機すると、未実行Jobを進められなくなるため。完了通知や残Job数の具体的な実装、Worker Poolを描画専用にするかEngine内で共有するかは未確定とする。

各Record結果にはPass順とPass内のrange順に対応する出力slotを用意する。終了したWorkerから共有List配列へ追加する方法でGPU実行順を決めない。初期構成では必要な記録が揃ってから、そのFrameの提出列を組み立てる。

## Pass内の分割とDrawのまとめ方
Buildでは、元のRenderItem配列を連続したindex rangeで分割する。Recordでは、Sort後の描画列を改めて連続範囲へ分割する。

MaterialやMeshの境目を分割候補にすると、同じBindingのまとまりを保ちやすい。Instancing可能なItemをBatchへまとめた場合は、Batchのindex rangeでRecordを分割し、確定したDrawを途中で切らない。

ただし、MaterialとMeshが同じだけでは一つのDrawへまとめられない。PSO、描画設定、Geometry範囲、Overrideなどが互換であること、Transformや個別の値をInstanceデータとして表現できること、必要な描画順を維持できることを確認する。

まとまりを常に分割しない規則にすると、大きなグループだけ一つのWorkerへ偏る可能性がある。Bindingの再設定やBatchの再分割によるコストと、Jobの負荷分散を比較して境界を選ぶ。Batch数だけで負荷が均等になるとは限らないため、推定Command量や計測結果も候補にする。分割上限と閾値は未確定。

## Command ListとAllocator
別々のCommand Listへの並列記録は可能だが、一つのCommand Listを複数Threadから同時に記録しない。同じAllocatorに、同時に記録中のListを複数関連付けない。[Microsoft：Command ListのThread規則](https://learn.microsoft.com/en-us/windows/win32/direct3d12/design-philosophy-of-command-queues-and-command-lists)

NativeRender側の共通処理が、Record JobへAllocatorとCommand Listを排他的に貸し出す。Reset、Record呼出し、Close、結果回収を共通の実行処理へ置き、Passは記録内容を担当する。Record JobはExecuteやPresentを行わない。

Allocatorは、それを使ったGPU処理のFence完了後にResetして再利用する。Command ListのResetはAllocatorのResetと条件が異なるが、記録メモリやUpload領域をCPU Job完了だけで再利用しない。初期のPoolはFrame資源のslotごとに管理する案を起点とし、保有数や再利用方法は実装時に決める。[Microsoft：記録と再利用](https://learn.microsoft.com/en-us/windows/win32/direct3d12/recording-command-lists-and-bundles)

Worker間で同じDescriptor領域やUpload領域を割り当てない。必要な範囲を事前に予約するか、排他的に貸し出す。共有のMaterial GPUキャッシュをRecord中に再構築する処理も避け、固定済みのBindingを使う。

## ハードコードしたPass順とBarrier
初期実装ではNativeRenderが、採用するPassの順序、使用Resource、前後のState、必要なBarrierを明示する。一般化したGraph構築や依存解析は追加しない。Draw数の多いPassのListが複数あっても、GPU提出はPass順、次にPass内の描画範囲順で組み立てる。

BarrierはCommand Listへ記録する。専用Listを描画Listの間へ置く方法と、前後の描画Listの先頭・末尾へ入れる方法がある。Workerが記録する順序からGPU Resource Stateを推測せず、NativeRenderが確定した遷移計画を使う。[Microsoft：Resource Barrier](https://learn.microsoft.com/en-us/windows/win32/direct3d12/using-resource-barriers-to-synchronize-resource-states-in-direct3d-12)

### Barrier専用Listを使う提出例
```text
CommandLists[]
  [0] GBuffer準備：RT / DepthへのTransition、Clear
  [1] GBuffer Record range 0
  [2] GBuffer Record range 1
  [3] GBuffer Record range 2
  [4] Lighting準備：必要なGBufferをSRVへTransition、出力の準備
  [5] Lighting Record
  [6] Forward準備：必要なTransition
  [7] Forward Record range 0
  [8] Forward Record range 1
   …
  [N] 最終出力とPresentに必要なTransition
```

これは配置を示す例であり、全Passや全境界に専用Listを作る規則ではない。Barrierがなければ専用Listを作らず、小さい処理は隣接する記録へまとめられる。ClearはPass開始時に一度行い、後続の分割Listで繰り返さない。

同じResource Stateで描くPass内の分割境界には、分割したという理由だけでTransitionを追加しない。ただし、途中でResourceの利用方法が変わる処理やUAV依存などがあるなら、その区切りと必要な同期を明示する。全Passを視錐台Cullingと独立したDrawだけで表せるとは限らない。

一回の`ExecuteCommandLists`で複数Listを渡しても、List境界そのものが全GPU処理の完了境界になるわけではない。必要な依存はBarrierで表す。PassごとにCPUからGPU Fenceを待つ構成にはしない。同じDirect Queue内の提出と、CPU側のJob合流は別の同期として管理する。[Microsoft：ExecuteCommandLists](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12commandqueue-executecommandlists)

採用するBarrier方式とState追跡、暗黙のPromotion / Decayをどう扱うか、Listの提出を一回にまとめるか分けるかは合わせて設計する。Barrier専用Listを使うことでState管理が不要になるわけではない。

### DX12のRender Pass APIとの区別
本資料のPassはエンジン上の論理的な描画段階を指す。DX12の`BeginRenderPass` / `EndRenderPass`を使う場合、一組のBegin / Endを複数Command Listにまたがらせることはできない。Pass内分割時のAttachmentの維持とClear、必要ならSuspend / Resumeの扱いを別途検討する。このAPIの採用自体は未確定。[Microsoft：D3D12 Render Pass](https://learn.microsoft.com/en-us/windows/win32/direct3d12/direct3d-12-render-passes)

## IRenderPassの契約案
Passへ共通の三段階を持たせる`IRenderPass`を候補とする。NativeRender側でJob発行や記録先の貸出を共通化し、各PassにCulling条件、Sort規則、描画内容を持たせるための内部インターフェース。GameLogicへ公開するAPIではない。

| 関数 | 呼出し単位 | 入力と成果物 |
| --- | --- | --- |
| `Build` | Pass内の入力rangeごとのJob | 固定済み描画入力とrangeから、可視Itemのindex・Sort KeyをJob専用出力へ書く |
| `Sort` | 当該Passの全Build完了後、一つのJob | 部分出力を集約してSortし、描画列・必要ならBatchを確定する |
| `Record` | Sort後の描画rangeごとのJob | 固定済みBuild結果とBinding、range、記録Contextを使いCommandを記録する |

```cpp
// 責務と引数の関係を示すAPIスケッチ。型・戻り値・ABIは未確定。
class IRenderPass
{
public:
    virtual ~IRenderPass() = default;

    virtual void Build(const PassBuildInput& input,
                       ItemRange range, BuildChunk& output) const = 0;

    virtual void Sort(BuildChunks& chunks,
                      PassDrawData& output) const = 0;

    virtual void Record(const PassRecordInput& input,
                        DrawRange range, RecordContext& context) const = 0;
};
```

同じPassへ複数のBuild / Recordを同時に呼べる契約とし、出力の所有と変更範囲を引数で区切る。`const`指定だけでThread安全になるわけではない。Passが参照する外部データや内部キャッシュにも、固定または排他の条件が必要。

同じ関数構成を使うことと、すべてを別Jobで実行することは分ける。ScreenEffectなどでは入力Item走査やSortが不要な場合があり、準備を省略して短い記録をNativeRenderまたはまとめたJobで行える。最初から全PassへItem配列やInstancingを必須にしない。

仮想インターフェースの採用、上記の型、Record rangeの生成場所、失敗結果の形は未確定。既存のPass callbackで同じ責務を表せるなら、その構成との実装コストも比較する。

## データ所有と同期
NativeRenderが、そのFrameの描画入力、Passごとの部分出力、Sort結果、Record結果を所有する。Workerへ渡す参照は、参照するJobがすべて終了するまで保持する。部分出力を集約・破棄するのは、そのPassの全Build Jobが終わった後。

Handleが不変でも、参照先のMaterialが不変とは限らない。[[Material]]の直接操作APIと整合するよう、Material変更をFramePacketと順序付けて渡すか、固定した描画用Snapshotを渡す方法を詰める。Workerがゲーム側のMaterialへ直接アクセスする方式にはしない。Shader ReloadやGPUキャッシュの更新も、Record中の参照を無効化しない地点で反映する。

| 期間 | 保持する対象と条件 |
| --- | --- |
| Packet提出からNativeRenderでの解決まで | Handleの参照先について、Unload・無効化と待機中Packetの関係を定める |
| CPU Job完了まで | FramePacket由来のCPU入力、描画用Snapshot、部分出力、Sort結果、Record Contextを保持する |
| GPU Fence完了まで | GPUが使うResource、Descriptor、Upload領域、Allocatorの記録メモリを保持する |

FramePacketの有界キューとBackpressureは[[RenderingArchitecture]]、[[RuntimeFrameLoop]]に従う。CPU上で待機するPacket数と、GPU処理中Frameの資源slot数は別に管理する。並列化のために未処理Frameを無制限に増やさない。

exe内のEditorの公式ImGui backendとGPU借用は[[EditorGraphicsIntegration]]・[[EditorArchitecture]]に従う。Scene記録Jobを合流し、SceneColorをSRVへ遷移させてからEditor Listを記録する。初月のEditorはMain / Render / GPUを1 Frameずつ直列化し、WorkerからImGui Contextを並行利用しない。Main先行と公式DrawData保持は後続の別判断とする。

## 失敗と終了処理
Build、Sort、Recordの失敗はPassとFrameに対応する結果としてNativeRenderへ集約する。Allocator / ListのResetやCloseに失敗した記録結果を成功として提出しない。失敗したPassの出力を必要とする後続処理も、そのまま送信しない。

Frame処理を中止しても、実行中Workerが参照するCPUデータや記録先を先に破棄しない。Jobの完了を確認し、既にGPUへ提出した資源はFenceに従って回収する。停止・Device喪失時の伝播は既存のNativeRender境界へ合わせる。回復可能な失敗の代替描画と、Frame全体を中止する条件は未確定。

## 並列化のコストと計測
Buildの負荷は主に入力Item数とCulling・分類処理、Recordの負荷は主に可視Batch数とBinding・Command量に応じて変わる。Entity数が多くても、描画対象が少ないFrameではRecord分割の利益が小さい。

Job投入、完了通知、出力集約、最も遅いJobの待機、ListごとのBinding設定、追加の記録メモリがコストになる。MaterialやMeshのまとまりを保つことと、Workerへ均等に仕事を渡すことにもTrade-offがある。

PassごとにBuildする構成では、同じItemを複数Passで走査するコストも生じる。同じViewと判定条件で共有できるCullingや分類があるかは計測後に検討し、異なる視錐台や描画条件の結果を一律には共有しない。

最初は少量の入力を直列処理できる経路を保ち、次を測定する。

- Passごとの入力Item数、可視Item数、Batch数、実Draw数。
- Build、集約・Sort、RecordのCPU時間と、Frame全体の準備・提出までの時間。
- Job数、Worker間の負荷差、合流待ち時間、Command List数。
- Allocator、部分出力、Upload、Descriptorの保有量。
- GPU時間とPresent / Fence待機。CPU記録の短縮とGPU描画の短縮を区別する。

同じ描画内容と順序で直列構成と比較し、Culling結果、Sort結果、Batch化によるDraw数、画像の一致を確認する。DX12 Debug LayerでStateやAllocator再利用の不正も確認する。性能効果は未検証であり、Worker数、rangeサイズ、直列へ切り替える閾値は実測で決める。

## 次に詰める点
- Material変更・Shader ReloadとFrameごとの描画データ固定。
- Sort Keyの比較規則、Batchの互換条件、初期Instancingの範囲。
- Record rangeをItem列とBatch列のどちらで扱うか、分割粒度とWorker数。
- ハードコードするPassごとのResource Stateと、Barrier専用Listを使う境界。
- Allocator / Command ListのPool、Upload / Descriptor領域の貸出とFenceによる回収。
- Job完了通知、エラー結果、停止時の合流方法。

Render Graphの導入は、Passの増減、Effectの組合せ、Resource Stateや一時資源の寿命を手動で管理する負担が大きくなった時に判断する。初期構成でGraphを実装せず、Build / Sort / Recordと明示的な提出順を先に成立させる。
