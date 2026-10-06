# Forward/Deferredのどちらでも使えるShaderの概要

**拡張子 :**`.matsh`

---

## 基本方針

Materialは必ず1つのMaterialShaderを持つ。
Shaderとしては、これ以外に`LightingShader`というShadingModel定義用のShaderを定義する予定。

MaterialShaderのPassは、レンダリング方式ではなく**出力先・実行フェーズ**を`RenderType`で指定する。

```hlsl
RenderType = GBuffer;
RenderType = Forward;
```

- **GBuffer**
  - SurfaceDataを生成し、エンジン側でGBufferへ出力する
  - Deferred Lighting用に限らず、SSAO / SSR / Outline等で使用するSurface情報の生成にも利用する
  - ShadingModelが指定されている場合、そのSurfaceはDeferred Lighting時に対応するLightingShaderで評価される
- **Forward**
  - PixelShaderから最終的なColorを直接出力する
  - 特殊なライティング、Rim、半透明などに使用する
  - 同一MaterialShader内でGBufferと併用可能
    詳しくは[[#RenderType]]へ

したがって、
**GBufferへの出力 ≠ Deferred Lighting**
となる。

---

## Sample

```hlsl
MaterialShader "Sample/Standard"
{
    Properties
    {
        _BaseColor ("Base Color", Color) = (1, 1, 1, 1)
        _BaseMap   ("Base Map", Texture2D) = "white"

        _Roughness ("Roughness", Range(0, 1)) = 0.5
        _Metallic  ("Metallic", Range(0, 1)) = 0.0
    }


    // ============================================================
    // GBuffer / Forward 共通
    // ============================================================

    SHARED_HLSL_BEGIN

    struct VSInput
    {
        float3 position : POSITION;
        float3 normal   : NORMAL;
        float2 uv       : TEXCOORD0;
    };

    struct PSInput
    {
        float4 positionCS : SV_POSITION;
        float3 positionWS : TEXCOORD0;
        float3 normalWS   : TEXCOORD1;
        float2 uv         : TEXCOORD2;
    };

    PSInput VSMain(VSInput input)
    {
        PSInput output;

        output.positionWS = TransformObjectToWorld(input.position);
        output.positionCS = TransformWorldToClip(output.positionWS);
        output.normalWS   = TransformObjectToWorldNormal(input.normal);
        output.uv         = input.uv;

        return output;
    }

    SHARED_HLSL_END


    // ============================================================
    // GBuffer
    // ============================================================

    Pass
    {
        RenderType = GBuffer;
        RenderPhase = Opaque;
        ShadingModel = DefaultLit;

        HLSL_BEGIN

        #pragma vertex VSMain
        #pragma surface SurfaceMain

        SurfaceData SurfaceMain(PSInput input)
        {
            SurfaceData surface =
                CreateStandardSurface(input.uv);

            surface.normal =
                normalize(input.normalWS);

            return surface;
        }

        HLSL_END
    }


    // ============================================================
    // Forward
    // ============================================================

    Pass
    {
        RenderType = Forward;
        RenderPhase = Transparent;

        Blend = Alpha;
        ZTest = LessEqual;
        ZWrite = Off;

        HLSL_BEGIN

        #pragma vertex VSMain
        #pragma pixel ForwardPS

        float4 ForwardPS(PSInput input) : SV_Target0
        {
            SurfaceData surface =
                CreateStandardSurface(input.uv);

            surface.normal =
                normalize(input.normalWS);

            float3 color =
                EvaluateShadingModel(
                    surface,
                    input.positionWS
                );

            return float4(color, surface.alpha);
        }

        HLSL_END
    }
}
```

## Engine側Common - Sample

```hlsl
struct SurfaceData
{
    float3 baseColor;
    float3 normal;

    float roughness;
    float metallic;

    float3 emission;
    float occlusion;

    float alpha;
    float4 customData;
};

SurfaceData CreateStandardSurface(float2 uv...);
float3 EvaluateShadingModel(...);

GBufferOutput OutputGBuffer(SurfaceData surface...);
```

- SurfaceData - [[#Surface]] 参照
- CreateStandardSurface - 標準的なSurface作成。Albedo,Roughness,Metalness等のサンプリングも行うためuvを要求。Propertyによってテクスチャ名が変わるので、サンプリング時にどうするかは再考の余地あり。多分引数は増える
- EvaluateShadingModel - 対応したShadingを行う。うまくやればForwardでも使えるかも
- OutputGbuffer - MRTを利用したGBufferへの出力surfaceを各Bufferに出力する。`RenderType = GBuffer`のPSに使用。ユーザー側に公開するhlslには定義しない

初期の`OutputGBuffer`はBaseColor + AO、OctNormal + Roughness + Metallic、Emission、Material / Object Metadataへ展開する。利用者はPackingを直接書かず、Engine側Wrapperが[[RenderingPipeLine#GBuffer: Opaque / Masked|Rendering PipelineのGBuffer構成]]に合わせて出力する。

---

## 基本概念

---

### RenderType

Passの出力先と、レンダリングパイプライン内での実行タイミングを指定する。

```hlsl
RenderType = GBuffer;
RenderType = Forward;
```

#### GBuffer

SurfaceDataをGBufferへ書き込む。

```
SurfaceData      ユーザー定義
    ↓
OutputGBuffer()  エンジン定義
    ↓
GBuffer
```

GBufferはDeferred Lighting専用のデータではなく、

- Deferred Lighting
- SSAO
- SSR
- Outline
- その他Screen Space処理

などから共通して利用可能なSurface情報として扱う。
ShadingModelが指定されている場合は、Deferred Lighting時に対応するLightingShaderを使用してLightingする。

DeferredLighting時の使用例

```hlsl
RenderType = GBuffer;
ShadingModel = DefaultLit;
```

同一MaterialShaderに後続のForward Passが存在する場合でも、GBuffer Passを持つことができる。

例えばキャラクターを、

```
GBuffer Pass
    → Normal / Roughness等を書き込む
    → ShadingModel = None によりDeferredLightingをスキップ

Forward Pass
    → Rim等を含む最終結果をSceneColorへ描画
```

のように描画できる。
この場合、Deferred Lightingの結果はForward Passによって上書きされるため描画結果には影響しないが、不要なLighting計算コストが発生する。
必要になった場合は、Forward描画予定のPixelをDeferred Lightingから除外する最適化を別途行う。

この構成はForward OpaqueをSSR対象にする場合にも使用する。Forward Passより前にGBuffer PassでNormal、Roughness、Reflection Maskなどを残し、Forward描画後の[[RenderingPipeLine#SSR]]がSurface情報を参照する。初期構成ではForward用に別のSSR Bufferを増やさず、既存GBufferを共用する。

#### Forward

PixelShaderの出力をSceneColorへ直接書き込む。

```
Material
    ↓
Forward PixelShader
    ↓
SceneColor
```

特殊なLighting、Rim、半透明など、GBufferを経由せず直接最終色を計算したい場合に使用する。

---

### ShadingModel

ShadingModelはDeferred Lightingされる際に使用するLightingShaderを指定する。

LightingShader "DefaultLit" を使用する場合

```hlsl
RenderType = GBuffer;
ShadingModel = DefaultLit;
```

ShadingModelを指定しない場合はNoneにフォールバックする。
NoneはLighting処理を行わない予約済みShadingModelとする。

例:

```
RenderType = GBuffer;
// ShadingModel未指定 → None
```

将来的にはGBuffer Pass時にStencilへ
Deferred Lighting対象かどうかを記録し、
Lighting PassのStencil TestによってNoneのPixelを
PS実行前に除外可能。

MaterialShader側ではSurfaceDataを生成し、LightingShader側ではそのSurfaceDataを使用してLightingを行う。

```
MaterialShader
    ↓
SurfaceData
    ↓
GBuffer
    ↓
LightingShader
    ↓
SceneColor
```

MaterialShaderのロード時に、指定された名前を登録済みLightingShaderのShadingModel Registryから検索する。GBuffer用PixelShader Wrapperを生成する際に解決済みのShadingModel IDを埋め込む。未登録名はMaterialShaderのロードエラーとし、実行時に文字列検索しない。`None`は予約済みIDを使う。

---

### RenderPhase

PassをPipelineのどの描画段階へ提出するかを指定する。

```hlsl
RenderPhase = Opaque;
RenderPhase = Masked;
RenderPhase = Transparent;
```

- `Opaque`: 不透明物。通常はDepth Test / Writeを有効にする。
- `Masked`: Alpha Testを行う不透明物。GBufferやDepth PrepassでOpaqueと分けて扱える。
- `Transparent`: 半透明物。Forward Transparent段階で順序付けして描く。

`RenderType`はGBufferへSurfaceを書くか、Forwardで最終色を書くかを表し、`RenderPhase`はOpaque / Transparentなどの配置とSort規則を表す。Forward PassにもOpaqueとTransparentの両方があるため、別の設定として扱う。

Depthの実際の振る舞いは`ZTest`と`ZWrite`で指定する。RenderPhaseから既定値を与えられるが、Pass内の明示指定を優先する。不自然な組合せはShaderロード時に警告またはエラーとする。

---

### Property

Shader内で使用する変数のうち、外部に公開する変数。CPUのコードやEditorから値を触ることが出来る
記法

```hlsl
プロパティ名     表示名       型       デフォルト値
_BaseColor ("Base Color", Color) = (1, 1, 1, 1)
```

**DSL側内部処理**

```
Propertyを1行ごとに取得
↓
分解
↓
各値をDescとして登録、Shaderに保持
Property名から安定したハッシュのPropertyIDを生成する
宣言順の配列indexとPropertyIDは区別する（[[Material]] を参照）
```

---

### SHARED_HLSL

ファイル内で使用する共通定義
主にVS,PSのInputや頂点変位などの共通処理などを定義する

**DSL側内部処理**

```
SHARED_HLSL_BEGIN~SHARED_HLSL_ENDを保持
↓
各Shaderのコンパイル時に先頭に挿入。挿入順はProperty→SHARED_HLSL
```

---

### Pass

一度の描画で呼び出される単位。主にVSとPSが書かれる

---

### ShaderType

GPU StateやRenderingのタイプを指定する

```
RenderType = GBuffer;
RenderPhase = Opaque;
ShadingModel = DefaultLit;
Blend = Alpha;
ZTest = LessEqual;
Zwrite = Off;
```

など
これらはShaderブロック内に書くことで全体に適用し、Pass内に書くことでそれをオーバーライドして設定できる。

**ShadingModelについて**
LightingShaderの名前(ファイル名ではない)と合わせる必要がある。
また、主にDeferred Lightingで使用するためForwardでの利用は未対応。  
将来的にはForward Passのコンパイル時に対応するLightingShaderを結合し、専用の評価関数として呼び出せるようにする予定。

**DSL側内部処理**

```
Shader単位のものを取得、保持
↓
Pass単位のパース時に見つかったものはそれを利用
↓
最終的にPass単位でそれぞれ持つ。OverrideされていないものはShader単位のものをコピー
```

---

### 独自Pragma

コンパイラに情報を伝えるためのデータ。実際はコンパイラに直接渡さず、DSL側が保持してコンパイラに渡す
現在はEntrypointの指定のみ対応
Vertex,PixelはそのままVS,PSに対応する
**Surfaceは次項へ  [[#Surface]]**

---

### Surface

`RenderType = GBuffer`ではPSを書かず、SurfaceDataを返す関数を定義する

**パースエラー条件**

```
・RenderType = GBuffer; なのに #pragma surface がない
・指定されたSurface関数を展開済みHLSLから発見できない
```

**DSL側内部処理**
下記コードをPSとしてコンパイルする

```hlsl
#include Engine_Common.hlsl // 例,実際にはincludeが増える可能性あり

Propertyの変数宣言
SHARED_HLSL
HLSL_BEGIN ~ END

GBufferOutput __GBufferPS(PSInput input)
{
    SurfaceData surface = SurfaceMain(input);    

    return OutputGBuffer(
        surface,
        CURRENT_SHADING_MODEL_ID
    );
}
```

`__GBufferPS` 生成方法(ユーザー定義部分の取得方法)

```
SurfaceMain → #pragma surface から取得
PSInput     → SurfaceMain の引数型から取得
```

#### SurfaceDSL処理順

1. Property宣言 + SHARED_HLSL + Pass内HLSLを結合する
2. 上記HLSLをD3DPreprocessする
   \#include / \#define 等を展開し、Surface関数探索用のHLSLを生成する
3. 展開済みHLSLから、
   \#pragma surfaceで指定されたSurfaceMainを検索する
4. SurfaceMainの関数シグネチャを取得し、
   SurfaceFunctionDescを生成する
5. `ShadingModel`名を登録済みRegistryからIDへ解決する
   未指定時は`None`の予約IDを使用し、未登録名はロードエラーとする
6. SurfaceFunctionDesc.inputTypeと解決済みIDを使用して
   `__GBufferPS`を生成する
7. **Preprocess前の結合済みHLSL**の末尾に
   `__GBufferPS`を追加する
8. `__GBufferPS`をPixelShaderのEntryPointとしてD3DCompileする
   この時、include / define / CURRENT_SHADING_MODEL_ID等は
   Preprocess前のものなので再度Preprocessされる

Surface関数が見つからない、
または生成した`__GBufferPS`から正しく呼び出せない場合は、
D3DCompile時にShaderコンパイルエラーとなる

```c++
struct SurfaceFunctionDesc 
{
    std::string name = "SurfaceMain";
    std::string returnType = "SurfaceData";
    std::string inputType = "PSInput";
};
```
