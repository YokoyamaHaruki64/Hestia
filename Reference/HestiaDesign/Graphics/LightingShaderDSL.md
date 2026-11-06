# DeferredRenderingにおけるShadingModelを定義する
**拡張子 :** `.litsh`

---
## 基本方針

LightingShaderはShadingModelのライティング処理を定義するShader。
MaterialShaderが生成した`SurfaceData`を入力として受け取り、以下の3種類の評価を提供する。

```
Direct
IndirectDiffuse
IndirectSpecular
```

- **Direct**
    - Directional / Point / Spot等の直接光によるライティング
    - Diffuse / Specularの両方を含んでよい
- **IndirectDiffuse**
    - GI / LightProbe / Irradiance等による間接Diffuse成分
    - SSAO等による遮蔽の適用対象として利用可能
- **IndirectSpecular**
    - Reflection Passが選んだSkydome / Reflection Probe / SSRの反射元を、Surface上の間接Specularへ変換する

3種類のEntry Pointは必須ではない。未記述のEntry Pointは`DefaultLit`の同種の関数を使う。独自の評価を行う成分だけを記述し、意図的にLightingを出さない場合は0を返す関数を記述する。Fallbackは`DefaultLit`のPropertyも使うため、`DefaultLit`を先にロードする。

初期構成では`DefaultLit`以外のShadingModelを流用する指定は持たない。必要になった時点で、依存関係・循環参照・コンパイル時の関数結合を含めて設計する。

LightingShader側では、それぞれの最終結果を返す関数のみをDSLへ公開する。

関数内部の構造は自由とし、
```
float3 CalcBRDF(...);
float3 CalcDiffuse(...);
float3 CalcSpecular(...);
```
などの補助関数への分割方法はDSLでは規定しない。

---
## Sample
```hlsl
LightingShader "DefaultLit"
{
    Properties
    {
        _IndirectIntensity ("Indirect Intensity",Range(0, 2)) = 1.0
    }

    SHARED_HLSL_BEGIN

    float3 CalcDiffuse(...);
    float3 CalcSpecular(...);

    float DistributionGGX(...);
    float GeometrySmith(...);
    float3 FresnelSchlick(...);

    SHARED_HLSL_END

    HLSL_BEGIN

    #pragma direct DirectMain
    #pragma indirect_diffuse IndirectDiffuseMain
    #pragma indirect_specular IndirectSpecularMain

    float3 DirectMain(
        SurfaceData surface,
        LightingContext context)
    {
        float3 diffuse = ...;
        float3 specular = ...;

        return diffuse + specular;
    }


    float3 IndirectDiffuseMain(
        SurfaceData surface,
        LightingContext context)
    {
        float3 irradiance = ...;

        return irradiance *
               surface.baseColor *
               _IndirectIntensity;
    }


    float3 IndirectSpecularMain(
        SurfaceData surface,
        ReflectionContext context)
    {
        float3 reflection = context.reflectionRadiance;

        return reflection *
               _IndirectIntensity;
    }

    HLSL_END
}
```

---
## 基本概念
### Lightingの出力

LightingShaderは最終的なSceneColorを直接出力しない。
Deferred Lightingは`Direct`と`IndirectDiffuse`を評価し、RendererがHDR SceneColorへ合成する。`IndirectSpecular`はDeferred Lightingで出力せず、Forward Opaqueの後に行うReflection Passから呼ぶ。

```
DirectMain
    ↓
Direct


IndirectDiffuseMain
    ↓
IndirectDiffuse × AO
    ↓
HDR SceneColor


IndirectSpecularMain
    ↓
Reflection Pass
    ↓
HDR SceneColor
```

SSAOは[[ScreenEffectShaderDSL]]が作る遮蔽Textureとして`IndirectDiffuse`へ適用する。Reflection PassはSkydome、Reflection Probe、SSRから反射元のRadianceを選び、`IndirectSpecular`へ渡す。SceneColorの合成、Ping-Pong、Pass順、Resource Stateは[[RenderingPipeLine]]とRendererが管理する。

