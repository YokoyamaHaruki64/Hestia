# 描画パイプライン
1描画フレームの中で、何をどの順に描き、次の処理へ何を渡すかを整理する草案。[[RenderingArchitecture]]のNativeRender Threadがこの順序を実行する。

## 基本方針
- **Deferred + Forwardを基本にする。** 通常の不透明物はGBufferを経てLightingし、半透明物や特殊な最終色が必要な物はForwardで描く。
- [[MaterialShaderDSL]]の`RenderType = GBuffer`はSurface情報の生成、`RenderType = Forward`は最終色の直接出力を意味する。GBuffer出力は必ずしもDeferred Lightingを意味しない。`ShadingModel = None`ならLightingShaderによる評価を行わない。
- Deferred Lightingは[[LightingShaderDSL]]の`Direct`と`IndirectDiffuse`、GBuffer2のEmissionをHDR SceneColorへ合成する。`IndirectSpecular`はForward Opaque後のReflection Passが評価する。初期構成では成分別の専用BufferやMRTを常設しない。
- SSR、SSAO、Bloom、Tone Mapなどの画面効果には[[ScreenEffectShaderDSL]]を使う。EffectはPipelineが用意するHookへ登録し、入力・出力を宣言してPixelShaderを実行する。
- HDR SceneColorは2枚のTextureを`Src` / `Dst`として使うPing-Pong構成にする。SceneColorを変更する各Effectは`Src`を読み`Dst`へ書き、完了後に交換する。
- PassとResourceの依存、SceneColorのVersion、Resource State遷移は[[RenderGraph]]で管理する。HookはScreen Effectを挿入できる大分類としてGraph上のAnchorになる。
- 本書の「Pass」はフレーム全体の描画段階を指す。MaterialShader内の`Pass`は、その段階で使うShaderの記述単位であり、両者を一対一に固定しない。
- NativeRenderは[[RenderingArchitecture]]の不変なFramePacketから対象ViewのRenderItemを分類する。Pass間のResource State遷移、GPU資源の寿命、PresentはNativeRenderが管理する。[D3D12 Resource Barrier](https://learn.microsoft.com/en-us/windows/win32/direct3d12/using-resource-barriers-to-synchronize-resource-states-in-direct3d-12)

## 全体像
初期構成で必要な経路を実線、ゲーム内容や計測結果に応じて追加する処理を括弧で示す。Shadowは影を使う光源がある場合に実行する。

RenderFramePacket / Camera / RenderItem
          ↓
    (Skinning)
		  ↓
    (Shadow Map)
	      ↓
    (Depth Prepass)
          ↓
    **GBuffer 出力:SurfaceData / DepthStencil**
          ↓
    (Decal)
	      ↓
    \[AfterGBuffer: SSAO / GTAOなど]
          ↓
    **Deferred Lighting   出力:HDR SceneColor**
          ↓
    **Forward Opaque / 特殊表現**
          ↓
    **Reflection Pass: Skydome / Probe / SSR**
          ↓
    **Forward Transparent**
          ↓
    \[BeforeToneMap: Fog / Bloomなど]
	      ↓
    **Tone Map  出力: LDR RenderTexture**
          ↓
    \[AfterToneMap]
	      ↓
	**Upscale**
		  ↓
	\[AfterUpscale]
		  ↓
	**UI Render**
          ↓
    最終RenderTexture → BackBufferへCopy → Present


初期の骨格はGBuffer、Deferred Lighting、HDR SceneColorへの合成、必要なForward描画、Tone Map、最終出力とする。括弧内は初期実装の必須項目ではない。添付資料のPass名は、この順序に処理を追加する際の候補として参照する。

NativeRenderは各RenderItemのMaterialShader Passを調べ、GBuffer用とForward用の描画へ振り分ける。1つのMaterialShaderが両方のPassを持つ場合、同じRenderItemが複数の描画段階に現れ得る。

MaterialShader Passの`RenderPhase`でOpaque / Masked / Transparentを分類する。`RenderType`はGBuffer / Forwardの出力経路、`RenderPhase`はPipeline上の配置とSort規則、`ZTest` / `ZWrite` / `Blend`は実際のGPU Stateを担当する。

| 主な資源                     | 作る段階                             | 主な利用先                       |
| ------------------------ | -------------------------------- | --------------------------- |
| Depth                    | GBuffer、またはDepth Prepass         | Depth Test、AO、画面空間処理        |
| GBuffer                  | GBuffer Pass                     | Deferred Lighting、必要な画面空間処理 |
| HDR SceneColor Src / Dst | Deferred Lighting、Forward、Reflection、Screen Effect | Effectごとに読取元と書込先を交換         |
| LDR RenderTexture         | Tone Map、Upscale、LDR Screen Effect、UI | 最終表示用RenderTexture |
| BackBuffer                | 最終RenderTextureからCopy | Present |

EditorではGame ViewとScene Viewを別Viewとして描く。Game Viewはゲーム画面の経路を基準にし、Scene Viewに必要な表示・選択機能だけを追加する。EditorのDear ImGuiは、Viewの画像をSRVで参照する最終UIとして合成する。描画ThreadとTextureの寿命は[[EditorArchitecture]]に従う。

仮のTexture Formatと容量・帯域の試算は[[RenderTargetBudget]]に分ける。ここに記したPassの採用と解像度が変われば、その試算も更新する。

## 各Pass
### Skinning（必要な場合）
アニメーションした頂点をShadow、Depth、GBuffer、Forwardで共有するための準備段階。Compute Skinningは添付資料にもあるが、最初からComputeに限定しない。CPU / Vertex Shader / Computeのどれで行うかは、対象数と各Passでの再利用回数を確認して決める。
現状はComputeSkinningをメインに行う予定

### Shadow Map
**入力:** 影を落とすRenderItemとLight
**出力:** Lightから参照するShadow情報
初期構成はDirectional LightのCSMを毎フレーム更新する。SpotとPointのShadowも初期対応の対象とし、AreaはPoint Light相当の近似Shadowから始める。Deferred LightingとForwardは同じShadow Texture、Cascade Matrix、Split Distanceを参照する。Shadowを使うLightだけをShadow Passへ渡し、影が不要なFrameではPassを省く。負荷が問題になった場合に、Static Light / Static Casterの更新停止やLightごとの更新間隔を追加する。

### Depth Prepass / Custom Depth（追加候補）
GBufferより前にDepthを確定したい場合のPass。
PrepassなしではGBufferがDepthを書く。MaskedのPrepassはAlpha判定の再実行を伴うため、OverdrawやHiZの効果を計測してから採用する。輪郭や選択表示用のCustom Depthは通常Depthと目的を分ける

### GBuffer: Opaque / Masked
**入力:** `RenderType = GBuffer`のMaterialShader Pass
**出力:** Depth、GBuffer0 / 1 / 2、MaterialMetadata、ObjectMetadata、必要ならMotionVector

`#pragma surface`で指定した関数が`SurfaceData`を返し、Engine側が次の初期Formatへ展開する。通常のOpaqueとAlpha Testを行うMaskedをここで扱う。

| 出力 | 初期内容 |
| --- | --- |
| GBuffer0 | BaseColor.rgb + Material AO |
| GBuffer1 | OctNormal.xy + Roughness + Metallic |
| GBuffer2 | `R11G11B10_FLOAT`のEmission.rgb |
| MaterialMetadata | Material ID 16bit + ShadingModel ID 8bit + Flags 8bit |
| ObjectMetadata | Object ID 24bit + Flags 8bit |

詳細な容量・bit配置は[[RenderTargetBudget]]にまとめる。MSAAの扱いは未確定。`ShadingModel = None`の画素も、AOやOutlineなどのためにSurfaceを残せる。[MaterialShaderのGBuffer説明](MaterialShaderDSL.md)

Material Flagsのbit 1を`SSRReceive`、bit 2を`ReflectionMask`へ割り当てる。組込みSSRは初期構成ではこのbool Flagsだけを使い、MaterialごとのReflection Factorは参照しない。

同じMaterialShaderに後続のForward Passがある場合、GBufferへ必要なSurfaceだけ書き、Deferred Lighting対象からは外す構成を検討する。Stencil等による対象除外とDepth Testの条件は、不要なLightingコストや二重描画を避けるために別途決める。

### Decal（追加候補）
GBufferのSurface情報を変更する場合はGBufferの後、Lightingの前に置く。どのChannelを変更してよいか、Depth・Normalとの整合、複数Decalの順序を定めてから導入する。Forwardだけで描かれる物へのDecalは、この方法だけでは扱えない。

### SSAO / GTAO（追加候補）
`AfterGBuffer` HookでDepthとNormalから遮蔽量を求めるScreenEffectShaderとして実装する。Screen-space AO Textureを出力し、GBuffer0のMaterial AOと組み合わせてDeferred Lightingの`IndirectDiffuse`へ適用する。Forward OpaqueがAOを必要とする場合は両方をForward Shaderから参照する。Forward描画後の最終SceneColorへ一律に掛ける方式にはしない。DirectやReflectionへ機械的には掛けない。画質、解像度、History、Blurは効果を見て決める。

### Light Culling（必要になった場合）
Sceneに登録するLight数は固定しないが、初期構成で各ViewのGPU Light Bufferへ渡すLightは最大128個とする。Directionalは常に候補へ含め、Point / Spot / Areaは距離と設定された影響範囲から選ぶ。Light数が負荷になったとき、画面や空間を区切って評価対象のLightを絞る。Deferred Lightingにも利用できるが、初期構成で専用Passを必須にはしない。Forward+を選ぶ場合は重要な前段になる。

### Deferred Lighting
**入力:** GBuffer、Depth、Light、Shadow、必要ならAO
**出力:** HDR SceneColor

`ShadingModel`に対応する[[LightingShaderDSL|LightingShader]]を使い、`None`の画素は評価しない。PixelShader内では`Direct`と`IndirectDiffuse`を別の変数として評価し、Material AOとScreen-space AOを後者へ適用してGBuffer2のEmissionとともにHDR SceneColorへ出力する。`IndirectSpecular`はここでは評価しない。

### HDR SceneColorへの合成
Direct、IndirectDiffuse、GBuffer2のEmissionからOpaqueのHDR色を作る。後続のForward Passが重ねる先をここで確定する。成分別のBufferは初期構成では保持しない。

EmissionはまずGBuffer2方式を実装して計測する。非発光PixelにもGBuffer2の固定費用が掛かるため、負荷が問題になった場合は発光Materialだけを別PassでSceneColorへ加算する方式へ移行できるよう、Emissionの合成責務をこの段階に閉じ込める。

### Forward Opaque / 特殊表現
**入力:** `RenderType = Forward`の不透明・特殊MaterialShader Pass、Depth、Light / Shadow、必要ならAO。**出力:** HDR SceneColor。

Rim、Hair、独自Lightingなど、GBufferのShadingModelだけでは表しにくい最終色を描く。GBuffer済みの同一物を再度描く場合はDepthとStencilの条件を明示する。現行のShader DSLではForwardからLightingShaderを再利用する方法が未確定なので、`EvaluateShadingModel`を既存の機能として前提にしない。

初期のLight評価は、各Viewの最大128個の候補から対象Objectまたは描画範囲に近いLightを最大N個選び、Forward Shaderへ渡す。Directionalは常に候補へ含め、Point / Spot / Areaは距離と設定された影響範囲を使う。Areaは初期段階ではPoint Light相当として近似する。Nは設定で変更でき、初期値は8とする。詳細な影響度スコアは実装時に決める。Light数が問題になった場合に、画面分割のTiled / Forward+または空間分割のCluster / Grid方式へ移行する。

### Reflection Pass
`AfterForwardOpaque`で実行するPass。DeferredとForward Opaqueを描いた後の`SceneColorSrc`、Depth、GBufferのSurface情報、MaterialMetadataを使い、Skydome、Reflection Probe、SSRによる反射を評価して`SceneColorDst`へ加算する。完了後にSceneColorを交換する。初期の環境反射はSkydomeのみとし、Reflection Probeは後から追加する。

```text
Deferred Material ──────────── GBuffer Pass ─┐
Forward Opaque Material ────── GBuffer Pass ─┼→ Depth / Surface情報
Forward Opaque Material ────── Forward Pass ─┘          ↓
                                              SceneColorSrc
                                                    ↓
                                            Reflection Pass
                                           Skydome / Probe / SSR
                                                    ↓
                                              SceneColorDst → swap
```

Forward OpaqueをSSR対象にする場合、そのMaterialShaderにもGBuffer Passを持たせる。GBuffer PassはNormal、RoughnessとMaterialMetadataを残し、`ShadingModel = None`ならDeferred Lightingを行わない。初期実装では既存のGBufferを共用し、SSR専用のSurface Bufferへ分ける最適化は必要になってから検討する。

Reflection PassはSkydome / Probeから環境反射を取得し、SSRが有効な場合はSceneColorを探索した結果とConfidenceに応じて混ぜる。選ばれた反射元のRadianceは、対象ShadingModelの`IndirectSpecular`へ渡してSurface上の反射色へ変換する。MaterialMetadataの`SSRReceive`が無効なPixelはSSRの適用対象外とし、`ReflectionMask`を組込みSSRのbool Maskとして使う。Ray March、Confidence、Composite式は実装時に決める。

将来は組込みSSRと同じHook / I/O契約を持つCustom SSR Shaderへ差し替えられるようにする。Custom SSRはMaterial IDからGPU上のMaterialData Tableへアクセスし、MaterialごとのReflection Factorや追加Parameterを取得できる。Custom SSRとFactor参照は初期構成に含めない。

Transparentは初期のSSR対象に含めない。SSRの後に描くため反射探索にも映らず、自身にもSSRを適用しない。必要になった場合は透明物専用の方法を別途設計する。

### Fogなどの画面空間処理（追加候補）
OpaqueのSceneColorとDepthを使うFogは`AfterForwardOpaque`または`BeforeToneMap`へ登録できる。透明物にも同じFogを適用するなら、Transparent Shader側で評価するか、透明物後に画面全体へ適用するかを決める。Effectごとに必要な入力と合成対象を見てHookを選び、添付資料のPass順をそのまま固定しない。

### Forward Transparent
**入力:** `RenderType = Forward`の半透明MaterialShader Pass、OpaqueのDepth / SceneColor
**出力:** Blend後のHDR SceneColor

まずDepth Testし、通常はDepthを書かずにAlpha Blendする。重なりに依存する物は描画順を決める。屈折のためにOpaque SceneColorを読む場合は、読取元と書込先が同じResourceにならないよう一時Textureまたは別Passを設ける。OITなどの高度な透明処理は必要性が分かってから検討する。

### Post Process / Tone Map
HDR SceneColorから表示用のLDR色を作る。Bloom、被写界深度、Motion BlurなどHDR入力が必要なEffectは`BeforeToneMap`へ登録する。Tone Mapは`RGBA8_UNORM`などのLDR RenderTextureへ書き出す。Tone Map後のColor Gradingや表示色上のEffectはLDR側の`AfterToneMap`を使う。HDR SceneColorのPing-PongとLDR ColorのPing-Pongは別Resource Domainとして扱う。

Effectごとに必要な入力と順序を明示する。Render Graphは依存、寿命、Barrierを解決するが、見た目へ影響するEffect順を推測しない。Pipeline設定がHook内の並び順を保持する。

### Upscale（必要な場合）
内部描画解像度と出力解像度を分ける場合に置く。`AfterToneMap`のEffectはTone Map後・Upscale前の内部解像度LDR、`AfterUpscale`のEffectは出力解像度LDRで動く。UIは原則Upscale後の最終RenderTextureへ合成する。初期構成では等倍出力と2枚のHDR SceneColorを使い、LDR側のPing-Pong資源はEffectの有無と解像度に応じて確保する。

### UI合成 / Present
ゲーム側UIが必要なら最終RenderTextureへ重ねる。EditorではStage済みImGui DrawDataをNativeRenderがScene / Game ViewのSRVを使って合成する。最終RenderTextureからSwapChain BackBufferへCopyし、BackBufferをPresent可能なResource Stateへ遷移させてからPresentする。RenderTextureとBackBufferは同じResourceとして扱わない。[D3D12 Resource StateとPresent](https://learn.microsoft.com/en-us/windows/win32/direct3d12/using-resource-barriers-to-synchronize-resource-states-in-direct3d-12)

## Screen Effect Hook
HookはScreenEffectShaderを挿入できるPipeline上の同期点。初期候補を次に示す。

| Hook                    | 利用できる主な入力                           | 用途例                   |
| ----------------------- | ----------------------------------- | --------------------- |
| `AfterGBuffer`          | Depth、GBuffer                       | SSAO / GTAO、Surface解析 |
| `AfterDeferredLighting` | Lighting成分、Depth、GBuffer            | Lighting成分への補正        |
| `AfterForwardOpaque`    | Opaque SceneColor、Depth、GBuffer     | SSR、Opaque対象の画面効果     |
| `AfterTransparent`      | Transparent合成済みHDR SceneColor、Depth | 透明物を含むFogなど           |
| `BeforeToneMap`         | HDR SceneColor、Depth、必要な中間Texture   | Bloom、DoF、Motion Blur |
| `AfterToneMap`          | Tone Map済み・Upscale前のLDR RenderTexture | Upscale前のColor Effect |
| `AfterUpscale`          | Upscale済み・UI合成前のLDR RenderTexture | 出力解像度のEffect、UIを含めない最終画面効果 |

すべてのHookを毎Frame実行するわけではない。登録されたEffectがなければ処理もSceneColorの交換も行わない。同じHook内ではPipeline設定の登録順を基本とし、Render GraphがI/O依存との矛盾、未生成Resource、循環を検証する。Shaderの意図を推測して別Hookへ移動したり、見た目が変わる順序へ自動調整したりはしない。

### SceneColorの交換
SceneColorを更新するEffectは`SceneColorSrc`をSRVとして読み、`SceneColorDst`をRTVとして書く。Effect完了後に論理的なSrc / Dstを交換する。

```text
Current SceneColor = A
SSR:       A → B  / swap
Fog:       B → A  / swap
Tone Map:  A → B  / swap
Current SceneColor = B
```

ForwardやTransparentのRaster PassをSceneColorへ直接Blendする場合は、その時点の`Src`をLoadするColor Attachmentとして使う。次のScreen Effectはもう一方を`Dst`にする。Render GraphがSceneColorの論理Versionと物理Texture A / Bの割当を管理し、同じTextureを同時に入力と出力へBindingしない。

## 将来のForward / Forward+切替
Forward-onlyではOpaqueもForwardへ移し、GBufferとDeferred Lightingを省く。GBufferを入力とするDecal、AO、Outlineなどは、そのままでは使えないため別経路か機能制限が必要になる。Forward+ではDepthを基にLightを絞り、そのLight ListをForwardのShadingで使う。多数のLightが実際の負荷になる場合に検討する。[Forward+原論文](https://takahiroharada.wordpress.com/wp-content/uploads/2015/04/forward_plus.pdf)

現行のMaterialShader DSLでGBuffer用Surface関数しか持たないMaterialは、単に描画方式を切り替えてもForwardでは描けない。Forward用Passを用意するか、Surface関数とLightingShaderからForward PixelShaderを生成する仕組みが必要になる。したがって切替はPassのON/OFFだけでは成立しない。

Forward-onlyでもSSRなどのScreen EffectにSurface情報が必要なら、対象MaterialへGBuffer相当の情報出力Passを残すか、Screen Effect用のSurface Bufferを別途用意する。描画方式をForwardへ切り替えることと、画面効果用の情報を破棄することは分けて考える。

初期実装で実行中の自由な切替を支える必要はない。RenderItemの契約とMaterialShaderの`RenderType`を保ち、別方式が必要になった時にPipeline構成とResourceを切り替える。切替時にGPU使用中のResourceをどう破棄するか、Forward側でLightingShaderを再利用するかは未解決。

## 今後の設計・実測項目
- Oct Normalの精度とEmission方式は実装後に実画像・GPU時間・VRAM Trafficを測る。
- 初期構成ではLighting成分の専用Bufferを持たず、Deferred LightingはHDR SceneColorへ出力する。Debug / 計測用の成分別Bufferは必要になった時点で検討する。
- Reflection PassのSSR Ray March、Confidence、Composite式はSSR実装時に決める。Custom SSRとMaterial Factorは初期構成に含めない。
- Forwardの最大Light数Nと選別基準を実装時に決め、負荷が問題になった場合に画面分割または空間分割へ移行する。
- `RenderPhase`と`ZTest` / `ZWrite` / `Blend`の許可する組合せ、既定値、診断規則を実装時に確定する。
- EditorのScene View / Game Viewは原則2回描画し、実装後の計測結果から解像度・更新頻度・Effectの省略を検討する。

Material Flags、ShadingModel IDの解決時点、Screen Effect Hookの統合方針は本資料の現時点の方向として扱う。bit 1 / bit 2の意味とID解決のタイミングは設計判断済みで、具体的なHeader / Public APIとPipeline設定形式は実装側レビューで正式化する。
