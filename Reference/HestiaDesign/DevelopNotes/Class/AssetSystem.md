# Asset System

Engine内部のAsset管理クラスとGame.dll向け公開APIのクラス設計モック。
**導入順はMesh・Texture → MaterialShader・Material**。AssetSystemが登録情報DB・型別Loader・型別Storageを持ち、GraphicsへGPU資源生成を依頼する。以下のTupleは4型の設計到達形であり、実装中は完成した型から順次有効化する。
失敗処理はまずエラーログ出力を中心とし、非同期・復旧・詳細な状態管理は設計しない。

## AssetSystem

**役割**

`Load<T>`・`Get<T>`・`Register<T>`を公開する。共通の名前解決・Cache検索の後、型別Loaderに処理を委譲する。
**LoaderがAsset生成・Registerまで実行し、戻り値は全型で`AssetHandle<T>`に統一する。**

```cpp
namespace Hestia
{
    class GraphicsSystem;

    class AssetSystem
    {
    public:
        AssetSystem();
        ~AssetSystem();

        bool Initialize(
            std::string_view databasePath,
            GraphicsSystem* graphics);
        void Finalize();

        template<class T>
        static AssetHandle<T> Load(std::string_view name);

        template<class T>
        static T* Get(AssetHandle<T> handle);

        // assetの所有権は呼出し時点でAssetSystemへ移る
        template<class T>
        static AssetHandle<T> Register(
            std::string_view name, T* asset);

    private:
        template<class T>
        AssetHandle<T> LoadImpl(std::string_view name);

        template<class T>
        AssetHandle<T> RegisterImpl(
            std::string_view name, T* asset);

        template<class T>
        AssetLoader<T>& GetLoader();

        template<class T>
        AssetStorage<T>& GetStorage();

        // Meshだけ登録直後のGPU Uploadを行う
        void OnRegistered(AssetHandle<Mesh> handle, Mesh& mesh);

    private:
        AssetDB m_assetDB;

        std::tuple<
            AssetLoader<Mesh>,
            AssetLoader<Texture>,
            AssetLoader<MaterialShader>,
            AssetLoader<Material>
        > m_loaders;

        std::tuple<
            AssetStorage<Mesh>,
            AssetStorage<Texture>,
            AssetStorage<MaterialShader>,
            AssetStorage<Material>
        > m_storages;

        GraphicsSystem* m_graphics = nullptr; // 非所有
        static AssetSystem* s_instance;
    };
}
```

