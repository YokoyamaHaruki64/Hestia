# Material Shader

HestiaのMaterialShader Assetと、DSLから生成されるProperty定義・Pass・PSO構築情報のクラス設計モック。
Shader全体で**1つのPropertyレイアウト**を共有し、全PassのVS/PSに同じProperty宣言を挿入する。
DSL構文の正本はHestiaDesignの`Graphics/MaterialShaderDSL.md`とし、ここではCPU型・所有関係・DX12接続を整理する。

## MaterialShader

**役割**

`.matsh`を解析・コンパイルしたShader定義を所有するAsset。
Property Schema・共通CBufferレイアウト・Pass群を保持し、Materialは本体への**非所有ポインタ**で参照する。

```cpp
namespace Hestia
{
    class MaterialShader
    {
    public:
        const MaterialPropertyDesc* FindProperty(
            MaterialPropertyID id) const;

        const MaterialPropertyDesc* FindProperty(
            std::string_view name) const;

        const MaterialShaderPass* GetPass(
            uint32_t index) const;

        uint32_t GetConstantBufferSize() const;
        uint64_t GetRevision() const;

    private:
        AssetHandle<MaterialShader> m_handle;
        std::string m_name;

        std::vector<MaterialPropertyDesc> m_properties;
        std::unordered_map<MaterialPropertyID, uint32_t>
            m_propertyIndices;

        uint32_t m_constantBufferSize = 0;
        std::vector<MaterialShaderPass> m_passes;

        uint64_t m_revision = 1;

        // Reload導入時の検討事項
        // std::vector<Material*> m_materials;
    };
}
```

