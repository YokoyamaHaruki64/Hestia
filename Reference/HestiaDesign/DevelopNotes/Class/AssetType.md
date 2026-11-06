# Asset Type

Hestiaの初期Asset型であるMeshとTexture、およびGraphics内部のDX12リソース対応を定義するクラスモック。
CPU AssetとGPU Resourceの所有者を分け、RenderTexture・Material・Shader等は初期対象に含めない。
構造のベースはHestiaDesignの`Asset/AssetType.md`。実装詳細ではなく保持するデータと寿命を示す。

## Mesh

**役割**

CPU側の静的Meshデータを保持する。頂点属性をStreamごとに分け、Indicesで頂点を参照する。
AssetStorageが`unique_ptr<Mesh>`で所有し、GraphicsがGPU上の`DXMesh`を別途所有する。

```cpp
namespace Hestia
{
    struct Mesh
    {
        AssetHandle<Mesh> m_handle;

        std::vector<PositionVertex> m_positions;
        std::vector<NormalTangentVertex> m_normalTangents;
        std::vector<PrimaryTexCoordVertex> m_primaryTexCoords;
        std::vector<ColorVertex> m_colors;

        std::vector<uint32_t> m_indices;
    };
}
```

[AssetHandle<T>](./AssetSystem.md#assethandlet)  
[PositionVertex / NormalTangentVertex / PrimaryTexCoordVertex / ColorVertex](#meshvertexstream)  
[DXMesh](#dxmesh)  
[AssetSystem](./AssetSystem.md#assetsystem)

- Positionは必須。Index列も初期実装では必須とする。
- その他のStreamは省略可能。存在するStreamの頂点数はPositionと一致する。
- 欠損するNormal/Tangent、UV、Colorに対してMesh単位の追加GPU Bufferは作らず、描画側の共通デフォルトBufferを使用する。
- 初期はロード後のCPU Mesh編集によるGPU再アップロードを実装しない。
- `m_handle`はAssetStorageへの登録時に発行・設定する。登録前は無効値。

### MeshVertexStream

**役割**

Shaderの入力Semanticと対応する、CPU上の頂点Streamの要素型。
NormalとTangentは同一Streamに配置し、InputLayoutではそれぞれのOffsetを指定する。

```cpp
namespace Hestia
{
    struct PositionVertex
    {
        std::array<float, 3> m_position;
    };

    struct NormalTangentVertex
    {
        std::array<float, 3> m_normal;
        std::array<float, 4> m_tangent;
    };

    struct PrimaryTexCoordVertex
    {
        std::array<float, 2> m_uv;
    };

    struct ColorVertex
    {
        std::array<float, 4> m_color;
    };

    enum class VertexStream
    {
        Position,
        NormalTangent,
        PrimaryTexCoord,
        Color,
        Count
    };
}
```

[Mesh](#mesh)  
[DXMesh](#dxmesh)

`VertexStream`はCPUとGraphicsで共有するStream区分。GPUのSlot・Format・Offset・Strideは共通規則で対応させる。
Skinning用Streamや第二UV等は必要になった段階で追加する。行列やカラーのC++型はエンジン共通Math型へ置換してよい。

## Texture

**役割**

GPU上のTexture Assetを参照するためのHandleとメタ情報を保持する。
**画素データは保持しない。** 画像をデコードした一時データは`AssetLoader<Texture>`が保持し、GraphicsへのUpload要求に使用する。

```cpp
namespace Hestia
{
    struct Texture
    {
        AssetHandle<Texture> m_handle;
        TextureDesc m_desc;
    };
}
```

[TextureDesc](#texturedesc)  
[AssetHandle<T>](./AssetSystem.md#assethandlet)  
[DXTexture](#dxtexture)  
[AssetLoader<T>](./AssetSystem.md#assetloadert)

- TextureのCPU実体はAssetStorageが所有する。GPUの`ID3D12Resource`とDescriptorは含めない。
- `m_handle`はAssetStorage登録時に発行・設定する。`m_desc`はLoaderがデコード結果から設定する。
- 元の画素データを後から参照・編集・保存するAPIは初期には用意しない。
- 初期はファイルロード由来TextureのみをGPU生成対象とする。画素データを渡さない手動`Register<Texture>`ではGPU Textureを作成できない。

### TextureDesc

**役割**

Textureの寸法・Format・Mip数をCPU / Graphics間で共有する。

```cpp
namespace Hestia
{
    enum class TextureFormat
    {
        RGBA8Unorm,
        RGBA8Srgb
        // 必要な形式を追加
    };

    struct TextureDesc
    {
        uint32_t m_width = 0;
        uint32_t m_height = 0;
        uint32_t m_mipLevels = 1;
        TextureFormat m_format = TextureFormat::RGBA8Unorm;
    };
}
```

[Texture](#texture)

`TextureFormat`の値は**初期候補**。DXGI_FORMATをそのまま公開せず、Graphics内部で必要なDXGI形式へ対応付ける。
当面は2D Textureを想定し、Array / Cube / 3D等の情報は対応が必要になった時に拡張する。

## DXMesh

**役割**

MeshのGPU Vertex / Index Bufferと、Draw時に必要なBufferViewを保持するGraphics内部型。
Graphics側のGPUResourceStorageが`AssetHandle<Mesh>`のIndexとGenerationに対応させて保持する。

```cpp
namespace Hestia
{
    struct DXVertexBuffer
    {
        ComPtr<ID3D12Resource> m_resource;
        D3D12_VERTEX_BUFFER_VIEW m_view{};
    };

    struct DXMesh
    {
        std::array<
            DXVertexBuffer,
            static_cast<size_t>(VertexStream::Count)
        > m_streams;

        ComPtr<ID3D12Resource> m_indexBuffer;
        D3D12_INDEX_BUFFER_VIEW m_indexView{};

        uint32_t m_vertexCount = 0;
        uint32_t m_indexCount = 0;
    };
}
```

[Mesh](#mesh)  
[MeshVertexStream](#meshvertexstream)

- 存在しないStreamの`m_resource`は空でよい。描画時にGraphicsが共通の既定値BufferをBindする。
- Vertex/Index ResourceはDefault Heap上に置く想定。Upload ResourceはGraphics側のUpload管理機構がFence完了まで保持する。
- GPUResourceStorage側のSlotでGenerationを管理する場合、DXMesh本体に`AssetHandle<Mesh>`を重複保持しない。

## DXTexture

**役割**

GPU Texture Resourceと、Master SRV Heap上のDescriptor Indexを保持するGraphics内部型。

```cpp
namespace Hestia
{
    struct DXTexture
    {
        ComPtr<ID3D12Resource> m_resource;
        uint32_t m_masterSRVIndex = UINT32_MAX;
    };
}
```

[Texture](#texture)

- `m_masterSRVIndex`はMaster Heap内のSRV位置。Frame単位のVisible HeapへのCopyなどはGraphicsが行う。
- GraphicsのGPUResourceStorageは`AssetHandle<Texture>`のIndexとGenerationを用いてDXTextureを解決する。
- Decoderが保持する画素データとGPU Upload Resourceは別の寿命を持つ。
- 初期のTextureUploadは、`UploadTexture`が呼び出しから戻るまでに画素データをUpload Resourceへコピーする想定。Decoder側のデータはその後破棄でき、Upload ResourceはGPU Fence完了まで保持する。

## GPUResourceStorageとの対応

**役割**

CPU AssetとGPU Resourceは同一Handleで対応付ける。ただし所有者は別であり、AssetDBやAssetStorageはDX12実体を所有しない。

```text
AssetSystem
└─ AssetStorage<Mesh> / AssetStorage<Texture>
   └─ unique_ptr<Mesh> / unique_ptr<Texture>
           │
           └─ AssetHandle<T> (Index + Generation)
                         │
                         ▼
GraphicsSystem / DX12BackEnd
└─ GPUResourceStorage
   ├─ DXMesh    [AssetHandle<Mesh>]
   └─ DXTexture [AssetHandle<Texture>]

Graphics内部のRenderTarget
└─ DXRenderTexture m_sceneColor / m_gbuffer...（Assetとは別）
```

[AssetSystem](./AssetSystem.md#assetsystem)

- GPU Resourceの**論理寿命**はCPU Assetに合わせる。初期は個別Unloadせず、AssetSystem終了までAssetを保持する。
- GPU Resourceの解放・Fence管理はGraphics側に任せる。
- GBuffer / SceneColor / 一時RenderTargetは初期にはAsset化せず、Graphics内部の`DXRenderTexture`などで別管理する。
- `TextureHandle`や`MeshHandle`への追加変換表は当面作らない。

## 保留事項

- 頂点属性の全種類、Stream Format、共通デフォルトBufferの容量管理。
- CPU側Mesh編集と再アップロード、Vertex/Indexの動的更新。
- TextureのFormat拡張、Mip生成、DDS等のファイル形式と圧縮Texture対応。
- Textureの手動生成用ピクセル入力API、GPUだけの再生成。
- GPU Uploadの失敗処理・非同期完了通知・個別Unload。
- RenderTextureのGame向け公開とAsset化。

## 参照元

- [HestiaDesign / AssetType](https://github.com/YokoyamaHaruki64/HestiaDesign/blob/main/Asset/AssetType.md)
- [HestiaDesign / AssetManagement](https://github.com/YokoyamaHaruki64/HestiaDesign/blob/main/Asset/AssetManagement.md)