[AssetDB](#assetdb)  
[AssetLoader<T>](#assetloadert)  
[AssetStorage<T>](#assetstoraget)  
[AssetHandle<T>](#assethandlet)  
[Mesh / Texture](./AssetType.md#mesh)  
[MaterialShader](./MaterialShader.md#materialshader)  
[Material](./Material.md#material)  
[Engine](./Engine.md#engine)  
[AssetAPI](#assetapi)

- EngineがAssetSystemを所有。`s_instance`はその実体へのprivateな非所有ポインタ。
- AssetDBはYAMLの名前・型・Pathを解決するのみ。CPU Asset本体はAssetSystemが直接所有するTuple内のAssetStorageへ格納する。
- `AssetLoader<T>`は**完全特殊化**、`AssetStorage<T>`と`Load/Get/Register/LoadImpl/RegisterImpl`は汎用テンプレート。
- `GetLoader<T>()`と`GetStorage<T>()`は`std::get<AssetLoader<T>>` / `std::get<AssetStorage<T>>`で取得する。非対応型はコンパイルエラー。
- Meshの手動RegisterはGPU Uploadまで実行する。Textureは画素を持たないため、**初期はファイルLoadのみGPU Texture生成に対応**する。型の汎用宣言としての`Register<Texture>`は残るが、手動呼出しではGPU Resourceを作れないため利用対象外とする。
- MaterialShader・Materialの内部型とLoader契約は[MaterialShader](./MaterialShader.md) / [Material](./Material.md)に定義する。実装時はMesh・Textureが動いた後に追加する。
- 個別Unload・RenderTexture Asset・GPUのみの解放・LightingShader等への対応は後回し。

**Initialize**

```cpp
bool AssetSystem::Initialize(
    std::string_view databasePath,
    GraphicsSystem* graphics)
{
    m_graphics = graphics;
    s_instance = this;

    if (!m_assetDB.Initialize(databasePath))
        return false;

    if (!GetLoader<Mesh>().Initialize())
        return false;

    if (!GetLoader<Texture>().Initialize(graphics))
        return false;

    if (!GetLoader<MaterialShader>().Initialize())
        return false;

    if (!GetLoader<Material>().Initialize())
        return false;

    return true;
}
```

`AssetLoader<Texture>`のみGraphicsSystemの非所有ポインタを持つ。Loaderが一時画素データを保持するため、TextureのGPU UploadはTextureLoader内で実行する。
`AssetLoader<MaterialShader>`はDSLのParser/Compile/ReflectionからShaderを構築し、`AssetLoader<Material>`は依存ShaderをLoad/GetしてMaterialを登録する。どちらもPSOを生成しない。
このInitialize例は4型を有効化した際の到達形。段階的実装中は未実装Loaderの初期化をまだ呼ばない。
`Initialize`失敗はエラーログを出して`false`を返す程度に留める。

**AssetSystem.cppに置く汎用処理**

```cpp
template<class T>
AssetLoader<T>& AssetSystem::GetLoader()
{
    return std::get<AssetLoader<T>>(m_loaders);
}

template<class T>
AssetStorage<T>& AssetSystem::GetStorage()
{
    return std::get<AssetStorage<T>>(m_storages);
}

template<class T>
AssetHandle<T> AssetSystem::Load(std::string_view name)
{
    return s_instance->LoadImpl<T>(name);
}

template<class T>
T* AssetSystem::Get(AssetHandle<T> handle)
{
    return s_instance->GetStorage<T>().Get(handle);
}

template<class T>
AssetHandle<T> AssetSystem::Register(
    std::string_view name, T* asset)
{
    return s_instance->RegisterImpl<T>(name, asset);
}

template<class T>
AssetHandle<T> AssetSystem::LoadImpl(std::string_view name)
{
    auto& storage = GetStorage<T>();

    if (auto handle = storage.Find(name); handle.IsValid())
        return handle;

    const AssetInfo* info = m_assetDB.Find(name);
    if (!info)
    {
        // LogError: 未登録の名前
        return {};
    }

    // info->m_type と T の型照合。不一致はエラーログを出して{}を返す。

    // Loaderが内部でRegisterまで行い、Handleを返す。
    return GetLoader<T>().Load(name, info->m_path);
}

template<class T>
AssetHandle<T> AssetSystem::RegisterImpl(
    std::string_view name, T* asset)
{
    // 既存ポインタを所有する。make_unique<T>(asset)は使わない。
    std::unique_ptr<T> owner(asset);

    auto& storage = GetStorage<T>();
    auto handle = storage.Register(name, std::move(owner));

    if (!handle.IsValid())
    {
        // LogError: 名前重複や登録失敗
        return {};
    }

    if constexpr (std::is_same_v<T, Mesh>)
    {
        OnRegistered(handle, *storage.Get(handle));
    }

    return handle;
}

void AssetSystem::OnRegistered(
    AssetHandle<Mesh> handle, Mesh& mesh)
{
    // m_graphics->UploadMesh(handle, mesh);
    // Upload失敗時はエラーログだけ出す。詳細な復旧は後回し。
}
```

[AssetLoader<T>](#assetloadert)  
[AssetStorage<T>](#assetstoraget)  
[AssetDB](#assetdb)

- `LoadImpl<T>`はCache → DB名・型・Path解決 → Loaderを呼ぶだけ。型ごとの`if constexpr`によるロード分岐は不要。
- `RegisterImpl<T>`は`T*`を引き取り、`unique_ptr<T>`に変換してStorageへ登録する。登録失敗時もポインタの所有権は呼出し元へ戻さない。
- Meshのみ`RegisterImpl`から`OnRegistered`を呼ぶ。Textureは`RegisterImpl`でメタ情報だけ登録し、Loader側でGPUへ画素データを渡す。
- 現段階の失敗処理は**エラーメッセージ出力**を基本とする。見つからない名前・デコード失敗等は無効Handle、無効HandleのGetは`nullptr`。GPU Uploadの失敗はログのみとし、登録取消・再試行・Ready状態などは作らない。

**テンプレート定義の配置**

AssetSystemのテンプレート関数本体は`AssetSystem.cpp`に置き、Mesh / Textureだけ明示的インスタンス化する方針。

```cpp
// AssetSystem.cpp末尾、公開関数を具体型ごとに生成
template AssetHandle<Mesh>
AssetSystem::Load<Mesh>(std::string_view);
template Mesh*
AssetSystem::Get<Mesh>(AssetHandle<Mesh>);
template AssetHandle<Mesh>
AssetSystem::Register<Mesh>(std::string_view, Mesh*);

template AssetHandle<Texture>
AssetSystem::Load<Texture>(std::string_view);
template Texture*
AssetSystem::Get<Texture>(AssetHandle<Texture>);
template AssetHandle<Texture>
AssetSystem::Register<Texture>(std::string_view, Texture*);

template AssetHandle<MaterialShader>
AssetSystem::Load<MaterialShader>(std::string_view);
template MaterialShader*
AssetSystem::Get<MaterialShader>(AssetHandle<MaterialShader>);
template AssetHandle<MaterialShader>
AssetSystem::Register<MaterialShader>(std::string_view, MaterialShader*);

template AssetHandle<Material>
AssetSystem::Load<Material>(std::string_view);
template Material*
AssetSystem::Get<Material>(AssetHandle<Material>);
template AssetHandle<Material>
AssetSystem::Register<Material>(std::string_view, Material*);
```

内部テンプレートも同じ`.cpp`内で定義する。`AssetStorage<T>`の汎用実装は基本ヘッダーへ置く。
AssetSystem.hでは`std::tuple`による値所有のために対応する全Loader特殊化の**完全なクラス定義**が必要。
LoaderのヘッダーはAssetSystem.hをIncludeせず、必要な場合のみ各Loaderの`.cpp`からIncludeする。

### AssetDB

**役割**

YAMLのAsset名 → Type / Pathを管理し、Load時に検索する。Cache・Asset本体・Loaderは持たない。

```cpp
namespace Hestia
{
    enum class AssetType
    {
        Mesh,
        Texture,
        MaterialShader,
        Material
    };

    struct AssetInfo
    {
        AssetType m_type;
        std::string m_path;
    };

    class AssetDB
    {
    public:
        bool Initialize(std::string_view databasePath);
        const AssetInfo* Find(std::string_view name) const;

    private:
        std::unordered_map<std::string, AssetInfo> m_entries;
    };
}
```

[AssetSystem](#assetsystem)

- DBの登録情報は起動時に読み込む。名前は正規化（従来方針では英字の大小文字を区別しない）。PathはAssetsルート相対。
- AssetDBの`type`は初期`Mesh`・`Texture`から始め、MaterialShader・Materialを順次追加する。検索時に要求型と照合する。
- 手動`Register`の名前はCacheの名前と衝突させない。YAML登録名との全型共通の衝突判定については後続実装で整理する（Loaderの`Register`はYAML登録名を正常に使用する必要がある）。
- DBの手動Reload、ファイル移動や名前変更の自動追跡は初期対象から外す。

### AssetStorage<T>

**役割**

型別のAssetを`unique_ptr<T>`で所有し、連番Indexでアクセスする。名前→HandleのCacheも持つ。
`Get`による貸出ポインタが後続の登録によって移動しないようにする。

```cpp
namespace Hestia
{
    template<class T>
    class AssetStorage
    {
    public:
        AssetHandle<T> Find(std::string_view name) const;

        AssetHandle<T> Register(
            std::string_view name,
            std::unique_ptr<T> asset);

        T* Get(AssetHandle<T> handle);
        const T* Get(AssetHandle<T> handle) const;

    private:
        struct Slot
        {
            std::unique_ptr<T> m_asset;
            uint32_t m_generation = 1;
        };

        std::vector<Slot> m_slots;
        std::unordered_map<
            std::string, AssetHandle<T>
        > m_nameToHandle;
    };
}
```

[AssetHandle<T>](#assethandlet)  
[Mesh / Texture](./AssetType.md#mesh)

- Storageは汎用テンプレート。型ごとのIndexは0から連番で発行し、IndexとGenerationで照合する。
- `Slot`がVector再配置で移動しても、`unique_ptr`が指すAsset実体は移動しない。
- `Register`ではHandleを発行し、各対応Asset（Mesh / Texture / MaterialShader / Material）の`m_handle`に設定する。
- MaterialはMaterialShader実体を非所有ポインタで参照する。Finalize時は**MaterialをMaterialShaderより先に破棄する**。
- 初期はUnloadと空きIndex再利用を実装しない。AssetはAssetSystem終了まで保持する。
- `Find`は正規化した名前を使って検索し、登録時に`m_nameToHandle`へ記録する。

### AssetHandle<T>

**役割**

型ごとのStorageとGPUResourceStorageで共通して使用する識別子。

```cpp
namespace Hestia
{
    template<class T>
    struct AssetHandle
    {
        uint32_t m_index = UINT32_MAX;
        uint32_t m_generation = 0;

        bool IsValid() const
        {
            return m_index != UINT32_MAX;
        }
    };
}
```

[AssetStorage<T>](#assetstoraget)

- 型は`T`が表す。異なるAsset型のHandleが同一Indexでも問題ない。
- Generationは将来のIndex再利用に備える。`IsValid`は無効値判定で、Storageに存在する保証ではない。
- GraphicsのGPUResourceStorageもIndexとGenerationを共有するため、`TextureHandle`等への変換表は作らない。

### AssetLoader<T>

**役割**

型固有のファイル読込・CPU Asset生成からAssetSystemへのRegisterまで行う。`Load`の戻り値は共通で`AssetHandle<T>`。
完全特殊化したクラスごとに`Initialize`引数と内部状態を変えられる。

```cpp
namespace Hestia
{
    class GraphicsSystem;

    template<class T>
    class AssetLoader;

    template<>
    class AssetLoader<Mesh>
    {
    public:
        bool Initialize();

        AssetHandle<Mesh> Load(
            std::string_view name,
            std::string_view path);

    private:
        // Mesh用のファイル解析・Vertex Stream構築
    };

    template<>
    class AssetLoader<Texture>
    {
    public:
        bool Initialize(GraphicsSystem* graphics);

        AssetHandle<Texture> Load(
            std::string_view name,
            std::string_view path);

    private:
        GraphicsSystem* m_graphics = nullptr; // 非所有
    };

    template<>
    class AssetLoader<MaterialShader>
    {
    public:
        bool Initialize();
        AssetHandle<MaterialShader> Load(
            std::string_view name,
            std::string_view path);

    private:
        // DSL Parser / HLSL Compiler / Reflection等
    };

    template<>
    class AssetLoader<Material>
    {
    public:
        bool Initialize();
        AssetHandle<Material> Load(
            std::string_view name,
            std::string_view path);

    private:
        // Shader名・Property値の解析
    };
}
```

[AssetSystem](#assetsystem)  
[Mesh / Texture](./AssetType.md#mesh)  
[Texture](./AssetType.md#texture)  
[MaterialShader](./MaterialShader.md#materialshader)  
[Material](./Material.md#material)

**Mesh Loaderの処理例**

```cpp
AssetHandle<Mesh> AssetLoader<Mesh>::Load(
    std::string_view name, std::string_view path)
{
    auto mesh = std::make_unique<Mesh>();

    // pathを読み、頂点StreamとIndexを構築
    // 失敗時はエラーログを出して{}を返す

    return AssetSystem::Register<Mesh>(name, mesh.release());
}
```

MeshはCPU Asset自体が頂点Streamを持つため、`RegisterImpl<Mesh>` → `OnRegistered`がGPU Uploadを行う。

**Texture Loaderの処理例**

```cpp
AssetHandle<Texture> AssetLoader<Texture>::Load(
    std::string_view name, std::string_view path)
{
    DirectX::ScratchImage image;

    // pathからWIC等で画像デコード
    // 失敗時はエラーログを出して{}を返す

    auto texture = std::make_unique<Texture>();

    // texture->m_descに幅・高さ・Format・Mip情報を設定

    auto handle = AssetSystem::Register<Texture>(
        name, texture.release());

    if (!handle.IsValid())
        return {};

    // Graphics側がこの呼び出し中に一時画素をUpload Resourceへコピー
    // Upload失敗時はエラーログだけ出す
    m_graphics->UploadTexture(handle, image);

    return handle;
}
```

[Texture](./AssetType.md#texture)  
[DXTexture](./AssetType.md#dxtexture)

- `Texture`本体はHandleとDescのみで**画素データを所有しない**。`ScratchImage`はLoader内だけで生存する。
- `UploadTexture`は復帰までに`ScratchImage`の画素をGPU Upload Resourceへコピーし終える契約とする。Upload ResourceはGraphicsがGPU Fence完了まで保持する。
- `Mesh`も`Texture`もLoader自身がRegisterを呼び、`AssetHandle<T>`を返すため、`LoadImpl<T>`には型別分岐が要らない。
- MaterialShader Loaderは`.matsh`をParser/Compile/Reflectionして共通Property LayoutとPassを構築し、`Register<MaterialShader>`で登録する。PSOはGraphics側が描画時に必要に応じて作成する。
- Material LoaderはShader名を解決して`Load<MaterialShader>()`・`Get<MaterialShader>()`で実体ポインタを取得し、型付き値とbyteキャッシュを初期化して`Register<Material>`で登録する。
- Loaderが依存Assetを読む場合は`AssetSystem::Load<U>()`を再帰的に呼ぶ。循環依存は初期対象では扱わない。

### AssetAPI

**役割**

Engineが値所有するGame.dll向けBoundary API。AssetSystemへの非所有ポインタを保持する。

```cpp
namespace Hestia
{
    class ENGINE_API AssetAPI
    {
    public:
        void Initialize(AssetSystem* system);

        template<class T>
        AssetHandle<T> Load(std::string_view name);

        template<class T>
        T* Get(AssetHandle<T> handle);

        template<class T>
        AssetHandle<T> Register(std::string_view name, T* asset);

    private:
        AssetSystem* m_system = nullptr;
    };
}
```

[AssetSystem](#assetsystem)  
[GameEngineAPI](./Engine.md#gameengineapi)

公開操作形状のみ定義する。DLL境界でのテンプレート公開・Export・所有権移転ABIは後で検討する。

### HestiaGame::Assets

**役割**

Game Script側のstatic Facade。AssetAPIを経由してEngine側のAssetを参照する。

```cpp
namespace HestiaGame
{
    class Assets
    {
    public:
        template<class T>
        static Hestia::AssetHandle<T> Load(std::string_view name);

        template<class T>
        static T* Get(Hestia::AssetHandle<T> handle);

        template<class T>
        static Hestia::AssetHandle<T> Register(
            std::string_view name, T* asset);

    private:
        static void Bind(Hestia::AssetAPI* api);
        static void Unbind();

        inline static Hestia::AssetAPI* s_api = nullptr;
    };
}
```

[AssetAPI](#assetapi)  
[GameEngineAPI](./Engine.md#gameengineapi)

`Get`はAssetStorageからの借用ポインタで、呼び出し側は`delete`しない。初期はEngine終了までアドレスを維持する。

## Graphicsとの接続

`DXMesh`と`DXTexture`の詳細は[AssetType](./AssetType.md#dxmesh)にまとめる。
CPUとGPUの所有者・同一Handleによる対応関係は維持する。

```text
AssetSystem
├─ AssetDB (Name → AssetInfo)
├─ tuple<AssetStorage<Mesh>, AssetStorage<Texture>,
│        AssetStorage<MaterialShader>, AssetStorage<Material>>
└─ tuple<AssetLoader<Mesh>, AssetLoader<Texture>,
         AssetLoader<MaterialShader>, AssetLoader<Material>>

MeshのGPU Upload:
  MeshLoader → AssetSystem::Register<Mesh>
             → RegisterImpl<Mesh> → Graphics::UploadMesh

TextureのGPU Upload:
  TextureLoader (ScratchImageを一時所有)
     → AssetSystem::Register<Texture> (CPUメタ情報登録)
     → Graphics::UploadTexture(handle, image)

MaterialShader Loader → Register<MaterialShader>()
Material Loader → Load/Get<MaterialShader>()
                → Material::m_shader (非所有ポインタ)
                → Register<Material>()

GraphicsSystem
├─ GPUResourceStorage
│  ├─ DXMesh    [AssetHandle<Mesh>]
│  └─ DXTexture [AssetHandle<Texture>]
└─ PSOCache
   └─ (ShaderHandle, ShaderRevision, PassIndex) → PSO

Graphics内部RenderTarget
└─ DXRenderTexture m_sceneColor / m_gbuffer... (別管理)
```

[AssetType](./AssetType.md#gpuresourcestorageとの対応)  
[MaterialShader](./MaterialShader.md#materialshader)  
[Material](./Material.md#material)

- MeshとTextureは同じIndex/GenerationでGPU Resourceを解決。別のGraphics専用Handle変換表は初期に持たない。
- GPUの実体・Descriptor・Upload Resource・FenceはGraphics側が管理する。
- CPU AssetとGPU Resourceの論理寿命を合わせる。初期は個別Unloadを持たない。
- GPU Upload失敗でCPU Assetだけ登録された状態になっても、まずは**エラーログ出力のみ**。Rollbackや再試行機構は設けない。
- MaterialShaderのBytecode・Pass・共通LayoutとMaterialの型付き値・byteキャッシュはCPU Asset側。GraphicsはPSOとFrame CB領域の所有者。

## 未決定・後回し

- GPU資源作成・Upload APIの確定、失敗後の再試行・Ready状態・非同期ロード。
- Textureの手動生成用画素入力API、RenderTextureの公開やAsset化。
- Asset名の全型共通一意性検査と、YAML上の名前を経由したRegisterの区別。
- DB手動Reload、個別Unload、Generation更新、CPU AssetとGPU Resourceの再生成。
- Meshの追加Stream・動的更新、Textureの圧縮形式・Mip生成。
- Loaderの依存Asset再帰ロードにおける循環依存検出。
- MaterialShaderのReload / Material値移行 / 逆索引・PSO Cache無効化。
- DLL境界のテンプレート関数・ABI・所有権譲渡の具体的な実装。

## 参照元

- [AssetType（このリポジトリ）](./AssetType.md)
- [MaterialShader（このリポジトリ）](./MaterialShader.md)
- [Material（このリポジトリ）](./Material.md)
- [HestiaDesign / AssetType](https://github.com/YokoyamaHaruki64/HestiaDesign/blob/main/Asset/AssetType.md)
- [HestiaDesign / AssetDB](https://github.com/YokoyamaHaruki64/HestiaDesign/blob/main/Asset/AssetDB.md)
- [HestiaDesign / AssetManagement](https://github.com/YokoyamaHaruki64/HestiaDesign/blob/main/Asset/AssetManagement.md)
