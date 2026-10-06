# Asset Type: Mesh / Texture
[[AssetManagement]] の型別Assetのうち、まずMeshとTextureのメモリ上の形とGPUへの受け渡しを整理する草案。ファイル形式や全Asset共通の管理方法はここでは決めない。頂点Streamの考え方は [[MaterialShaderDSL]] と既存のRefarenceFrameWorkの実装を踏まえる。


## Public Facadeと内部Subsystem
Game.dllはAssetsの型付きC++ APIを使い、その実装をGameAPI.libへリンクする。Engine::AssetDBは登録Map / Cache / Loader接続を持つSubsystem実体であり、中継Serviceではない。Engine内部はその実体を直接使う。Public AssetはSTLを含んでもよく、所有入力の限定変換だけをAPI lib内へ隠す。GetはEngine所有の借用Pointer、Register(T&&)は入力を消費しEngineが登録結果を所有する。具体的な内部View / コピー案とLifetimeは[[FacadeBoundarySketch]]・[[AssetSceneSketch]]を参照する。

## 基本方針
- `AssetDB` のCacheが `Mesh` / `Texture` を所有する。各Assetは自身の `AssetHandle<T>` を持ち、`Assets::Get(handle)` はCache内の実体への貸出参照またはポインタを返す。
- ゲーム側に公開するAsset型には、D3D12のResourceやDescriptorを含めない。対応する `DXMesh` / `DXTexture` はGraphics側の `GPUResourceStorage` がHandleをキーに保持する。
- 初期構成ではファイルのロードとGPUアップロードを一組の処理にする。ゲーム側がロード後に明示的な `Upload` を呼ぶ必要はない。
- 描画時は `RenderItem` にHandleを入れ、NativeRenderがGraphics内部でGPU実体を解決する。ゲーム側はSRV番号やDX12 Resourceを扱わない。
- `RenderTexture` は描画結果を保持する別の資源とし、Texture AssetのCacheへ混ぜない。CPUから画素を頻繁に更新する機能は、実際に必要になったときに設計する。

## MeshとDXMesh
`Mesh` はCPU側の形状データ。頂点属性は共有の `VertexAttribute`（名称は仮）で区分し、属性群ごとに独立したStreamとして保持する。型としては `Position`、`NormalTangent`、`PrimaryTexCoord`、`Color` など全Streamの配列とIndex列を持ち、元データにないStreamは空のままにする。空のStream用にMeshごとのGPU Bufferは作らない。

```cpp
// 構造と責務を示すスケッチ。属性の全種類・型名は未確定。
struct Mesh
{
    AssetHandle<Mesh> handle;
    std::vector<PositionVertex> positions;
    std::vector<NormalTangentVertex> normalTangents;
    std::vector<PrimaryTexCoordVertex> primaryTexCoords;
    // 他の定義済みStreamも同様に保持し、存在しなければ空にする。
    std::vector<uint32_t> indices;
};
```

`VertexAttribute` はユーザー側とGraphics側で共用するStream区分のenum。`NormalTangent`のように複数のShader入力を一つのStreamへまとめる区分もある。Shaderの頂点入力Semanticから必要なStreamとSlotを解決し、`DXMesh` の対応するVertex Bufferを結び付ける。Format、Stream内Offset、Stride、Slotは共通の規則に従う。CPUとGPUで任意に再配置する構成は初期案に含めない。既存の `RefarenceFrameWork/include/Graphics/GraphicInc.h` は考え方の参考であり、DX11の型をそのまま使う意味ではない。

`DXMesh` はアップロード済みの頂点・Index Bufferと、それらの描画用Viewを持つGraphics内部の型。Mesh内で空のStreamに対応するBufferは空欄にする。Resourceの生成、更新、破棄とGPU完了待ちはNativeRender側が担当する。

```cpp
// Graphics内部の概念スケッチ。具体的な所有型や配列長は未確定。
struct DXMesh
{
    AssetHandle<Mesh> handle;
    VertexBufferResource streams[kVertexStreamCount];
    IndexBufferResource indices;
    uint32_t indexCount;
};
```

```text
AssetDB Cache: Mesh[handle]
    Position / NormalTangent / UV / Index ...
               ↓ ロードに続けてGraphicsへアップロード要求
GPUResourceStorage: DXMesh[handle]
    属性ごとのVertex Buffer + View / Index Buffer + View
               ↓
NativeRender: Shaderが要求するStreamをSlotへ結び付けて描画
```

Shaderが要求するStreamがMeshにない場合は、Graphicsが属性ごとに持つ共通の既定値Bufferを結び付ける。たとえばNormal/Tangentには既定方向、UVにはゼロ、Colorには白を使える。一方、Positionがなければ形状を決められないため、ロードまたは登録時にエラーとする。非空Streamの頂点数はPositionと一致させ、Indexはその範囲内とする。

