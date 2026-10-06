# ScreenEffectShader DSL
SSR、SSAO、Bloom、Tone Mapなど、画面全体または画面上の各Pixelを処理するShaderを記述する3つ目のDSL。MaterialShaderやLightingShaderとは異なり、描画対象のMaterialではなくRendering Pipeline上のTextureを入出力にする。拡張子は未確定。

## 基本方針
ScreenEffectShaderは、実行できるHook、調整用Property、使用する入力、生成する出力、PixelShaderのEntryPointを定義する。

- MaterialShaderのような`Pass`やGPU Stateは持たない
- LightingShaderのようなShadingModel定義でもない
- `Hook`でPipeline上の大分類を指定する。正確な順序は同じHookへ登録されたEffectの並び順と[[RenderGraph]]の依存関係で決まる。
- 初期仕様では1つのScreenEffectShaderにつき1つのHookを指定する。同じ処理を複数Hookで使う必要が出た時に拡張する。
- SceneColorを変更するEffectは、同じTextureを読み書きせず、`SceneColorSrc`から読み`SceneColorDst`へ書く。
- Depth、GBuffer、Lighting成分などの入力は、登録先Hookで利用可能なものだけを指定できる。

## Sample
Forward Opaque後に動くSSRの記述例。構文名や型は草案である。

```hlsl
ScreenEffectShader "Reflection/SSR"
{
    Hook = AfterForwardOpaque;

    Properties
    {
        _MaxDistance ("Max Distance", Float) = 50.0
        _Thickness   ("Thickness", Float) = 0.1
        _Intensity   ("Intensity", Range(0, 1)) = 1.0
    }

    Inputs
    {
        SceneColorSrc;
        SceneDepth;
        GBufferNormal;
        GBufferRoughness;
        MaterialMetadata;
    }

    Outputs
    {
        SceneColorDst;
    }

    HLSL_BEGIN

    #pragma pixel SSRMain

    float4 SSRMain(ScreenInput input) : SV_Target0
    {
        float depth = SampleSceneDepth(input.uv);
        float3 normal = SampleGBufferNormal(input.uv);
        float roughness = SampleGBufferRoughness(input.uv);
        uint metadata = SampleMaterialMetadata(input.uv);
        bool receivesSSR = TestMaterialFlag(metadata, SSRReceive);
        bool reflectionMask = TestMaterialFlag(metadata, ReflectionMask);

        SSRResult reflection = TraceSSR(
            input.uv,
            depth,
            normal,
            roughness,
            _MaxDistance,
            _Thickness
        );

        float3 source = SampleSceneColor(input.uv);
        float3 color = CompositeSSR(
            source,
            reflection,
            roughness,
            receivesSSR && reflectionMask ? _Intensity : 0.0
        );

        return float4(color, 1.0);
    }

    HLSL_END
}
```

Pipeline設定は同じHook内での実行順、ON / OFF、Propertyの上書き値を保持する。

```text
Hook: AfterForwardOpaque
    - Shader: Reflection/SSR
      Enabled: true
      _MaxDistance: 40.0
```

SSRのRay March、History、Confidence、粗さに応じたBlurなどはShader内部またはSSR専用の補助処理で扱う。ScreenEffectShader DSLはSSR固有の計算を規定しない。

初期構成の組込みSSRはMaterialMetadataの`SSRReceive`と`ReflectionMask`をboolとして使い、MaterialごとのReflection Factorは参照しない。将来は同じHook / I/O契約を満たすCustom SSRへ差し替えられるようにする。Custom SSRではMaterial IDからMaterialData Tableを参照し、Factorや追加Parameterを取得できる。Custom SSRとMaterialData Table入力は初期仕様に含めない。

## 基本概念
### Hook
Effectが実行されるPipeline上の大分類。

```hlsl
Hook = AfterForwardOpaque;
```

Hookは利用可能な入力Resourceと、前後にある組込みPassを制限する。
例えばSSRはForward OpaqueのSceneColorを必要とするため`AfterForwardOpaque`、SSAOはGBufferを必要とするため`AfterGBuffer`を指定する。
HookはEffect間の正確な順序を表さない。同じHook内の順序はPipeline設定に従い、最終的なResource依存とBarrierはRender Graphが構築する。Shaderが指定したHookとPipeline設定の登録先が一致しない場合はエラーとする。
### Inputs
Effectが読むPipeline Resourceを宣言する。宣言名はNativeRenderが提供する既知のSemanticへ対応させる。

```hlsl
Inputs
{
    SceneColorSrc;
    SceneDepth;
    GBufferNormal;
    GBufferRoughness;
}
```

