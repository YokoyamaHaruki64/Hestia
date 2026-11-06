# Material

HestiaのMaterial Assetと、MaterialShaderのPropertyに対応する値・GPU用キャッシュのクラス設計モック。
Materialは`MaterialShader*`を直接保持し、**型付きProperty値を正本**としたうえで、共通CBuffer LayoutへPack済みのbyte列をキャッシュする。
普段は名前指定の型別Setter/Getterを使用でき、頻繁なアクセスではPropertyIDを事前計算して使用する。

## Material

**役割**

MaterialShaderを非所有ポインタで参照し、Propertyの実値とGPU転送用byteキャッシュを保持する。
ShaderのSchema、Pass、PSOやGPUリソースそのものは所有しない。

```cpp
namespace Hestia
{
    class Material
    {
    public:
        // インスタンス非依存。名前から決定的なPropertyIDを生成
        static MaterialPropertyID MakePropertyID(
            std::string_view name);

        // 名前指定：日常的な操作
        bool SetFloat(std::string_view name, float value);
        bool SetVector3(std::string_view name, const Vector3& value);
        bool SetColor(std::string_view name, const Color& value);
        bool SetTexture(
            std::string_view name,
            AssetHandle<Texture> texture);

        // ID指定：頻繁な操作
        bool SetFloat(MaterialPropertyID id, float value);
        bool SetVector3(MaterialPropertyID id, const Vector3& value);
        bool SetColor(MaterialPropertyID id, const Color& value);
        bool SetTexture(
            MaterialPropertyID id,
            AssetHandle<Texture> texture);

        bool GetFloat(std::string_view name, float& out) const;
        bool GetFloat(MaterialPropertyID id, float& out) const;
        // 他の対応型にもSet/Getの名前版・ID版を用意

        bool SetProperty(
            MaterialPropertyID id,
            const MaterialPropertyValue& value);
        bool GetProperty(
            MaterialPropertyID id,
            MaterialPropertyValue& out) const;

        // Shader差し替え。値配列・byteキャッシュを新Schemaへ合わせる
        bool SetShader(MaterialShader* shader);
        MaterialShader* GetShader() const;

    private:
        AssetHandle<Material> m_handle;
        MaterialShader* m_shader = nullptr; // 非所有

        std::vector<MaterialPropertyValue> m_values;
        std::vector<std::byte> m_constantBufferCache;

        uint64_t m_shaderRevision = 0;
        uint64_t m_valueRevision = 0;
    };
}
```

