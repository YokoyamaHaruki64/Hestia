# Lighting

Deferred Renderingにおける光源評価と反射の設計草案。LightingShaderはShadingModelごとの光の応答を定義し、Passの順序、Resource、反射元の選択はRendererが管理する。

## 基本方針

- Deferred Lightingは`Direct`と`IndirectDiffuse`を評価し、HDR SceneColorへ出力する。
- `IndirectSpecular`はDeferred Lightingでは出力しない。Forward Opaqueの後に行うReflection Passが、Skydome、Reflection Probe、SSRから反射元を選び、ShadingModelごとの`IndirectSpecular`を呼ぶ。
- MaterialのEmissionはLighting評価から分け、GBuffer2からHDR SceneColorへ加算する。
- SSAOは遮蔽値Textureを出力し、`IndirectDiffuse`へ適用する。Directや反射へ一律に掛けない。
- 3成分を個別Textureとして常設しない。必要な値はPixelShader内の変数として分け、初期構成ではSceneColorへ合成する。成分別の出力はDebugや計測が必要になった時点で追加する。

## 全体像

```text
GBuffer + Depth ──→ SSAO
       │              ↓
       └──────→ Deferred Lighting
                    Direct + IndirectDiffuse × AO + Emission
                                      ↓
                                HDR SceneColor
                                      ↓
                                Forward Opaque
                                      ↓
                              Reflection Pass
                          Skydome / Probe / SSR
                                      ↓
                                HDR SceneColor
```

Reflection Passは現在のSceneColorを読み、次のSceneColorへ書くPing-Pong Passとする。[[RenderingPipeLine]]と[[RenderGraph]]がResourceのVersionとState遷移を管理する。

## Light

初期対応するLight種別はDirectional、Spot、Point、Areaとする。Areaは初期段階ではPoint Light相当として近似し、形状光源としての正確な評価は必要になってから検討する。

Sceneに登録できるLight数は固定上限を設けない。各ViewでGPUへ渡すLightは最大128個とし、Directionalは常に候補へ含める。Point、Spot、AreaはCameraまたはObjectとの距離と設定された影響範囲から候補を選ぶ。Forward Materialが実際に評価する個数は、この候補から選んだ最大N個とし、初期値は8とする。Nは設定で変更できるものとし、詳細な選別スコアは実装時に決める。

Light数が負荷になった場合は、Tiled / ClusterなどのLight Cullingを検討する。初期構成でForward+を必須にはしない。

## Shadow

初期実装はDirectional LightのCSMとし、毎フレーム更新する。SpotとPointのShadowも初期対応の対象とし、AreaはPoint Light相当の近似Shadowから始める。生成したShadow Texture、Cascade Matrix、Split DistanceはDeferred LightingとForward Lightingが共通で参照する。Shadowを使うLightだけをShadow Passへ渡し、すべてのLightがShadowを持つことは要求しない。

初期の更新頻度は毎フレームとする。負荷が問題になった場合に、Static Light / Static Casterの更新停止やLightごとの更新間隔を追加する。

## Lighting成分

### Direct
Directional / Point / Spot / AreaからSurfaceへ届く直接光を評価する。DiffuseとSpecularを含む。Shadowを使う場合は、対象Lightの可視性をこの成分へ反映する。
BRDFとAttenuationはEngine側の共通Includeとして提供する。関数構成、具体式、パラメータ、最終的な近似方法は詳細設計・実装時に決める。

### IndirectDiffuse
初期構成ではSkydomeから生成したSH9をDiffuse IBLとして使う。Material AOとScreen-space AOを組み合わせて適用する。

```text
IndirectDiffuse × MaterialAO × ScreenSpaceAO
```

後からLight Probeを追加し、場所ごとの間接Diffuseを供給できるようにする。

### IndirectSpecular / Reflection

`IndirectSpecular`は、反射元のRadianceをSurface上の反射色へ変換するShadingModel側の関数として残す。Reflection Passが反射元を用意し、この関数を呼ぶ。

```text
Skydome / Reflection Probe / SSR
              ↓
      Reflection Radianceを選択
              ↓
LightingShader.IndirectSpecular
              ↓
         HDR SceneColorへ加算
```

初期の環境反射はSkydomeを使う。Reflection Probeは後から追加する。反射元のTextureは輝度を保持できる`R11G11B10_FLOAT`を候補とし、元Assetの圧縮形式、Mip構成、Sampling方法は詳細設計・実装時に決める。SSRは`AfterForwardOpaque`で試行し、取得できた反射とSkydome / ProbeをConfidenceに応じて混ぜる。`IndirectSpecular`の評価自体はSSRの有無を意識しない。

```hlsl
float3 reflectionRadiance =
    lerp(environmentRadiance, ssrRadiance, ssrConfidence);

float3 reflection =
    IndirectSpecularMain(surface, reflectionContext);
```

## GBufferとForwardの関係

Reflection PassはGBufferのNormal、Roughness、Metallic、BaseColor、Depth、MaterialMetadataを使う。`SSRReceive`と`ReflectionMask`は組込みSSRの適用対象を決める。

Forward Opaqueが共通Reflection Passで反射を受けるには、同じMaterialがGBuffer Passも持ち、Reflectionに必要なSurface情報を書き込む。GBuffer側の`ShadingModel = None`ならDeferred Lightingでは評価されず、Forward Passだけが本体色を描く。

GBufferを書かないForward MaterialはReflection Passの受け手にならない。反射をForward Shader内で独自に描く。SSRの映り込み対象として扱うには、SSRが参照するDepth / Hi-ZへそのObjectが反映されている必要がある。初期実装では、SSRに関わるForward OpaqueへGBuffer Passを持たせる。

## ShadingModelの登録と選択

LightingShaderのロード時に名前をShadingModel Registryへ登録し、8bitのIDを割り当てる。`None`は予約IDとする。MaterialShaderのPixelShader Wrapper生成時に名前をIDへ解決してGBufferへ埋め込み、描画中に文字列検索を行わない。

初期実装では、Deferred LightingとReflection PassのPixelShader内でShadingModel IDを`switch`し、対応する関数を呼ぶ。複数ShadingModelによる分岐や命令数が問題になった場合に、分類・別Pass・Computeなどを比較する。

ForwardではEngine側の共通Lighting関数を使い、生成Wrapperで`SHADING_MODEL_ID`をコンパイル時に埋め込む。ユーザーが参照する固定関数の宣言はEngine Includeとして提供し、Visual Studioの補完対象にできる形を検討する。Macroの展開位置、Includeの生成、補完用の定義は詳細設計・実装時に確定する。

## 詳細設計・実装時に確定する項目

- BRDFとAttenuationの具体式、共通Includeの関数構成、パラメータ
- ForwardのLight選別スコア、Nの上限、Light Listの受け渡し
- Spot / Point / Area ShadowのResource構成、Atlas、面数、更新最適化
- Material AOとSSAOの合成式・強度
- SSRのRay March、Confidence、SceneColorへの合成式
- Skydome / Reflection ProbeのFormat、圧縮、Mip、Sampling方法
- Forward共通Lighting関数のMacro、生成Wrapper、補完用Include

関連: [[RenderingPipeLine]] / [[LightingShaderDSL]] / [[RenderGraph]] / [[RenderTargetBudget]] / [[MaterialShaderDSL]]
