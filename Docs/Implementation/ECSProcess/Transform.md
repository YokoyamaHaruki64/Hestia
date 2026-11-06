# Transformの実装仕様
---
現行コードから確認した、必須Componentの初期値と寿命、World行列の更新順、DLL公開Facadeの動作を示す。
宣言は説明に必要な部分を抜粋し、各型のリンク先を実装の確認元とする。

[Transformの設計](../../Design/Transform.md) ・ [実装仕様書の索引](../README.md)

## TransformProcess
---
**役割**

ECSSystemの単一Worldで、借用したComponent列またはHandleから解決したComponentを使い、dirty伝播とWorld行列の計算を行う。

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

[宣言](../../../Engine/Include/Transform/TransformProcess.h) ・ [実装](../../../Engine/Source/Transform/TransformProcess.cpp) ・ [Transform](#transform) ・ [TransformCache](#transformcache) ・ [TransformHierarchy](#transformhierarchy)

`ECSSystem::RegisterECS` はName／Tag／LayerとTransform系3型を登録し、`TransformProcess::Run` を `PreRender` に登録する。通常フレームの実行条件を満たし、ECSが初期化済みの場合、`Engine::FrameExecute` は `LateUpdate` の後、既存の描画提出より前にこのphaseを呼ぶ。 [ECSSystem.cpp](../../../Engine/Source/ECS/ECSSystem.cpp) ・ [Engine.cpp](../../../Engine/Source/Engine.cpp)

### dirtyの伝播と確定
---

1. Local Setterと内部の `SetLocalTRS` は、TRS一式の有限性を確認し、Quaternionを正規化してからLocal値を保存する。ゼロQuaternionはIdentityになる。自身の `m_localDirty` を立て、`MarkWorldDirty` で自身と子孫を無効化する。 [TransformProcess.cpp](../../../Engine/Source/Transform/TransformProcess.cpp) ・ [Quaternion.h](../../../Common/Include/Math/Quaternion.h)
2. `MarkWorldDirty` は既にWorld dirtyの枝で再帰を打ち切る。新たにdirtyにした場合は子Handleを解決し、取得できない子や破棄予約済みの子を除いて伝播する。 [TransformProcess.cpp](../../../Engine/Source/Transform/TransformProcess.cpp)
3. `Run` はChunkの列を走査する。Entityのactiveは判定に使わず、`IsDestroy()` がtrueの行だけを除外する。World Getterも `EnsureWorld` を経由し、`Run` と同じ `UpdateWorld` を使う。 [TransformProcess.cpp](../../../Engine/Source/Transform/TransformProcess.cpp) ・ [TransformFacade.cpp](../../../Engine/Source/Transform/TransformFacade.cpp)
4. `UpdateWorld` はWorldがcleanなら親を辿らずに戻る。dirtyなら親を先に更新し、自身のLocalがdirtyの場合だけLocal行列を作り直す。Worldを書き込んでから `m_worldDirty` を落とすため、Chunkや行の並び順に依存しない。 [TransformProcess.cpp](../../../Engine/Source/Transform/TransformProcess.cpp)

### Transform
---
**役割**

Local TRSの正本とLocal行列を格納し、LocalとWorldの更新要否を別々に記録する。

```cpp
struct Transform
{
    Vector3 m_localPosition = Vector3::Zero();
    Quaternion m_localRotation = Quaternion::Identity();
    Vector3 m_localScale = Vector3::One();
    Matrix4x4_SIMD m_localMatrix = Matrix4x4_SIMD::Identity();
    bool m_localDirty = false;
    bool m_worldDirty = false;
};
```

[TransformData.h](../../../Engine/Include/Transform/TransformData.h) ・ [TransformProcess](#transformprocess)

**合成順**

行ベクトル規約で `Local = S × R × T`、`World = Local × ParentWorld` とする。親なしのWorldはLocalと同じ。World位置はWorld行列の第4行の先頭3成分から取得する。 [TransformProcess.cpp](../../../Engine/Source/Transform/TransformProcess.cpp) ・ [TransformFacade.cpp](../../../Engine/Source/Transform/TransformFacade.cpp)

**構造変更**

生成時に必須ID一覧から3型を既定構築する。同じ一覧が `RemoveProcess` の保護にも使われる。Archetype移行では共通列としてMoveされるため、保持したFacadeはHandleから移行後の行を解決する。Engine内で `SetLocalTRS` に渡す参照は同一Entityの列を借用し、構造変更をまたいで保持しない。 [ECSWorld.cpp](../../../Engine/Source/ECS/ECSWorld.cpp) ・ [TransformProcess.h](../../../Engine/Include/Transform/TransformProcess.h)

### TransformCache
---
**役割**

計算済みWorld行列を独立したSoA列に格納する。

```cpp
struct TransformCache
{
    Matrix4x4_SIMD m_worldMatrix = Matrix4x4_SIMD::Identity();
};
```

[TransformData.h](../../../Engine/Include/Transform/TransformData.h) ・ [Transform](#transform)

生成時はIdentityで、LocalとWorldのdirtyはどちらもfalse。生の列を読むだけでは遅延更新を行わないため、後続Processが最新値を必要とするときは、Transformの `PreRender` 更新が終わった後に読む。現行の描画提出は `Engine::FrameExecute` 内で作ったデバッグ用のWorld行列を使う。Cache列を要求するProcessの読み取りはテストに実例がある。 [Engine.cpp](../../../Engine/Source/Engine.cpp) ・ [TransformTests.cpp](../../../Test/Hestia_Tests/Source/TransformTests.cpp)

### TransformHierarchy
---
**役割**

親Handleと直下の子Handleを保持し、World更新時の親参照と子孫への通知・破棄予約に使う。

```cpp
struct TransformHierarchy
{
    EntityHandle m_parent;
    std::vector<EntityHandle> m_children;
};
```

[TransformData.h](../../../Engine/Include/Transform/TransformData.h) ・ [EntityHandle](../../../Engine/Include/ECS/Entity.h) ・ [HestiaGame::Transform](#hestiagametransform)

**破棄予約**

`DestroyEntity` は生存中の対象から未予約の子孫を集め、破棄キューの容量を確保してから全件に予約bitを立てる。予約直後から対象と子孫はFacadeの操作対象外になり、`Entity::IsActive()` はfalseを返す。内部ECSの `GetEntity` と `GetComponent` は、構造変更の適用中を除き、予約済みでも実体の解放までは取得できる。生成待ちのHandleに対する破棄は生成取消として扱う。 [ECSWorld.cpp](../../../Engine/Source/ECS/ECSWorld.cpp) ・ [ECSWorld.h](../../../Engine/Include/ECS/ECSWorld.h) ・ [Entity.h](../../../Engine/Include/ECS/Entity.h) ・ [TransformProcess.cpp](../../../Engine/Source/Transform/TransformProcess.cpp)

**破棄の確定**

`EndFrame` の `FlushDestroy` は、破棄予定の行を解放する前に親の子一覧からHandleを外す。その後にComponentとEntityを破棄し、Swap RemoveとSlot返却を行う。先に参照を外すことで、破棄順や行移動に左右されない。 [ECSWorld.cpp](../../../Engine/Source/ECS/ECSWorld.cpp)

### HestiaGame::Transform
---
**役割**

EntityHandleを値で保持し、呼び出しのたびに操作対象を解決する。取得結果は値として返す。

```cpp
namespace HestiaGame
{
    class Transform
    {
    public:
        Transform() = default;
        explicit Transform(Hestia::EntityHandle entity) noexcept;
        HESTIA_ENGINE_API bool IsValid() const noexcept;
        HESTIA_ENGINE_API Matrix4x4_SIMD GetWorldMatrix() const;
        HESTIA_ENGINE_API bool SetWorldMatrix(const Matrix4x4_SIMD& matrix);
        HESTIA_ENGINE_API Transform GetParent() const;
        HESTIA_ENGINE_API bool SetParent(const Transform& parent,
            Hestia::ParentMode mode = Hestia::ParentMode::KeepWorld);

    private:
        Hestia::EntityHandle m_entity;
    };
}
```

[公開ヘッダー](../../../Engine/Include/Game/Transform.h) ・ [Facadeの実装](../../../Engine/Source/Transform/TransformFacade.cpp) ・ [TransformHierarchy](#transformhierarchy) ・ [ParentMode](#parentmode)

操作メソッドには個別に `HESTIA_ENGINE_API` が付く。通常のDLL構成ではビルド側がexport、利用側がimportとなり、Test構成ではこの装飾が空になる。Handleからの構築はその時点で生存を検査しない。Facadeは生成元ECSの寿命内で使う。 [Transform.h](../../../Engine/Include/Game/Transform.h) ・ [DllAPI.h](../../../Common/Include/DllAPI.h)

| 操作 | 読み書きする値と挙動 |
| --- | --- |
| `GetLocalPosition`／`SetLocalPosition` | `Vector3`。Local位置を読み書きし、回転と拡縮は保持する |
| `GetLocalRotation`／`SetLocalRotation` | `Quaternion`。Setterは正規化してから保存する |
| `GetLocalScale`／`SetLocalScale` | `Vector3`。有限な負値と0も受け付ける |
| `GetWorldPosition`／`SetWorldPosition` | `Vector3`。Getterは最新のWorld行列から読み、Setterは親の変換からLocal位置を求める |
| `GetWorldRotation`／`SetWorldRotation` | `Quaternion`。子から親の順にLocal回転を合成し、Setterでは親の合成回転の逆を適用してLocal回転を求める |
| `GetWorldScale`／`SetWorldScale` | `Vector3`。基底長に基づく符号付き近似。Setterは同じ倍率で逆算する |
| `GetWorldMatrix`／`SetWorldMatrix` | `Matrix4x4_SIMD`。Getterは更新済みのWorldを返し、Setterは入力行列をWorld TRSへ近似してLocalに反映する |
| `GetParent`／`SetParent` | `Transform`。親なしは空のFacade。Setterは親変更時に保持する座標系を指定できる |

**対象の有効性と失敗時**

`IsValid()` は、ECSが初期化済みで、Handleが生成済みのEntityへ解決でき、破棄予約されていない場合だけtrueを返す。activeは条件に含めず、ログやassertも行わない。無効なGetterはログを記録してassertし、assert無効時には位置0、回転Identity、拡縮1、行列Identityまたは空の親を返す。Setterの検証失敗はログとfalseで通知する。`SetParent` の子一覧の確保例外は呼び出し側へ伝播し得る。 [TransformFacade.cpp](../../../Engine/Source/Transform/TransformFacade.cpp) ・ [TransformProcess.cpp](../../../Engine/Source/Transform/TransformProcess.cpp)

#### WorldからLocalへの変換
---

- World位置の逆算では、親の線形部を `L`、World位置の要求値と現在のLocalを親で変換した位置の差を `r` とする。`d × (L × Lᵀ) = r × Lᵀ` の最小二乗補正 `d` を元のLocal位置に加える。対称行列をJacobi法で対角化し、固有値が最大固有値の `1e-12` 倍以下の方向を補正から外す。親の変換で失われた方向には旧Local値が残る。 [TransformProcess.cpp](../../../Engine/Source/Transform/TransformProcess.cpp)
- World回転は行列の分解値とは独立したQuaternion合成値。World回転のSetterは `要求World回転 × 親World回転の逆` を求め、Localに正規化して保存する。 [TransformProcess.cpp](../../../Engine/Source/Transform/TransformProcess.cpp) ・ [TransformFacade.cpp](../../../Engine/Source/Transform/TransformFacade.cpp) ・ [Quaternionの積](../../../Common/Include/Math/Quaternion.h)
- World拡縮は、Local回転の各基底に親Worldの線形部を適用した基底長から求める。対応するWorld回転の基底との内積が負なら倍率も負にする。Getterはこの倍率にLocal拡縮を掛け、Setterは要求値を倍率で割る。倍率がちょうど0の軸はLocal拡縮を維持する。 [TransformProcess.cpp](../../../Engine/Source/Transform/TransformProcess.cpp) ・ [TransformFacade.cpp](../../../Engine/Source/Transform/TransformFacade.cpp)
- World行列のSetterは、全16要素が有限で、最終列が `(0, 0, 0, 1)` から各成分 `1e-6` 以内の入力を受理する。基底を直交化して回転を求め、潰れた基底は現在のWorld回転などから補う。符号付き基底長と平行移動を合わせてWorld TRSへ近似し、親に対するLocalへ変換する。Cacheは確定したLocal TRSから作り直すため、せん断を含む入力行列と取得行列は一致しないことがある。 [TransformProcess.cpp](../../../Engine/Source/Transform/TransformProcess.cpp) ・ [TransformFacade.cpp](../../../Engine/Source/Transform/TransformFacade.cpp)

#### ParentMode
---
**役割**

親変更時に保持する座標系を指定する。

```cpp
namespace Hestia
{
    enum class ParentMode { KeepWorld, KeepLocal };
}
```

[ParentModeの宣言](../../../Engine/Include/Game/Transform.h) ・ [SetParentの実装](../../../Engine/Source/Transform/TransformFacade.cpp)

1. 対象と新しい親の生存状態、modeを確認する。空のFacadeは親解除として受理する。新しい親から祖先を辿って自身が現れる場合は循環として拒否する。同じ親への有効な指定はtrueを返して終了する。
2. `KeepWorld` の場合は旧階層でWorld位置、合成Quaternion、近似拡縮を取得する。新しい親の子一覧の容量を先に確保し、新しい親を基準としたLocal TRSの書き込みに成功してから親子リンクを更新する。逆算できない成分は旧Localを保持し、ゼロ拡縮によるWorld側の潰れを許容する。
3. `KeepLocal` はLocal TRSを維持する。どちらのmodeでも旧親の子一覧から自身を外し、新親の子一覧と自身の親Handleを更新して配下のWorldを無効化する。 `KeepWorld` が保持するのは上記のWorld TRSで、せん断を含むWorld行列全体の一致は保証しない。

### 検証記録
---

2026-11-05の導入時は、Transform関連の追加23件が成功した。全体は103件中98件成功で、既存のEngine初期化経路4件とLogの行数期待値1件が失敗した。Debug_Editor／Release_EditorのEngineビルドと外部consumerのDLLリンク・実行は成功し、Debugの無効GetによるassertとReleaseの既定値復帰を確認した。この記録は導入時のもので、本資料の追記時点の実機描画、GPU、Game.dllからの有効Entity操作の確認は含まない。 [Transformテスト](../../../Test/Hestia_Tests/Source/TransformTests.cpp) ・ [DLL consumer](../../../Test/Smoke/TransformFacadeConsumer.cpp)