[MaterialShader](./MaterialShader.md#materialshader)  
[MaterialPropertyDesc](./MaterialShader.md#materialpropertydesc)  
[MaterialPropertyValue](#materialpropertyvalue)  
[AssetHandle<T>](./AssetSystem.md#assethandlet)

- `m_handle`はAssetStorage登録時に割り当てる。`m_shader`はAssetStorageが`unique_ptr<MaterialShader>`で所有するオブジェクトへの直接参照。
- Shader解決はMaterialのロード時または`SetShader`実行時だけでよく、**Property操作ごとにAssetSystemへ問い合わせない**。
- `m_values`はShaderの`m_properties`と**同じ配列Index**で型付き値を保持する。Shader側のPropertyID→Indexを使ってアクセスする。
- `m_constantBufferCache`はGPU用の再構築可能なキャッシュ。Shader共通のCBuffer Layoutに従い、各PropertyのReflection OffsetへPackする。Passごとには持たない。
- `m_valueRevision`は実値の変更時に更新。`m_shaderRevision`はキャッシュを構築した時点のShader Revisionとする。
- 初期実装ではGPU Buffer・Descriptor・PSOをMaterial自身に保持しない。Frame用のUpload/CBV領域はGraphics側が管理する。
- 個別Unloadを持たない初期段階ではShader実体のアドレスを維持する。将来のReloadでもShader本体を入れ替えずに内容を更新することを基本とする。

### MaterialPropertyValue

**役割**

MaterialのProperty値をCPU上で型付きに保持するためのVariant。
数値系とTexture参照の双方を表せるが、実際にCBufferへ書くのは数値系のみ。

```cpp
namespace Hestia
{
    using MaterialPropertyValue = std::variant<
        bool,
        int32_t,
        float,
        Vector2,
        Vector3,
        Vector4,
        Color,
        Matrix3x3,
        Matrix4x4,
        AssetHandle<Texture>
    >;
}
```

[Material](#material)  
[MaterialPropertyDesc](./MaterialShader.md#materialpropertydesc)  
[Texture](./AssetType.md#texture)

- `MaterialPropertyDesc::m_type`とVariantの具体型を照合してSet/Get時の型を検証する。
- `Range(0,1)`などのDSL上の編集制約はFloat値＋PropertyのEditor Metadataとして扱う。
- `Texture2D`の値は`AssetHandle<Texture>`。SRV IndexやGPU Resourceのアドレスは保持しない。
- CPU側のColor/MatrixとGPU上の表現が異なる場合は、byteキャッシュへのPack時に変換する（HLSL Boolは32bit、Matrixの行列配置等に注意）。
- すべてのPassで未使用のPropertyも正本`m_values`には残す。

### PropertyアクセスとMakePropertyID

**役割**

名前指定APIは便利な入口、ID指定APIは実際の更新処理として統一する。
`MakePropertyID`はShaderやMaterialインスタンスに依存しないstatic関数とする。

```cpp
// 普段の利用
material->SetFloat("_Roughness", 0.5f);
material->SetColor("_BaseColor", color);

// 高頻度の場合はIDを保持する
static const MaterialPropertyID roughnessID =
    Material::MakePropertyID("_Roughness");

material->SetFloat(roughnessID, roughness);

// 名前版はID版に転送
bool Material::SetFloat(std::string_view name, float value)
{
    return SetFloat(MakePropertyID(name), value);
}
```

[Material](#material)  
[MaterialShader](./MaterialShader.md#materialshader)

- `MakePropertyID(name)`は決定的な64bit Hashの生成のみ。Shader内のPropertyが実在するかの検証はしない。
- ID指定時は`m_shader->FindProperty(id)`で存在・型を検証し、不正ならエラーログを出して`false`を返す。
- Shaderが違っても同名Propertyは同じID。Property配列のIndexはShaderによって変わるため、IDからIndexへの解決は現在のShaderで行う。
- 名前指定は毎回Hash生成が増えるが、日常のMaterial操作では使いやすさを優先する。
- 旧GM_FrameworkのMaterialIDとShaderPropertyIndexを結合する`MaterialPropertyID`方式は採用しない。

### ConstantBufferCache

**役割**

型付き正本値からGPU転送形式へ変換したbyte列。Materialにつき1個持つ。
GPUの`ID3D12Resource`は持たず、Graphicsが描画時にフレーム用Upload領域へコピーする。

```text
SetProperty(PropertyID, value)
    ↓
MaterialShader::FindProperty(id) → Property Index / Desc
    ↓
型検証
    ↓
m_values[index] = value（正本）
    ↓
CBuffer対象かつ有効Offsetあり
    └─ m_constantBufferCache[offset] へPack
    ↓
m_valueRevision++（値が変わった場合）

描画時:
m_constantBufferCache
    → GraphicsのFrame用CB領域（256byte境界）
    → b2 PerMaterialへBind
    → Pass共通レイアウトとして利用
```

[MaterialShader](./MaterialShader.md#materialshader)

- byteキャッシュはShader共通Layoutで1回構築し、通常の`SetProperty`では変更箇所だけを更新する。全Propertyを毎DrawでPackし直さない。
- キャッシュサイズは`MaterialShader::GetConstantBufferSize()`を使う。D3D12のCBV/GPUアドレス配置時の256byte AlignmentはGraphicsが行う。
- TextureなどのResource Propertyはbyteキャッシュへ書かない。描画時に`m_values`内のTexture HandleをGraphicsがBindingする。
- Reflectionで全Pass未使用のためOffsetが無効なPropertyは、`m_values`への書き込みだけ行う。未使用化してもMaterial値を失わない。
- Shaderが変更されたときは新Schemaへ値を移行し、byteキャッシュ全体を再構築する。通常のSet時にShader Revisionが一致していることを前提とする。
- GPUに実際に転送する領域のFrame寿命・Fenceや、Draw単位のOverrideの一時byteコピーはGraphicsで扱う。

### MaterialOverrides（後続）

**役割**

Drawごとの値上書き用。共有Materialの`m_values`やbyteキャッシュは変更しない。

```cpp
namespace Hestia
{
    struct MaterialOverrides
    {
        std::unordered_map<
            std::string, MaterialPropertyValue
        > m_values;
        uint64_t m_revision = 0;
    };
}
```

[Material](#material)

- 名前→値のマッピングは既存HestiaDesign案を参考にする。コンテナの具体型は初期の確定事項ではない。
- 描画用コピーへOverrideをPackする。`Material::SetProperty`と異なり、永続的な基準値は変えない。
- RenderItemからGraphicsへSubmitする際、可変なMaterial状態を描画後まで無条件に参照しないためのFrame内データ確定方法は描画システム設計で詰める。
- 初期のMaterialShader/Materialロード実装にOverride機構は必須としない。

## Shader参照とMaterialロード

**所有関係**

```text
AssetStorage<MaterialShader>
└─ unique_ptr<MaterialShader> ←─────────────┐
                                            │ 非所有
AssetStorage<Material>                      │
└─ unique_ptr<Material>                     │
   ├─ MaterialShader* ──────────────────────┘
   ├─ MaterialPropertyValue[]（正本）
   └─ std::byte[]（GPU用キャッシュ）

GraphicsSystem
└─ PSO Cache / GPU Frame CB（GPU実体所有）
```

[AssetSystem](./AssetSystem.md#assetsystem)  
[MaterialShader](./MaterialShader.md#materialshader)

**AssetLoader<Material>の流れ**

```cpp
AssetHandle<Material> AssetLoader<Material>::Load(
    std::string_view name,
    std::string_view path)
{
    // Materialの保存データを読む（Shader名 / Property上書き値）

    auto shaderHandle =
        AssetSystem::Load<MaterialShader>(shaderName);
    MaterialShader* shader =
        AssetSystem::Get<MaterialShader>(shaderHandle);

    if (!shader)
    {
        // LogError
        return {};
    }

    auto material = std::make_unique<Material>();

    // material->SetShader(shader) でSchema既定値を初期化
    // Materialファイルから個別Property値を反映

    return AssetSystem::Register<Material>(
        name, material.release());
}
```

[AssetLoader<T>](./AssetSystem.md#assetloadert)  
[MaterialShader](./MaterialShader.md#materialshader)

- Loader自身がRegisterまで実行し、他Assetと同様に`AssetHandle<Material>`を返す。
- ShaderはLoadで重複生成せずCacheから共有する。`AssetSystem::Get<MaterialShader>`で取得したポインタをMaterialが保持する。
- `SetShader`ではSchemaに合わせて既定値を初期化する。別Shaderへの差し替え時に同名・同型の既存Propertyを継承する考え方を維持するが、Reloadとの連携や移行不能時のError状態管理は後回し。
- Material登録時にはGPUリソースの実体を生成しない。PSOはGraphics側で描画条件に応じて必要時生成する。
- Shaderへの逆索引`vector<Material*>`はShader Reloadを実装する段階で検討する。Material自身はShaderを所有しない。

## 後回し・未決定

- 全Property型の詳細なGetter/Setterの網羅、Rangeや色編集等のEditor用Metadata。
- Shader Reloadと全Material値移行の整合性、逆索引登録・解除。
- 描画単位のMaterialOverridesとSubmit時のMaterial値確定。
- 共有Materialを複数Drawで利用する際のCBコピー再利用最適化。
- Material保存形式・Serialize詳細、Shader/Texture名からの依存解決。
- DLL境界における直接`Material*`操作・Variantの扱い。実装に必要になるまで境界変換用クラスを追加しない。
- 詳細な失敗復旧・Rollback・非同期化。初期はエラーログと`bool` / 無効Handleで扱う。

## 参照元

- [MaterialShader（このリポジトリ）](./MaterialShader.md)
- [HestiaDesign / Material](https://github.com/YokoyamaHaruki64/HestiaDesign/blob/main/Asset/Material.md)
- [HestiaDesign / MaterialShaderDSL](https://github.com/YokoyamaHaruki64/HestiaDesign/blob/main/Graphics/MaterialShaderDSL.md)
- [GM_Framework / Material.h](https://github.com/chaba0426/GM_Framework/blob/main/include/Graphics/Material.h)
- [GM_Framework / Material.cpp](https://github.com/chaba0426/GM_Framework/blob/main/source/Graphics/Material.cpp)
