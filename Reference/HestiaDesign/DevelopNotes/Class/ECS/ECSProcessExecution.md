# ECS Processの生成と実行

処理関数を指定して[ECSProcess](./ECSProcess.md#ecsprocess)を作り、Runから元の関数を呼ぶまでのデータの流れを説明する。
利用側のCreateを入口に、保持する関数ポインタ、Traits、引数展開を順に示す。コードは仕組みを説明するスケッチであり、Productionコードではない。

## 基本方針

```text
関数を指定してCreate
  → 条件と共通入口を持つインスタンスを作る
  → 対象Archetypeと列番号を対応付ける
  → RunでChunkごとに共通入口を呼ぶ
  → 共通入口から処理本体を呼ぶ
```

クラスの責務・所有関係・未確定事項は[ECSProcess](./ECSProcess.md)を参照する。本資料には生成と実行の内部処理をまとめる。

## 利用側の入口

```cpp
void SubmitMesh(
    size_t count,
    const Entity* entities,
    const Transform* transforms,
    const Renderer* renderers,
    const MeshRenderer* meshRenderers);

auto process = ECSProcess::Create<&SubmitMesh>(registry, phase);
```

[ECSProcess](./ECSProcess.md#ecsprocess)  
[ComponentRegistry](./ComponentRegistry.md#componentregistry)  
[Transform / Renderer / MeshRenderer](./Component.md)  
[Entity](./Entity.md#entity)

`&SubmitMesh`は関数のアドレス。この値をTemplate引数の`< >`へ指定する。Registryとphaseは通常の関数引数の`( )`へ渡す。TraitsやInvokeを利用側で個別に操作する必要はない。

## Createの内部

```cpp
template<auto Function>
ECSProcess ECSProcess::Create(
    const ComponentRegistry& registry,
    ECSProcessPhase phase)
{
    using FunctionType = decltype(Function);
    using Traits = ECSProcessFunctionTraits<FunctionType>;

    ECSProcess process;

    process.m_phase = phase;
    process.m_components = Traits::BuildComponents(registry);
    process.m_invoke = ProcessKey<Function>;

    return process;
}
```

[ECSProcess](./ECSProcess.md#ecsprocess)  
[ComponentRegistry](./ComponentRegistry.md#componentregistry)  
[関数型を取り出すTraits](#関数型からcomponent型の並びを取り出す)

上から順に次の処理を表す。

1. FunctionにはTemplateへ指定した`&SubmitMesh`という値が入る。
2. FunctionTypeで、その関数ポインタの型を取得する。
3. Traitsで、その関数型に対応するComponent型の並びを取り出す。
4. BuildComponentsでComponent IDとRead／Writeを作り、m_componentsへ保存する。
5. ProcessKeyからFunction専用の共通入口のアドレスを取得し、m_invokeへ保存する。
6. フェーズとこれらの情報を持つProcessを返す。対象Archetypeの表はまだ空。

```text
作られたprocess
├─ m_phase       = 指定したフェーズ
├─ m_components  = SubmitMeshが要求するComponent条件
├─ m_invoke      → Traits::Invoke<&SubmitMesh>という共通入口
└─ m_archetypes  = まだ空
```

元のSubmitMeshのポインタを別メンバへ保存しない。共通入口側で呼ぶ関数がコンパイル時に決まっている。引数型が同じ関数でも、指定する関数の値が異なれば共通入口のTemplate特殊化は異なる。

`::template`の構文の読み方は、データの流れを確認した後の[Createに出てくるTemplate構文](#createに出てくるtemplate構文)にまとめる。

## 対象Archetypeの対応付け

Createの引数にはArchetype群を渡していない。[ECSWorld::RegisterProcess](./ECSWorld.md#archetypeとprocessの接続)が既存ArchetypeをProcessのRegisterArchetypeへ提示し、条件に合う場合に列番号を対応付ける。例えば次の対応を保持する。

| Component引数 | 引数番号 | このArchetypeの列番号 |
| --- | --- | --- |
| Transform | 0 | 2 |
| Renderer | 1 | 0 |
| MeshRenderer | 2 | 1 |

```text
m_archetypesの1要素
├─ m_archetype     → 条件に合うArchetype（非所有）
└─ m_columnIndices = [2, 0, 1]
```

全ArchetypeにはName・Tag・Layerの列も存在するが、対象表の列番号は要求したComponentだけに対応する。型と引数順はコンパイル時に分かるが、実際の列番号は対象ArchetypeのLayoutに対して実行時に解決する。Archetype生成時にもWorldが対応付けを更新する。

## Runの流れ

```text
ECSSystemが対象フェーズのECSProcess::Runを呼ぶ
  ↓
対象表からArchetypeを順次取得
  ↓
ArchetypeのChunkを順次取得
  ↓
Component列番号を使って必要な配列を取得
  ↓
Chunkの有効Entity数と配列を元の処理関数へ渡す
  ↓
次のChunk、次のArchetypeへ進む
```

保存構造との接続を示す疑似コードは次のとおり。

```cpp
for (const auto& target : m_archetypes)
{
    auto& archetype = *target.m_archetype;

    for (size_t i = 0; i < archetype.GetChunkCount(); ++i)
    {
        auto& chunk = archetype.GetChunk(i);

        if (chunk.GetEntityCount() == 0)
        {
            continue;
        }

        m_invoke(chunk, target.m_columnIndices);
    }
}
```

[Archetype](./Archetype.md#archetype)  
[Chunk](./Chunk.md#chunk)

渡す件数はChunkの有効Entity数とし、容量を渡さない。共通入口でComponent配列を取得してから、元の処理関数を1回呼ぶ。Archetypeとの条件一致は対象表を作るときに済ませ、Runでは毎Chunkの完全な条件比較を行わない。


## 共通入口が行う変換

この関数に対応する共通入口を手書きで表すと、次の形になる。既存Chunkモックの`GetColumn`を使った説明用スケッチであり、取得APIの最終形を決めるものではない。

```cpp
void InvokeSubmitMesh(
    Chunk& chunk,
    std::span<const size_t> columnIndices)
{
    auto* transforms = static_cast<const Transform*>(
        chunk.GetColumn(columnIndices[0]));

    auto* renderers = static_cast<const Renderer*>(
        chunk.GetColumn(columnIndices[1]));

    auto* meshRenderers = static_cast<const MeshRenderer*>(
        chunk.GetColumn(columnIndices[2]));

    SubmitMesh(
        chunk.GetEntityCount(),
        chunk.GetEntities().data(),
        transforms,
        renderers,
        meshRenderers);
}
```

[Chunk](./Chunk.md#chunk)  
[Transform / Renderer / MeshRenderer](./Component.md)

ECSProcessには`InvokeSubmitMesh`のアドレスを保持する。元の`SubmitMesh`はこの入口から直接呼ぶため、元の関数ポインタを別の実行時Contextに格納する必要はない。

```text
ECSProcess::Run
  └─ m_invoke（共通型の関数ポインタ）
       └─ InvokeSubmitMesh（処理関数ごとの入口）
            ├─ Chunkから3列を取得して型を合わせる
            └─ SubmitMesh(count, Entity列, Transform列, Renderer列, MeshRenderer列)
```


この手書き例と同じ引数の組み立てを、次のTemplateで表す。Component配列は各Chunkの呼び出し時点で取得し、入口には保持し続けない。

## Templateによる共通入口の具体化

まず、関数の値と型を分ける。

| 表記 | SubmitMeshを指定した場合 |
| --- | --- |
| Function | `&SubmitMesh`という値 |
| decltype(Function) | `void (*)(size_t, const Entity*, const Transform*, const Renderer*, const MeshRenderer*)`という型 |

decltypeだけでは引数型を列挙しない。Createから利用するTraitsが、取得した関数型を分解する。

### 関数型からComponent型の並びを取り出す

```cpp
template<class FunctionType>
struct ECSProcessFunctionTraits;

template<class... Components>
struct ECSProcessFunctionTraits<void (*)(size_t, const Entity*, Components*...)>
{
    // この中でComponents...を使用できる
};
```

最初の宣言が基本形で、その下が「先頭はsize_t、次はconst Entity*、残りはComponentへのポインタ」という関数型に対応する部分特殊化。実際の関数型と照合することで、コンパイラが次の対応を見つける。

```text
実際の型：void (*)(size_t, const Entity*, const Transform*, const Renderer*, const MeshRenderer*)
照合する形：void (*)(size_t, const Entity*, Components*...)

Components... = const Transform, const Renderer, const MeshRenderer
```

`Components...`は型の並びを表すTemplateパラメータパック。ポインタの`*`は照合する形に書かれているため、取り出す型は`const Transform`などとなり、constは残る。先頭のsize_tと次のconst Entity*は共通引数であり、Component型の並びに含めない。Entity列はGetEntitiesから渡し、Component IDで検索しない。

この例の対応範囲は`void(size_t, const Entity*, Component*...)`。メンバ関数ポインタ、参照引数、noexcept付き関数などには別の対応が必要になるため、このスケッチから対応済みとは扱わない。オーバーロードされた関数は、渡す関数ポインタの型を明確にする必要がある。

### Component条件を同じ型の並びから作る

各型からconstを外した型でRegistryのIDを取得し、元の型のconstからアクセス方法を決める。

| Componentsの要素 | IDを取得する型 | アクセス |
| --- | --- | --- |
| `const Transform` | `Transform` | Read |
| `const Renderer` | `Renderer` | Read |
| `const MeshRenderer` | `MeshRenderer` | Read |
| `Transform`の場合 | `Transform` | Write |

例えばTraits内の補助関数を次のように分ければ、型1つの処理と、型の並びの展開を区別できる。

```cpp
template<class T>
static ECSProcessComponent MakeComponent(const ComponentRegistry& registry)
{
    using Component = std::remove_const_t<T>;

    constexpr auto access = std::is_const_v<T>
        ? ECSComponentAccess::Read
        : ECSComponentAccess::Write;

    return { registry.GetId<Component>(), access };
}

static std::vector<ECSProcessComponent> BuildComponents(
    const ComponentRegistry& registry)
{
    return { MakeComponent<Components>(registry)... };
}
```

[ECSProcessComponent](./ECSProcess.md#ecsprocesscomponent)  
[ComponentRegistry](./ComponentRegistry.md#componentregistry)

末尾の`...`は、Componentsの要素ごとに式を展開する指定。この例のreturnは、次と同じ意味になる。

```cpp
return {
    MakeComponent<const Transform>(registry),
    MakeComponent<const Renderer>(registry),
    MakeComponent<const MeshRenderer>(registry)
};
```

RegistryへのID検索と条件の保存は、Createを実行したときに行う。Templateで型が分かることと、Registryが持つIDを実行時に取得することは別の段階。

### 型の並びと引数番号を対応させる

型が3つなら、必要な引数番号は`0, 1, 2`。標準の`std::index_sequence_for<Components...>`で、この番号列を表す型を作れる。

```text
Components... = const Transform, const Renderer, const MeshRenderer
I...          = 0,               1,              2
```

このIはComponent引数の番号であり、Archetype内の列番号そのものではない。例えばArchetypeの列順が異なる場合、対象表は次のようになる。

```text
Component引数    I      columnIndices[I]（このArchetypeでの列番号）
Transform       0      2
Renderer        1      0
MeshRenderer    2      1
```

引数番号はコンパイル時に分かるが、実際の列番号は対象Archetypeとの対応付けで決める。`std::index_sequence`は番号を型として表す仕組みで、実行時の番号配列を確保するものではない。

### 共通入口のTemplateを書く

Traitsの`Components...`と、番号列`I...`を同じ式で展開する。以下はTraitsの部分特殊化に置く呼び出し処理のスケッチ。

```cpp
template<class... Components>
struct ECSProcessFunctionTraits<void (*)(size_t, const Entity*, Components*...)>
{
public:
    template<auto Function>
    static void Invoke(
        Chunk& chunk,
        std::span<const size_t> columnIndices)
    {
        InvokeImpl<Function>(
            chunk,
            columnIndices,
            std::index_sequence_for<Components...>{});
    }

private:
    template<auto Function, size_t... I>
    static void InvokeImpl(
        Chunk& chunk,
        std::span<const size_t> columnIndices,
        std::index_sequence<I...>)
    {
        Function(
            chunk.GetEntityCount(),
            chunk.GetEntities().data(),
            static_cast<Components*>(
                chunk.GetColumn(columnIndices[I]))...);
    }
};
```

[Chunk](./Chunk.md#chunk)

この定義は先ほどのTraitsの具体化例であり、空の説明用定義と重ねて定義するものではない。条件の生成も同じTraitsに置く場合は、前節の`MakeComponent`と`BuildComponents`をこの型の中へ加える。

`InvokeImpl`の最後の引数に番号列を渡すことで、その型から`I...`を取り出す。展開する式にはComponentsとIの両方が含まれ、同じ位置の要素が対になる。SubmitMeshの場合、関数呼び出しは実質的に次の形になる。

```cpp
SubmitMesh(
    chunk.GetEntityCount(),
    chunk.GetEntities().data(),
    static_cast<const Transform*>(chunk.GetColumn(columnIndices[0])),
    static_cast<const Renderer*>(chunk.GetColumn(columnIndices[1])),
    static_cast<const MeshRenderer*>(chunk.GetColumn(columnIndices[2])));
```

`Invoke`の引数は常にChunkと列番号のspanだけなので、元のComponent引数の個数・型が異なっても、アドレスは共通のECSProcessInvoke型へ格納できる。型付き関数への変換はこの入口に集め、Runには持ち込まない。


## ProcessKey

Traitsへのアクセスは、共通の変数Templateへまとめる。次の宣言はTraitsを参照できる場所へ置き、利用する具体化時には対応するInvokeの定義が見えている前提とする。

```cpp
template<auto Function>
inline constexpr ECSProcessInvoke ProcessKey =
    &ECSProcessFunctionTraits<decltype(Function)>::template Invoke<Function>;
```

CreateはProcessKeyをm_invokeへ保存する。AddProcess／RemoveProcessも同じキーを渡し、Worldが登録済みProcessをMapから取得する。

```cpp
template<auto Function>
void ECSSystem::AddProcess(EntityHandle handle)
{
    s_instance->m_world.AddProcess(handle, ProcessKey<Function>);
}
```

[ECSSystem](./ECSSystem.md#ecssystem)  
[ECSWorldの登録表](./ECSWorld.md#関数をキーにしたprocessの参照)

ProcessKeyはコンパイル時に決まる関数アドレス。参照のたびにInvokeを生成したり、実行時に関数の型を解析したりしない。登録・追加・削除のたびにTraitsの構文を重複させないための名前とする。

## Createに出てくるTemplate構文

ProcessKeyがまとめている式を、Traitsという型の別名を使って書くと次の形になる。以下は構文を説明するための展開例。

```cpp
process.m_invoke = &Traits::template Invoke<Function>;
```

| 部分 | 意味 |
| --- | --- |
| `Traits::` | Traits内の名前を指定する |
| `template` | 続くInvokeをメンバTemplateとして解釈するようコンパイラに伝える |
| `Invoke<Function>` | 呼ぶ関数をFunctionに決めた共通入口を指定する |
| `&` | その共通入口のアドレスを取得する |

TraitsはCreateのTemplate引数によって変わる型なので、`::template`で続く名前がメンバTemplateだと明示する。このキーワード自体が関数を生成・実行するわけではない。「TraitsのInvokeにFunctionを指定し、そのアドレスを保存する」と読む。

関数ポインタ型の別名[ECSProcessInvoke](./ECSProcess.md#ecsprocessinvoke)へ、この共通入口のアドレスを格納する。元のComponent引数の構成が違っても、入口の引数形式はChunkと列番号のspanで共通となる。

## コンパイル・生成・実行の区別

| 段階 | 決まるもの・行うこと |
| --- | --- |
| コンパイル | 元の関数、Component型と引数番号の並び、対応する共通入口 |
| Create実行 | RegistryからIDを取得し、フェーズ・条件・入口ポインタを持つProcessを返す |
| 対象の登録 | 条件に合うArchetypeと、その列番号を対応付ける |
| Run | 現在のChunkから配列と有効数を取得し、入口を通して元の関数を呼ぶ |

コンパイラがTemplateの具体化に必要なコードを用意する。外部ツールによるソース生成や、実行時の関数型解析を行う仕組みではない。

型変換の正しさは、条件と共通入口が同じ型の並びを使い、対象表の列番号もその順序に対応することを前提とする。static_cast自体が実行時の型一致を確認するわけではない。