RefarenceFrameWorkも欠損属性のBufferをMeshごとには作らず、Renderer共通のBufferを使用する。ただし参考実装では各属性の既定値を約100万頂点分ずつ事前確保している。DX12版ではこの確保量を踏襲せず、必要な描画頂点数に応じた容量と増加方法を実装時に決める。

初期構成ではロード後のMesh編集と再アップロードを必須にしない。動的Meshが必要になった場合は、変更したStreamをGraphicsへ渡す操作と、アップロード中のCPUデータ・旧GPU Bufferの寿命を別途定める。[[RenderingArchitecture]] の動的Mesh更新方針との接続もそのときに詰める。

## 手動作成したAssetの登録
ファイル由来のAssetに加え、ゲーム側で組み立てたMeshやTextureを名前付きでAssetDBへ登録できるようにする。YAMLのパスやLoaderを経由せず、登録したAssetをCacheが所有し、ファイルロード後と同じGPUアップロード経路へ渡す。Asset内のHandleは登録時にAssetDBが設定する。

```cpp
Mesh mesh;
mesh.positions = /* 頂点列 */;
mesh.indices = /* Index列 */;

auto handle = Assets::Register<Mesh>("GeneratedQuad", std::move(mesh));
// 登録後はAssets::Get(handle)で借り、描画にはhandleを渡す。
```

手動登録名にも通常の小文字化と全Type共通の一意性検査を適用し、既存名との衝突はエラーとする。`Load<T>(name)` でも同じHandleを返せるようにする。この登録は実行中のメモリ上だけの操作で、YAMLへ自動追記しない。登録後に呼び出し側のMesh/Textureが破棄されてもよいよう、AssetDBまたはアップロード要求が必要なデータを所有する。移動とコピーの選択、アップロード完了までの保持方法はAPI詳細設計で決める。

## TextureとDXTexture
`Texture` はHandleと幅・高さ・Format・Mipmap等の情報を持つ。ロード時には画素データの先頭とサイズ、Row Pitchなど、アップロードに必要なViewも持たせる。実データはAssetDBのCacheまたはLoaderが所有し、Viewだけで所有権を表さない。Graphicsは `Texture` からHandle・情報・画素データを取り出してアップロードできる。

```cpp
// 概念スケッチ。PixelDataViewの所有者と具体的なFormat表現は未確定。
struct Texture
{
    AssetHandle<Texture> handle;
    TextureDesc desc;        // 幅・高さ・Format・Mipmap等
    PixelDataView pixelData; // 先頭・サイズ・Row Pitch等。ロード中のみ有効
};
```

`DXTexture` はD3D12 Resource、SRV Descriptorの位置など、GPU上で利用する情報を保持する。Graphics内部の `GPUResourceStorage` がHandleから解決する。Texture Assetのロードでは、デコード後にGraphicsへアップロードを要求し、GPUで利用可能になった時点でAssetを `Ready` とする。`Ready` の判定と失敗通知の具体的な仕組みは [[AssetManagement]] の非同期ロード設計で決める。

```cpp
// Graphics内部の概念スケッチ。Descriptorの管理方法は未確定。
struct DXTexture
{
    AssetHandle<Texture> handle;
    D3D12Resource resource;
    DescriptorIndex srv;
};
```

画素データはGraphicsがアップロード用Resourceへ取り込み終えれば破棄できる。その後も `Texture` のHandleとDescはCacheに残る。描画スレッドが後から取り込む場合は、取り込み完了まで元の画素データを保持するか、要求側へ所有権を移す必要がある。取り込み後はUpload ResourceをGPU Fence完了まで保持する。これは二つの異なる寿命であり、後者にはDX12_Initの `waitingResource` の考え方を利用できる。

```text
ファイル → デコード → Textureの情報と画素View → Graphicsへアップロード要求
                                             ↓ 描画スレッドが画素を取り込むまで元データを保持
                                  Upload Resource → DXTexture
                                             ↓ GPU Fence完了までUpload Resourceを保持
AssetDB Cache: Texture[handle] のHandle・Descを維持
GPUResourceStorage: DXTexture[handle] を維持
```

## AssetDBとGraphicsの境界
`Assets::Get<T>(handle)` の貸出参照は、AssetDBがそのAssetを保持している間だけ有効とする。長期参照にはHandleを使う。Cacheの退避・Reload・同時アクセス時の具体的な寿命保証は未確定で、貸出ポインタをFramePacketや非同期要求へそのまま入れない。

GraphicsはHandleをキーにGPUResourceStorageを参照する。GPUResourceStorageはゲーム向けの公開窓口にはせず、AssetDBにもDX12 ResourceやSRV番号を公開しない。LoaderからGraphicsへのアップロード要求に使うAPIと、描画スレッドでの登録・参照の同期方法は詳細設計で決める。

関連: [[AssetManagement]] / [[AssetDB]] / [[RenderingArchitecture]] / [[MaterialShaderDSL]]
