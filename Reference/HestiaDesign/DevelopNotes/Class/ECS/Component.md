# Component
---
Engine内部のArchetype ECSで保持するComponentのデータ型を示すクラス設計草案。
型登録の方針は会話で合意した内容とし、Transform／Renderer／MeshRendererのメンバは説明用の例として記載する。

## 基本方針
---
- Componentは通常のC++のデータ型とする。共通基底クラスや仮想関数は要求しない。
- 処理はSystem側に置き、ComponentにはEntityごとの状態を保持する。
- Componentの型ID、サイズ、アラインメント、型操作は[ComponentRegistry](./ComponentRegistry.md#componentregistry)へ登録する。
- 管理上の所有元はECSSystemとし、Componentの実データはその管理下のChunkが保持する。Componentが持つコンテナ等は各インスタンスの構築・破棄に従う。
- RendererとMeshRendererを別のComponentとして組み合わせる例を示す。最終的な分割は未確定。
- 全Archetypeには[Name・Tag・Layer](./Entity.md#全entity共通のcomponent)を含める。以下の描画Componentは、それらの基本Componentに追加して組み合わせる例。

## Transform
---
**役割**

Entityの位置・回転・拡縮を保持する。更新処理や描画要求の生成はSystem側で行う。

```cpp
namespace Hestia
{
    struct Transform
    {
        Vector3 m_position = { 0.0f, 0.0f, 0.0f };
        Quaternion m_rotation = Quaternion::Identity();
        Vector3 m_scale = { 1.0f, 1.0f, 1.0f };
    };
}
```

[ComponentRegistry](./ComponentRegistry.md#componentregistry)

Vector3／Quaternionと`Identity()`は数学型の説明用表記。数学APIをここで定義するものではない。

**未確定**：親子関係、Local／Worldの区別、行列キャッシュ、更新検出。これらを先行してメンバへ追加しない。

## Renderer
---
**役割**

Entityの描画に共通する表示状態を保持する例。描画実行クラスやGraphicsSystemそのものではない。

```cpp
namespace Hestia
{
    struct Renderer
    {
        bool m_visible = true;
    };
}
```

[MeshRenderer](#meshrenderer)  
[GraphicsSystem](../GraphicsSystem.md#graphicssystem)

**未確定**：共通Componentとして独立させるか、MeshRendererへ統合するか。複数の描画Componentに同じ表示状態が必要なら分離する理由になる。当面Mesh描画だけなら、表示フラグをMeshRendererにまとめる案もある。

## MeshRenderer
---
**役割**

EntityをMeshとして描画するためのMesh／Material参照を保持する例。
AssetやGPU Resourceの実体は所有せず、AssetHandleで対応付ける。

```cpp
namespace Hestia
{
    struct MeshRenderer
    {
        AssetHandle<Mesh> m_mesh;
        std::vector<AssetHandle<Material>> m_materials;
    };
}
```

[AssetHandle\<T>](../AssetSystem.md#assethandlet)  
[Mesh](../AssetType.md#mesh)  
[Material](../Material.md#material)  
[Renderer](#renderer)  
[ComponentInfo](./ComponentRegistry.md#componentinfo)

MeshRendererは`m_materials`のコンテナ領域を所有する。Mesh／Materialの実体はAsset側が保持し、GPU ResourceはGraphics側が保持する。HandleをコピーしてもAsset実体を複製する意味にはならない。

コンストラクタ、デストラクタ、Move／Copyが通常のメンバ操作で足りる場合、MeshRenderer側で自作する必要はない。Registryはその型操作を使う。

**未確定**：Materialを単一にするか複数にするか、SubMeshとMaterialの対応方法。`m_materials`は非自明なコンテナを持つComponentの例であり、MeshのSubMesh構造や描画契約を確定するものではない。

## 型登録とEntityへの組み合わせ
---
型はComponentRegistryへ登録する。登録自体はComponentインスタンスの生成ではない。

```cpp
registry.Register<Hestia::Transform>("Hestia::Transform");
registry.Register<Hestia::Renderer>("Hestia::Renderer");
registry.Register<Hestia::MeshRenderer>("Hestia::MeshRenderer");
```

[ComponentRegistry](./ComponentRegistry.md#componentregistry)  
[Transform](#transform)  
[Renderer](#renderer)  
[MeshRenderer](#meshrenderer)

この分割例では、Meshを表示するEntityがTransform／Renderer／MeshRendererを持ち、描画用Systemが3型をQueryする。RendererとMeshRendererは継承関係にせず、Entity上で組み合わせる。

Archetype／Chunkの保存構造は[Archetype](./Archetype.md)と[Chunk](./Chunk.md)を参照。Entityへの追加・削除API、SystemからGraphicsへの描画データ受け渡しは後続で定める。
