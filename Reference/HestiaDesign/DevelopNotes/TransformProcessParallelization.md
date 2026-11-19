# TransformProcess チャンク並列化設計案

> **状態:** 検討中・未実装（2026-11-11）

## 背景・目的

HestiaEngineのArchetype + Chunk型ECSにおいて、`TransformProcess`をチャンク単位で並列化する。
10万EntityでTransform更新とRenderItem更新を合わせて約6ms（個別内訳は未計測）。
現行のチャンクごとの`Process(entities, count, components...)`形式は維持する。

目標は**全チャンクを1度だけ並列走査し、Dirty起点判定→サブツリーのWorld行列更新→Dirty解除を完結**すること。
起点抽出用の別走査やDirty解除用の再走査は設けない。

## 前提条件

- 親がDirtyになると、**全子孫へDirtyを伝播済み**の状態で並列更新を開始する。
- 更新中は新たなDirty設定、親子付替え、Entity削除、チャンク移動をしない。
- 各チャンクの走査は1 Workerだけが担当する。同一Entityを複数の走査Jobへ割り当てない。
- 異なるチャンク間に親子関係があってもよい。
- 親のWorld行列を確定した後に子のWorld行列を更新する。
- 後段のProcessはTransform更新Jobの完了を待ってから実行する。

## Transformの状態

```cpp
enum class TransformState : uint8_t
{
    Clean,
    Dirty,
    Updating
};
```

通常の変更受付では`Clean → Dirty`、更新Process中は`Dirty → Updating → Clean`と遷移する。
`Dirty`と`Updating`はいずれも**他のWorkerから見て「親が処理を引き受けるのでスキップ」**する状態。

## 更新起点の判定

各チャンクの並列走査で、まず**親の状態**を確認する。

1. 親が`Dirty`または`Updating`なら、その子はスキップ（親側の再帰処理に任せる）。
2. 親が`Clean`、または親がない場合、自身が`Dirty`なら更新起点とする。
3. 起点を`Updating`に変更し、**親→子**の順でWorld行列を更新する。
4. 再帰処理の帰りがけに**子→親**の順で`Clean`へ戻す。

```text
開始            更新中                完了
A Dirty         A Updating           A Clean
└ B Dirty       └ B Updating         └ B Clean
  └ C Dirty       └ C Updating         └ C Clean

Cleanへの遷移順: C → B → A
```

親が`Dirty`でも`Updating`でも子側の走査はスキップする。
親が`Clean`を公開する時点では子孫も更新・解除済みなので、親のDirty解除による二重更新を防げる。
同一Entityの起点判定はチャンク担当Workerに一意であり、再帰更新はその起点の担当Workerだけが行うため、CASでの起点獲得は不要という設計。

## コードイメージ（擬似コード）

実際のコンポーネント取得API・Transform階層の保持形式・命名に合わせて調整する。

```cpp
TransformState GetState(TransformCache& cache)
{
    return std::atomic_ref(cache.m_state)
        .load(std::memory_order_acquire);
}

void SetUpdating(TransformCache& cache)
{
    std::atomic_ref(cache.m_state)
        .store(TransformState::Updating, std::memory_order_relaxed);
}

void SetClean(TransformCache& cache)
{
    std::atomic_ref(cache.m_state)
        .store(TransformState::Clean, std::memory_order_release);
}

void UpdateSubtree(Entity entity)
{
    auto& cache = GetTransformCache(entity);
    // ここに入る時点でcacheはUpdating
    UpdateWorldMatrix(entity);

    for (Entity child : GetChildren(entity))
    {
        auto& childCache = GetTransformCache(child);
        SetUpdating(childCache);
        UpdateSubtree(child);
    }

    // すべての子孫を更新し終えてから解除
    SetClean(cache);
}

void ProcessChunk(Entity* entities, uint32_t count)
{
    for (uint32_t i = 0; i < count; ++i)
    {
        Entity entity = entities[i];
        auto& cache = GetTransformCache(entity);
        Entity parent = GetParent(entity);

        if (IsValid(parent) &&
            GetState(GetTransformCache(parent)) != TransformState::Clean)
            continue;

        if (GetState(cache) != TransformState::Dirty)
            continue;

        SetUpdating(cache);
        UpdateSubtree(entity);
    }
}
```

チャンク内のコンポーネント配列が利用できる場合は`GetTransformCache`等の検索を避ける。

## Atomicの扱い

- `TransformCache::m_state`は普通のenumとして保持し、並列アクセス時にC++20の`std::atomic_ref`で読み書きする。ECSのSwapRemove等でコンポーネントをコピー・移動できる構造を維持する。
- 参照が生存する間、同じ状態変数へ非Atomicアクセスを混在させない。
- `std::atomic_ref<T>::required_alignment`を満たす必要がある。
- `SetClean`は`release`、親の状態参照は`acquire`とし、親の`Clean`を観測した側に子孫の更新完了を公開する。
- `Dirty → Updating`は、開始前にDirty伝播が完了している前提なら`relaxed` Storeで足りる想定。
- 通常のenumに対する読み書きは、操作が一瞬でも並行するとC++のデータ競合（未定義動作）になる。mutexの「書き込み側だけロック」も不十分。
- Atomic Load/Storeのみを利用し、mutexとCASは使用しない。ただし、`atomic_ref`が実環境でロックフリーかは実測・確認する。

## Renderer / RenderSystemとの関係

```text
TransformProcess (ECS / チャンク並列)
    World行列更新・Dirty解除、変更通知を保持
        ↓
MeshRendererProcess (ECS)
    Mesh / WorldMatrix / WorldAABBなどをRenderItemへ提出
        ↓
RenderSystem (非ECS)
    Viewごとのカリング・ソート・描画
```

- Meshには共有できるLocal AABB等の形状固有Boundsを保持する。
- RenderItemは毎フレーム破棄されるため、World AABBをキャッシュするならECSのRenderer側に保持する案。
- DirtyはTransformProcess中に解除する。後段が更新を知るには`worldChanged`またはWorld行列のVersion等を別途持つ。
- **World AABB更新**と**描画Viewごとのカリング判定**は別責務とする。

## 計測・検証項目

- 約6msの内訳をTransformProcess / MeshRendererProcess / RenderItem提出で分けて計測。
- 1 / 2 / 4 / 8 Worker、Dirty率0 / 10 / 100%、階層なし / 深い階層を比較。
- チャンク数・Job粒度・再帰による非連続メモリアクセスの影響を確認。
- 異なるチャンクにまたがる親子を重点的にストレステストし、二重更新や更新漏れがないか検証する。
- 1つの巨大サブツリーは1 Workerで再帰処理するため並列性能は伸びにくい。
- Atomic方式と2フェーズ方式の実測比較は未実施。採否は性能・検証結果で決める。
