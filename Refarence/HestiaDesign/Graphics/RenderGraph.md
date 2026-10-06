# Render Graph

NativeRenderが1 View分の描画PassとResource依存を構築・検証・実行する内部機構。[[RenderingPipeLine]]の描画順を表現し、[[ScreenEffectShaderDSL]]のI/O定義からScreen Effect Passを追加する。

## 基本方針

- Render GraphはNativeRender内部の機能とし、GameLogicやMaterialShader利用者へ直接公開しない。
- Passは読むResource、書くResource、実行処理を宣言する。Graphが依存順、Resource State遷移、一時Resourceの寿命を解決する。
- ScreenEffectShaderの`Hook`とI/OからNodeを生成する。GBuffer、Shadow、Deferred Lighting、Forward、UI、Presentなどの組込みPassはC++側から同じGraphへ登録する。
- Hookは大まかな挿入位置、I/Oは実際のResource依存、Pipeline設定は同じHook内の順序を表す。
- 最初から汎用Render Graph Editor、Async Compute、複数Queue、自動的な最適Pass融合までは扱わない。

## 全体像

```text
Rendering Pipeline設定
    ├─ 組込みPass定義
    ├─ 有効なScreenEffectShader
    └─ Hook内の順序・Property値
                 ↓
          Render Graph構築
                 ├─ Pass Node
                 ├─ Logical Resource / Version
                 └─ Read / Write依存
                 ↓
            Compile / Validate
                 ├─ 実行順
                 ├─ Resource State遷移
                 ├─ 一時Resource寿命
                 └─ 未使用Pass除外
                 ↓
       NativeRender ThreadでExecute
```

Graphの形はPipeline設定、EffectのON / OFF、Viewport解像度やFormatが変わった時に再構築する。通常Frameではコンパイル済みのGraphへFramePacket、RenderItem、Camera、Light、Property値をBindingして実行する。

## Passの宣言

組込みPassはC++側でI/Oを宣言する。

```cpp
graph.AddPass("GBuffer",
    [&](RenderGraphBuilder& builder)
    {
        builder.WriteDepth(sceneDepth, LoadOp::Clear);
        builder.WriteColor(gbufferNormal, LoadOp::Clear);
        builder.WriteColor(gbufferMaterial, LoadOp::Clear);
    },
    [&](RenderGraphContext& context)
    {
        RecordGBuffer(context);
    });
```

ScreenEffectShaderはDSLの定義から同等のNodeを生成する。

```text
ScreenEffectShader "Reflection/SSR"
Hook: AfterForwardOpaque
Read:  SceneColorSrc, SceneDepth, GBufferNormal, GBufferRoughness
Write: SceneColorDst
```

実行callbackはNode構築時にResourceの実体を捕まえず、`RenderGraphContext`から当該FrameのDescriptorやTextureを取得する。GraphへGameLogic.dll、ECS Component、Frame外で無効になるポインタを保持しない。

## ResourceとVersion

GraphではTextureやBufferを論理Resourceとして扱う。Passが書き込むたびに新しいVersionを作り、後続Passは必要なVersionを読む。

```text
SceneColor v0 ── Forward Opaque ──> SceneColor v1
SceneColor v1 ── SSR ─────────────> SceneColor v2
SceneColor v2 ── Fog ─────────────> SceneColor v3
```

SceneColorのScreen Effectは同時に同じTextureを読み書きできないため、物理的にはA / Bの2枚へ交互に割り当てる。

```text
v0 = A
v1 = A  // RasterのLoad + Blend。以前のVersionをSRVとして同時に読まない
v2 = B  // SSR: Aを読むため別Textureへ書く
v3 = A  // Fog: Bを読んでAへ書く
```

Raster Passが現在のSceneColorへBlendする場合はColor Attachmentを`Load`して新しい論理Versionを作る。Screen Effectは`SceneColorSrc`をSRVとして読み、別の`SceneColorDst`へ書く。Graphが現在のVersionとA / Bの割当を管理し、ShaderやPass実装は物理Texture名を知らない。

### Resourceの種類

- **Transient:** 当該Graph実行中だけ必要なGBufferや中間Texture。寿命は最初のWriteから最後のReadまで。
- **Persistent:** History、Shadow CacheなどFrameをまたいで保持するResource。所有者をGraph外に定めてImportする。
- **External:** SwapChain BackBuffer、Editorが参照するView Textureなど。生成・破棄は所有者が行い、Graphは当該Frameの利用だけを宣言する。

初期実装ではResource Aliasingを行わなくてもよい。論理的な寿命を正しく取得し、必要になった時に同時利用されないTransient ResourceのHeap領域を再利用できる設計にしておく。

## Hookと依存順

HookごとにAnchorを用意し、ScreenEffectShaderは指定したAnchorの範囲へ追加する。

```text
GBuffer
  ↓
[AfterGBuffer]
  ↓
Deferred Lighting
  ↓
[AfterDeferredLighting]
  ↓
Forward Opaque
  ↓
[AfterForwardOpaque]
```

同じHook内はPipeline設定の順序を基本とする。I/O依存に反する順序、未生成ResourceのRead、循環依存はCompile Errorとする。Render GraphがShaderの意図を推測して別Hookへ移動させることはしない。

## Compile時の処理

1. 組込みPassと有効なScreenEffectShaderからNodeを集める。
2. Hook Anchor、登録順、Read / Writeから依存Edgeを作る。
3. 未生成Resource、複数の不正なWriter、同一Resourceの不正な同時Read / Write、循環依存を検出する。
4. 最終出力へ寄与しない副作用のないPassを除外する。
5. Pass順と各Resourceの最初・最後の利用を確定する。
6. 論理Resourceを物理Resourceへ割り当てる。
7. D3D12 Resource State遷移と必要なBarrierを生成する。

Present、Timestamp Query、Capture、Readbackなど、Resource出力だけでは必要性を判断できない処理はSide Effect Passとして明示する。

## 実行とOwnership

Graphの構築・Compile・ExecuteはNativeRender Threadが行う。Main ThreadはPipeline設定やEffectの変更要求をFramePacketへ積み、NativeRenderがFrame境界で反映する。実行中GraphのNodeやResource定義を別Threadから変更しない。

GraphはResourceの利用期間を管理するが、GPU完了そのものは所有しない。NativeRenderがFenceを追跡し、GPU使用中のResource、Descriptor、Command Allocatorを再利用・破棄しない。外部・永続Resourceの最終所有者もGraphとは別に明示する。

## 初期実装の範囲

- Direct Queue上のRaster / Full Screen Pixel Pass。
- 明示的なTexture / BufferのRead・Write。
- Hookと登録順を含む依存検証。
- Resource State Barrierの生成。
- Transient Resourceの生成・寿命管理。
- SceneColor A / BへのVersion割当。
- 未使用Passの除外。

Compute Shader、Async Compute、Copy Queue、Resource Aliasing、並列Command List記録、Graphの可視化は後から追加する。必要なScreen EffectがComputeを要求した時点で、Pass種別とQueue間同期を設計する。

## 未解決事項

- Pass / Resource HandleとBuilder APIの最終形。
- GraphをViewごとに持つか、Scene ViewとGame Viewを1つのGraphへ含めるか。
- Pipeline設定変更時の再Compileと旧ResourceのFence待ち。
- History Resourceの世代、Resize、無効化条件。
- Debug表示、Capture、PIX MarkerとGraph Nodeの対応。

関連: [[RenderingArchitecture]] / [[RenderingPipeLine]] / [[ScreenEffectShaderDSL]]