[MaterialPropertyDesc](#materialpropertydesc)  
[MaterialShaderPass](#materialshaderpass)  
[Material](./Material.md#material)  
[AssetHandle<T>](./AssetSystem.md#assethandlet)

- `m_handle`はAssetStorageへの登録時に設定する。`m_name`はファイル名ではなくDSL内の`MaterialShader "..."`の名前。
- `m_properties`の順序はDSL宣言順。`m_propertyIndices`は**名前由来の安定PropertyID → 配列Index**。IndexはGPU Byte Offsetではない。
- `m_constantBufferSize`はShader共通の`PerMaterial`定数バッファの有効バイト数。GPUのCBV配置時の256Byte Alignmentとは区別する。
- `m_passes`はDSLの宣言順。Pipeline実行順・描画分類は`RenderType` / `RenderPhase`でGraphicsが決定する。
- `m_revision`はShader Reloadによる内容変更を識別する。AssetHandleのGenerationとは別物。
- `m_materials`は逆索引案。初期はShader Reloadも個別Unloadも実装しないため、追加しない。
- ShaderがMaterialから直接参照される間は実体のアドレスを維持する。Reloadを実装する際も`MaterialShader`オブジェクト自体の置換ではなく内容更新を基本とする。

### MaterialPropertyDesc

**役割**

DSLの`Properties`宣言1件に対応するSchema。型・既定値・表示名と、コンパイル時に確定したGPU Binding情報を保持する。

```cpp
namespace Hestia
{
    using MaterialPropertyID = uint64_t;

    enum class MaterialPropertyType
    {
        Bool,
        Int,
        Float,
        Vector2,
        Vector3,
        Vector4,
        Color,
        Matrix3x3,
        Matrix4x4,
        Texture2D
    };

    struct MaterialPropertyDesc
    {
        MaterialPropertyID m_id = 0;
        std::string m_name;
        std::string m_displayName;

        MaterialPropertyType m_type;
        MaterialPropertyValue m_defaultValue;

        // ConstantBuffer Propertyのみ有効
        uint32_t m_offset = UINT32_MAX;
        uint32_t m_size = 0;

        // Texture等のResource Propertyのみ有効
        uint32_t m_resourceSlot = UINT32_MAX;

        // Range等のEditor向け情報は必要時に追加
    };
}
```

[MaterialPropertyValue](./Material.md#materialpropertyvalue)  
[Material](./Material.md#material)

- IDはProperty名から決定的な64bit Hashで作る。同名ならShaderが違っても同じID。生成入口は`Material::MakePropertyID(name)`に統一する。
- `MaterialPropertyValue`はMaterialShaderとMaterialの双方で必要な共通型。実装ヘッダーではMaterialクラス本体から独立した共有ヘッダーに定義し、相互Includeを避ける。
- MaterialごとのPropertyIDではなく、**名前そのもののID**。異なる名前のHash衝突はShaderロード時に検出してログ出力・失敗とする。
- `Range(min,max)`はFloat型＋Editor編集制約であり、別のGPU型にはしない。
- 数値・Vector・Matrix等は1つの`PerMaterial` CBufferへ格納。`Texture2D`はCBufferではなく個別のSRV Resource宣言を生成する。
- `m_offset` / `m_size`はコンパイル後のReflection結果から確定し、構造体のメモリ配置や宣言順から勝手に算出しない。
- 全Stage/Passで未使用のPropertyはReflectionに現れず、`m_offset`が無効値のままでもよい。Materialは型付き正本値を保持し、GPU Byte列への書き込みだけを省略する。
- Resource Bindingの生成規則はShader全体で共通。使用しないPassでバインディングが最適化除去されても、Property Schemaは消さない。

### MaterialShaderPass

**役割**

DSLの`Pass`1件に対応し、**PSO構築情報**と**描画の分類情報**を分離して保持する。

```cpp
namespace Hestia
{
    struct MaterialShaderPass
    {
        ShaderPipelineDesc m_pipeline;
        ShaderPassInfo m_info;
    };
}
```

[ShaderPipelineDesc](#shaderpipelinedesc)  
[ShaderPassInfo](#shaderpassinfo)

- Pass固有のProperty CBufferレイアウトは保持しない。全PassはShader共通の`MaterialPropertyDesc`を参照する。
- Passのタグ（名前）による参照を後から追加できるようにするが、初期はPassIndexで識別できればよい。

### ShaderPipelineDesc

**役割**

GraphicsがD3D12のPSOを生成するために参照する、Shader由来の固定設定。
PSO自体は保持せず、Bytecode・VS入力・Rasterizer/Blend/DepthStencil設定等を保持する。

```cpp
namespace Hestia
{
    struct ShaderPipelineDesc
    {
        ShaderBytecode m_vertexShader;
        ShaderBytecode m_pixelShader;
        VertexInputLayout m_inputLayout;

        RasterizerState m_rasterizer;
        BlendState m_blend;
        DepthStencilState m_depthStencil;
        PrimitiveTopologyType m_topology;
    };
}
```

[ShaderBytecode / VertexInputLayout](#shaderbytecode--vertexinputlayout)  
[RenderState](#renderstate)  
[PSOCacheKey](#psocachekey)

- `RasterizerState`等はHestia定義の複数Enum・Bool・数値からなる状態型であり、DX12の`D3D12_*_DESC`への変換はGraphicsで行う。
- `PrimitiveTopologyType`はPSO固定のTriangle/Line/Point区分。`IASetPrimitiveTopology`で指定する実際のTopologyとは別。
- RT/DS Format、MSAA、RootSignature等のGraphics側描画環境はここへ保持しない。PSO生成時にGraphicsが設定する。
- PSOキーはBytecodeやRenderState構造体を毎Drawハッシュ化せず、Shader・Revision・PassIndexで固定設定を識別する。

### ShaderPassInfo

**役割**

Pipelineの分類・実行順判断など、PSOの固定状態とは独立したPass情報。

```cpp
namespace Hestia
{
    enum class RenderType
    {
        GBuffer,
        Forward
    };

    enum class RenderPhase
    {
        Opaque,
        Masked,
        Transparent
    };

    struct ShaderPassInfo
    {
        std::string m_name;
        RenderType m_renderType = RenderType::GBuffer;
        RenderPhase m_renderPhase = RenderPhase::Opaque;
        uint8_t m_shadingModelID = 0;
    };
}
```

[MaterialShaderPass](#materialshaderpass)

- `RenderType`は出力先の意味。`GBuffer`はSurface情報、`Forward`はSceneColorへの直接出力。GBuffer出力は必ずしもDeferred Lightingを意味しない。
- `RenderPhase`は不透明/Masked/透明のPipeline分類・Sort指針。Depth/Blendの実際の状態は`ShaderPipelineDesc`に反映された明示設定が優先。
- `m_shadingModelID`はLightingShader名をShaderロード時に解決した値。未指定は`None`の予約ID。GBuffer用Wrapperに埋め込まれるため、描画時の文字列解決は行わない。
- `m_name`はPassの識別名/Tag。初期は無名Passも許容する。

### ShaderBytecode / VertexInputLayout

**役割**

コンパイル済みBytecodeをAsset側で所有し、VS ReflectionによるInputLayout情報をPSO生成に渡す。

```cpp
namespace Hestia
{
    struct ShaderBytecode
    {
        std::vector<std::byte> m_data;
    };

    struct VertexInputElement
    {
        std::string m_semanticName;
        uint32_t m_semanticIndex = 0;
        VertexFormat m_format;
        uint32_t m_inputSlot = 0;
        uint32_t m_offset = 0;
    };

    struct VertexInputLayout
    {
        std::vector<VertexInputElement> m_elements;
    };
}
```

[ShaderPipelineDesc](#shaderpipelinedesc)  
[MeshVertexStream](./AssetType.md#meshvertexstream)

- `ShaderBytecode`はVS/PSごとに保持。`D3D12_SHADER_BYTECODE`はPSO生成時に所有データから組み立てる一時View。
- InputLayoutはVS ReflectionからSemanticを読み、HestiaのVertexStream/Offsetとの対応を確定する。
- InputElementの`m_semanticName`は`std::string`で所有する。D3D12の`SemanticName`へ渡すポインタの寿命はPSO生成中だけ保証すればよい。

### RenderState

**役割**

Shader DSLの`Cull`・`Blend`・`ZTest`・`ZWrite`等を、PSO固定設定として表現する。
各Stateは複数のEnumや数値を持つ構造体。

```cpp
namespace Hestia
{
    enum class FillMode { Solid, Wireframe };
    enum class CullMode { None, Front, Back };
    enum class BlendMode { None, Alpha, Additive };
    enum class CompareFunc { Less, LessEqual, Greater, GreaterEqual, Equal, NotEqual, Always };
    enum class PrimitiveTopologyType { Triangle, Line, Point };

    struct RasterizerState
    {
        FillMode m_fill = FillMode::Solid;
        CullMode m_cull = CullMode::Back;
        bool m_frontCounterClockwise = false;
        bool m_depthClip = true;
        int32_t m_depthBias = 0;
        float m_depthBiasClamp = 0.0f;
        float m_slopeScaledDepthBias = 0.0f;
    };

    struct BlendState
    {
        BlendMode m_mode = BlendMode::None;
        // 必要時に詳細Factor / Operation / RT毎のBlendへ拡張
    };

    struct DepthStencilState
    {
        bool m_depthTest = true;
        bool m_depthWrite = true;
        CompareFunc m_depthFunc = CompareFunc::LessEqual;
        // Stencilは必要になった段階で追加
    };
}
```

[ShaderPipelineDesc](#shaderpipelinedesc)

- Shader全体に指定された値を既定値とし、Pass内で同じ設定が指定された場合は上書きする。
- Hestia DSLには`Blend = Alpha`等の簡略表現を出し、Renderer内部でDX12の複数Blend要素へ展開する。
- 将来のStencil/Blend拡張時も、PSOキャッシュキー側をRenderState各フィールドの組み合わせへ変える必要はない。

## PSOCacheKey

**役割**

Graphics内部で`ID3D12PipelineState`を検索するためのキー。MaterialShaderはPSOを所有しない。

```cpp
namespace Hestia
{
    struct PSOCacheKey
    {
        AssetHandle<MaterialShader> m_shader;
        uint64_t m_shaderRevision = 0;
        uint32_t m_passIndex = 0;
    };

    // GraphicsSystem / DX12BackEnd内部
    // std::unordered_map<PSOCacheKey,
    //     ComPtr<ID3D12PipelineState>, PSOCacheKeyHash> m_psoCache;
}
```

[MaterialShader](#materialshader)  
[ShaderPipelineDesc](#shaderpipelinedesc)

- `AssetHandle<MaterialShader> + Revision + PassIndex`がShader固有のPSO固定情報を特定する。
- `RenderType = GBuffer / Forward`からGraphicsがRTV/DSV Format・RT数等を選ぶ。RenderTargetリソースの実体はキーに含めない。
- **初期は**同じRenderTypeに対応するFormat・SampleCount・RootSignature等を固定する。この前提で、RenderTargetSignature等はキーへ含めない。
- 同じPassを異なるFormat/MSAA/RootSignature条件で使い分ける場合は、描画先のSignature等をキーへ追加する。設定を全体変更するだけならPSOキャッシュを無効化・再生成してもよい。
- キャッシュミス時にGraphicsがBytecode/状態/描画先条件からPSOを作成する。GPU ResourceとPSOの寿命はGraphics側で管理する。

## DSLロードと共通レイアウト

**データ生成フロー**

```text
AssetLoader<MaterialShader>
 ├─ .matsh読込 / ShaderParser
 │   ├─ 名前・Properties・Pass・SHARED_HLSL
 │   └─ Shader全体のState / Pass単位のOverride
 ├─ 共通Property HLSLの生成
 │   ├─ 数値系 → cbuffer PerMaterial : register(b2)
 │   └─ Texture2D等 → Resource : register(t...)
 ├─ 全PassのVS/PSへ同一の宣言とSHARED_HLSLを挿入
 ├─ Compile（GBufferはSurface→PS Wrapper生成）
 ├─ Reflection（CBuffer Offset/Size, SRV, VS Input）
 ├─ 全Stage/Passの使用情報をマージ・レイアウト検証
 ├─ MaterialShader生成
 └─ AssetSystem::Register<MaterialShader>() → Handle
```

[AssetLoader<T>](./AssetSystem.md#assetloadert)  
[Material](./Material.md#material)

- PropertyのCBuffer宣言は全PassのVS/PSで共通。**1 Shader = 1 Layout**、Material側にPass別byteキャッシュは作らない。
- 各PassのReflectionでPropertyが確認できたら、最初のOffset/Sizeを共通`MaterialPropertyDesc`に記録。別Passでも見つかったら一致を検証し、不一致ならロードエラー。
- **未使用だからReflectionに出ない**ことと**他PropertyのOffsetが詰め直される**ことは別。同一の宣言が使われていれば使用されるPropertyのOffsetは共通レイアウトに従う。
- すべてのPassで未使用のPropertyは型付きSchemaに残す。Reflectionで取得できないGPU Offsetは無効のままとする。
- `SHARED_HLSL`や`#pragma vertex/pixel/surface`、Surface生成、LightingShaderとの関係は既存DSL仕様を正とする。
- ファイル読込・Parser・Compile・Reflectの失敗はエラーログを出し、無効Handleを返す。復旧・詳細なError階層は初期に実装しない。

## 後回し・未決定

- Shader Reloadの全Material値移行・逆索引、旧Schema保持、PSO失効と描画中同期。
- Property型の最終網羅、Range等のEditor Metadataの具体的な表現、Hash方式。
- Texture/Samplerの最終的なRegister/Space配置とDescriptor Table構成。
- ShaderBytecodeのディスクキャッシュ、Include依存検出。
- RenderTypeごとのRT Format/MSAA変更、複数RootSignature、PSOキーの拡張。
- LightingShader・ScreenEffectShaderの具体的なAsset型・Loader。
- Async Compile、コンパイル済みHLSL Reflection情報の保存形式。

## 参照元

- [HestiaDesign / MaterialShaderDSL](https://github.com/YokoyamaHaruki64/HestiaDesign/blob/main/Graphics/MaterialShaderDSL.md)
- [HestiaDesign / Material](https://github.com/YokoyamaHaruki64/HestiaDesign/blob/main/Asset/Material.md)
- [GM_Framework / ShaderLoader.cpp](https://github.com/chaba0426/GM_Framework/blob/main/source/Graphics/Shader/ShaderLoader.cpp)
- [GM_Framework / ShaderReflector.cpp](https://github.com/chaba0426/GM_Framework/blob/main/source/Graphics/Shader/ShaderReflector.cpp)