初期構成では3成分の専用BufferやMRTを常設しない。成分別のBuffer出力はDebugや計測で必要になった場合に追加する。

---
### Property

LightingShader全体で共有する外部設定値。
MaterialShaderのPropertyとは異なり、Materialごとの値ではなく、そのShadingModelを使用する全Materialで共有される。
```
Properties
{
           変数名               表示名             型      デフォルト値
    _IndirectIntensity ("Indirect Intensity",Range(0, 2)) = 1.0
}
```

Material単位でLightingに渡したい値は、MaterialShader側で`SurfaceData.customData`へ格納する。
```
Material Property
    ↓
SurfaceData.customData
    ↓
LightingShader
```

**DSL側内部処理**
MaterialShaderのProperty Parserを再利用する。
```
Propertyを取得
↓
各値をDescとして登録
↓
LightingShader単位で保持
↓
Shader用ConstantBuffer等へ展開
```

---
### SHARED_HLSL

**[MatarialShaderDSL/SHARED_HLSL](MaterialShaderDSL#SHARED_HLSL)** も参照

1LightingShaderファイル内で共有するHLSL定義。

```
SHARED_HLSL_BEGIN

float DistributionGGX(...);
float GeometrySmith(...);
float3 FresnelSchlick(...);

SHARED_HLSL_END
```
MaterialShaderと同じ構文・Parserを使用する。

主に、
```
BRDF関数
Fresnel
GGX
共通計算
独自Lighting Utility
```
などを記述。

---
### 独自Pragma

[MaterialShaderDSL/独自Pragma](MaterialShaderDSL#独自Pragma) も参照

LightingShaderでは必要なLighting EntryPointだけを指定する。
```
#pragma direct DirectMain
#pragma indirect_diffuse IndirectDiffuseMain
#pragma indirect_specular IndirectSpecularMain
```

それぞれ、
```
direct
    → Direct Lighting

indirect_diffuse
    → Indirect Diffuse Lighting

indirect_specular
    → Indirect Specular Lighting
```
に対応する。

指定されていないEntry Pointは`DefaultLit`の同種の関数へ解決する。例えば反射だけ独自にするShadingModelは、`#pragma indirect_specular`だけを持てばよい。

該当するLightingを使用しないShadingModelでは、明示的に0を返す関数を指定する。
```
float3 IndirectSpecularMain(
    SurfaceData surface,
    ReflectionContext context)
{
    return 0.0;
}
```

---
### Lighting関数

`Direct`と`IndirectDiffuse`の基本シグネチャは以下とする。
```
float3 LightingFunction(
    SurfaceData surface,
    LightingContext context);
```

`IndirectSpecular`はReflection Passから呼ばれ、反射元のRadianceを含む`ReflectionContext`を受け取る。

```hlsl
float3 IndirectSpecularFunction(
    SurfaceData surface,
    ReflectionContext context);
```

`SurfaceData`はMaterialShaderと共通のEngine定義。
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
```

`LightingContext`にはDirect / IndirectDiffuseに必要な共通情報を格納する。
候補:
```
WorldPosition
ViewDirection
Camera情報
Light情報 / LightList
Shadow情報
GI / Probe情報
その他Rendering Context
```
正確な構造はLighting実装時に確定する。

`ReflectionContext`の初期構成は、View DirectionとReflection Passが選んだ`reflectionRadiance`を持つ。SSRのRay MarchやProbeの検索は含めず、これらはReflection Passの責務とする。

---
## DSL側内部処理
### 構文解析

LightingShaderにはPassを持たない。
そのためMaterialShaderに比べて構文は単純になる。
```
LightingShader
├ Properties
├ SHARED_HLSL
└ HLSL_BEGIN ～ HLSL_END
    └ 必要な #pragma direct / #pragma indirect_diffuse / #pragma indirect_specular
```

MaterialShader Parserのうち、
```
Property Parser
SHARED_HLSL Parser
HLSL Parser
Pragma Parser
```
を再利用する。

以下はLightingShaderでは使用しない。
```
Pass
RenderType
GPU State
#pragma vertex
#pragma pixel
#pragma surface
```
---
### Compile
Lighting関数の引数型はEngine側で規定するため、MaterialShaderのSurfaceと異なり、Wrapper生成用にユーザー関数の引数型を推論する必要はない。
DSL側ではpragmaから関数名を取得する。pragmaがないEntry Pointは`DefaultLit`の同種の関数へ解決する。

```
DirectMain
    ← #pragma direct

IndirectDiffuseMain
    ← #pragma indirect_diffuse

IndirectSpecularMain
    ← #pragma indirect_specular
```
Renderer側でDeferred Lighting用とReflection Pass用のPixelShader Wrapperを生成する。Deferred Lighting Wrapperは`Direct`と`IndirectDiffuse`を評価してSceneColorへ出力する。Reflection Wrapperは`IndirectSpecular`を評価して現在のSceneColorへ加算する。

例:
```hlsl
float4 __DeferredLightingPS(ScreenInput input) : SV_Target0
{
    SurfaceData surface =
        LoadSurfaceData(input);

    LightingContext context =
        CreateLightingContext(input, surface);

    float3 direct =
        DirectMain(surface, context);

    float3 indirectDiffuse =
        IndirectDiffuseMain(surface, context);

    return float4(
        direct + indirectDiffuse * LoadAO(input) + surface.emission,
        1.0
    );
}
```
関数の戻り値や引数が規定されたシグネチャと一致しない場合は、生成されたWrapperの`D3DCompile`時にShaderコンパイルエラーとなる。

---
## MaterialShaderとの関係
LightingShaderのロード時に名前をShadingModel Registryへ登録し、8bitのShadingModel IDを割り当てる。`None`は予約済みIDとする。同名登録、ID上限超過、ロード失敗はエラーとする。

MaterialShaderが`ShadingModel`名を指定した場合は対応するLightingShaderを先にロードし、MaterialShaderのPixelShader Wrapper生成時に名前をIDへ解決して埋め込む。描画中に文字列からIDを検索しない。

MaterialShader側で、
```
RenderType = GBuffer;
ShadingModel = DefaultLit;
```
と指定された場合、
```
MaterialShader
    ↓
SurfaceData
    ↓
GBuffer
    ↓
ShadingModel ID = DefaultLit
    ↓
LightingShader "DefaultLit"
    ├ Direct
    ├ IndirectDiffuse
    └ IndirectSpecular（Reflection Passで評価）
```
としてLightingを行う。
`ShadingModel = None`の場合はLightingShaderによるLightingを行わない。

---
## Forward対応
初期実装ではLightingShaderは主にDeferred Lightingから利用する。
Forward PassではEngine側の共通Lighting関数を結合し、生成Wrapperで`SHADING_MODEL_ID`をコンパイル時に埋め込む。

```hlsl
#define SHADING_MODEL_ID 3

float3 EvaluateForwardLighting(
    SurfaceData surface,
    LightingContext context)
{
    return EvaluateLighting(
        SHADING_MODEL_ID,
        surface,
        context);
}
```

ユーザーが参照する関数宣言は固定Engine Includeとして提供する。Macroの定義位置、生成Include、Visual Studioの補完用定義は詳細設計・実装時に確定する。

将来的にMaterialShaderのForward PassへShadingModel固有の処理を結合し、
```
EvaluateDirect(...)
EvaluateIndirectDiffuse(...)
EvaluateIndirectSpecular(...)
```
等のEngine定義関数を介してForward PixelShaderから呼び出せるようにする。初期実装では、DeferredのShadingModel ID分岐とForwardのコンパイル時IDを共存させる。