Hookに到達していないResource、生成されていない任意Effectの出力、同時に書込中のResourceは入力にできない。必要な入力がない場合はPipeline構築時のエラーとする。ResourceのFormatやDescriptorをDSL利用者へ直接指定させない。

候補となる入力は次の通り。

- `SceneColorSrc`
- `SceneDepth`
- GBufferの各Surface情報
- `MaterialMetadata` / `ObjectMetadata`
- Shadow、Velocity、HistoryなどPipelineが明示的に公開するTexture
- 直前までに生成済みのScreen Effect出力

すべてを最初から実装する必要はない。Effectの追加に合わせてSemanticを増やす。

### Outputs
Effectが生成するResourceを宣言する。

```hlsl
Outputs
{
    SceneColorDst;
}
```

SceneColorを更新する場合は`SceneColorDst`を使う。AOやSSRの中間結果を後続Effectへ渡す場合は、Pipeline側で定義した専用出力を使えるようにする。複数Render TargetやUAVを必要とするEffectは、実際に必要になった時点で構文を拡張する。

### ScreenInput
Full Screen Triangleから得られる画面座標とUVを持つEngine定義の入力。利用者は頂点Shaderを書かず、`#pragma pixel`でPixelShaderだけを指定する。

```hlsl
struct ScreenInput
{
    float4 positionCS : SV_POSITION;
    float2 uv         : TEXCOORD0;
};
```

### Properties
Effectの調整値をEditorやPipeline設定から変更する。MaterialShaderと同じProperty構文とParserを再利用する。値はMaterial単位ではなく、登録されたEffect Instance単位で保持する。同じShaderを複数回登録する場合も、それぞれ別の値を持つ。調整値がないShaderは`Properties`を省略できる。

```hlsl
Properties
{
    _MaxDistance ("Max Distance", Float) = 50.0
    _Thickness   ("Thickness", Float) = 0.1
}
```

## SceneColor Ping-Pong
初期構成ではHDR SceneColor用に同じFormat・解像度のTextureを2枚用意し、現在の読取元を`Src`、次の書込先を`Dst`として扱う。Tone Map後はLDR RenderTextureのResource Domainへ切り替える。HDRとLDRでFormat・解像度が変わるため、物理TextureのPairはDomainごとに持つ。

```text
Effect A: SceneColor A (Src) → SceneColor B (Dst)
                                   ↓ swap
Effect B: SceneColor B (Src) → SceneColor A (Dst)
                                   ↓ swap
次のPassは現在のSrcを読む
```

Effect実行後に論理Handleを交換するため、後続Passは物理Texture A / Bを意識しない。Effectが無効なら交換しない。最終的なSrcがTone Mapや次の描画段階の入力になる。

SceneColor以外の専用Bufferまで一律にPing-Pongさせない。AO、SSR History、Bloom Pyramidなどはそれぞれの用途に合わせて所有する。Tone Map、Upscale、UI合成のように解像度やFormatが変わるEffectは、入力Domainと出力Domainを分ける。Upscale後にもEffectを行う場合は、出力解像度LDR側の書込先を用意する。最終RenderTextureからBackBufferへのCopyはPing-PongのEffectではない。

## DSL側内部処理
1. `Hook`、Property、`Inputs`、`Outputs`、`#pragma pixel`を解析する。
2. Pipeline設定の登録先とShaderのHookが一致し、各入力が利用できるか検証する。
3. Engine定義のFull Screen VertexShader、Property宣言、Resource Binding、利用者のHLSLを結合する。
4. 宣言したI/OからRender Graph Nodeを生成する。
5. `SceneColorSrc` / `Dst`などの論理Resourceを、そのFrameで使う物理TextureへBindingする。
6. PixelShaderを実行し、完了後に必要なResource State遷移とSceneColorの交換を行う。

Shaderコンパイルエラーに加え、存在しないSemantic、同じResourceの不正な読書き、Hook時点で未生成の入力を診断する。複数解像度、Compute Shader、History管理は初期仕様へ含めず、必要なEffectから拡張する。

## 未解決事項

- 拡張子と、`Inputs` / `Outputs`の最終構文。
- SSAOのような中間Texture生成と、SceneColorを直接更新するEffectの表現を同じDSLでどこまで扱うか。
- Effect Instanceの並び順、ON / OFF、Property上書き値を保存するPipeline設定形式。
- Half / Quarter Resolution、Compute Shader、History Resourceを扱う拡張方法。
- Reflection PassとScreenEffectShaderの境界。Reflection Pass内で使うSSRをScreenEffectShaderとして記述するか、専用のWrapperを生成するか。

関連: [[RenderingPipeLine]] / [[MaterialShaderDSL]] / [[LightingShaderDSL]]
