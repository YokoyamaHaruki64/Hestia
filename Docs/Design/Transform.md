# Transform
---
EntityのLocal TRS、計算済みWorld行列、親子関係を分けて保持する設計を定義する。
Engine内部の更新は `TransformProcess`、Game向け操作は `HestiaGame::Transform` が担う。以下の宣言は設計の骨格を示す。

[現行の実装仕様](../Implementation/ECSProcess/Transform.md)

## TransformProcess
---
**役割**

Local TRSと親のWorld行列からWorld行列を計算し、更新が必要なEntityのCacheを確定する。
Engine内のProcessとGame向けFacadeで、座標変換・dirty伝播・Cache更新の処理を共有する。

```cpp
namespace Hestia
{
    class TransformProcess
    {
    public:
        static void Run(size_t count, const Entity* entities, Transform* transforms,
            TransformCache* caches, const TransformHierarchy* hierarchies);

        static bool SetLocalTRS(Transform& transform, const TransformHierarchy& hierarchy,
            const Vector3& position, const Quaternion& rotation, const Vector3& scale);
    };
}
```

[Transform](#transform) ・ [TransformCache](#transformcache) ・ [TransformHierarchy](#transformhierarchy)
[ECSProcess](../../Reference/HestiaDesign/DevelopNotes/Class/ECS/ECSProcess.md#ecsprocess) ・ [Entity](../../Reference/HestiaDesign/DevelopNotes/Class/ECS/Entity.md#entity)

**所有・参照**

Engineが所有する `ECSSystem` のWorldで実行し、Chunk内のComponent列をその実行中だけ借用する。Transform専用のSystemや別のEntity格納領域は持たない。 [ECSSystemの設計](../../Reference/HestiaDesign/DevelopNotes/Class/ECS/ECSSystem.md#ecssystem)

**更新順**

Local変更時に自身のLocal行列と、自身・子孫のWorld行列を無効化する。親だけを変えた場合、Local TRSが同じならLocal行列を再利用する。World取得と `PreRender` は共通の更新経路を使い、親のWorldを確定してから子を計算する。Entityのactiveは行列の有効性に影響させず、破棄予約済みのEntityは更新対象から外す。

**更新の入口**

Game側はFacadeを経由し、Engine内で列を借用しているProcessは `SetLocalTRS` を使う。Local TRSの変更と子孫へのdirty通知を一つの操作にして、Cacheの更新漏れを防ぐ。

### Transform
---
**役割**

親に対する位置・回転・拡縮の正本と、Local行列および再計算の状態を保持する。

```cpp
namespace Hestia
{
    struct Transform
    {
        Vector3 m_localPosition = Vector3::Zero();
        Quaternion m_localRotation = Quaternion::Identity();
        Vector3 m_localScale = Vector3::One();
        Matrix4x4_SIMD m_localMatrix = Matrix4x4_SIMD::Identity();
        bool m_localDirty = false;
        bool m_worldDirty = false;
    };
}
```

[TransformProcess](#transformprocess) ・ [数学型の演算規約](../Implementation/Common/README.md)

**座標系**

行ベクトルを使い、Local行列を `S × R × T`、World行列を `Local × ParentWorld` とする。親なしの場合はLocalがWorldになる。行列は公開APIを含めて `Matrix4x4_SIMD` を使う。World操作も最終的にLocal TRSを更新する。

### TransformCache
---
**役割**

計算済みWorld行列を、描画収集側がChunk内で連続して読める独立した列にする。

```cpp
namespace Hestia
{
    struct TransformCache
    {
        Matrix4x4_SIMD m_worldMatrix = Matrix4x4_SIMD::Identity();
    };
}
```

[Transform](#transform) ・ [TransformProcess](#transformprocess)

World行列だけを格納する。将来の描画収集Processは、Transform更新後にこの列を読み取る構成を想定する。

### TransformHierarchy
---
**役割**

親への参照と直下の子一覧を保持し、親から子へのdirty伝播と破棄予約に使う。

```cpp
namespace Hestia
{
    struct TransformHierarchy
    {
        EntityHandle m_parent;
        std::vector<EntityHandle> m_children;
    };
}
```

[EntityHandle](../../Reference/HestiaDesign/DevelopNotes/Class/ECS/Entity.md#entityhandle) ・ [HestiaGame::Transform](#hestiagametransform)

**所有・参照**

ChunkはComponentを所有し、親子は互いのEntityをHandleで非所有参照する。親と子の関係は `SetParent` で一緒に更新する。空の親はrootを表し、自身や子孫を親にする指定は拒否する。

**寿命**

初版のEntityには `Transform`、`TransformCache`、`TransformHierarchy` を必須として付与する。親の破棄要求で子孫も予約し、EndFrameで行を解放する前に親の子一覧から参照を外す。

### HestiaGame::Transform
---
**役割**

GameからEntityのLocal・Worldの位置、回転、拡縮とWorld行列、親を操作する非所有Facade。
操作はEngine.dllに直接公開し、各呼び出しでHandleを現在のEntityへ解決する。

```cpp
namespace Hestia
{
    enum class ParentMode { KeepWorld, KeepLocal };
}

namespace HestiaGame
{
    class Transform
    {
    public:
        Transform() = default;
        explicit Transform(Hestia::EntityHandle entity) noexcept;
        HESTIA_ENGINE_API bool IsValid() const noexcept;

        HESTIA_ENGINE_API Vector3 GetLocalPosition() const;
        HESTIA_ENGINE_API Quaternion GetLocalRotation() const;
        HESTIA_ENGINE_API Vector3 GetLocalScale() const;
        HESTIA_ENGINE_API bool SetLocalPosition(const Vector3& position);
        HESTIA_ENGINE_API bool SetLocalRotation(const Quaternion& rotation);
        HESTIA_ENGINE_API bool SetLocalScale(const Vector3& scale);

        HESTIA_ENGINE_API Vector3 GetWorldPosition() const;
        HESTIA_ENGINE_API Quaternion GetWorldRotation() const;
        HESTIA_ENGINE_API Vector3 GetWorldScale() const;
        HESTIA_ENGINE_API Matrix4x4_SIMD GetWorldMatrix() const;
        HESTIA_ENGINE_API bool SetWorldPosition(const Vector3& position);
        HESTIA_ENGINE_API bool SetWorldRotation(const Quaternion& rotation);
        HESTIA_ENGINE_API bool SetWorldScale(const Vector3& scale);
        HESTIA_ENGINE_API bool SetWorldMatrix(const Matrix4x4_SIMD& matrix);

        HESTIA_ENGINE_API Transform GetParent() const;
        HESTIA_ENGINE_API bool SetParent(const Transform& parent,
            Hestia::ParentMode mode = Hestia::ParentMode::KeepWorld);

    private:
        Hestia::EntityHandle m_entity;
    };
}
```

[TransformProcess](#transformprocess) ・ [TransformHierarchy](#transformhierarchy) ・ [EntityHandle](../../Reference/HestiaDesign/DevelopNotes/Class/ECS/Entity.md#entityhandle)

**DLL境界**

FacadeはHandleを値保持し、Getterは値を返す。各メソッドの実装をEngine.dll側に置き、呼び出し時にECSへ接続する。これは従来のBoundary APIと `Bind` を使う設計に対するTransformでの試行であり、他のSubsystemへの採用は別途判断する。 [従来のSubsystem／Facade設計](../../Reference/HestiaDesign/DevelopNotes/SubsystemApiFacadeDesign.md)

**親の変更**

`KeepLocal` はLocal TRSを維持する。既定の `KeepWorld` は付け替え前のWorld位置、親子Quaternionの合成回転、基底長による近似拡縮を、付け替え後に表現できる範囲で維持する。せん断を含む任意の行列はLocal TRSで表せる形へ近似する。新しい親のゼロ拡縮で逆算できない成分は元のLocal値を保持し、World側の潰れを許容する。

**失敗時の契約**

`IsValid()` で操作対象の生存状態を確認できる。Setterは対象や入力が無効ならログと `false` を返す。無効なGetterはログを出し、assert有効時は契約違反として停止する。assert無効時はIdentity相当の値または空の親を返す。
