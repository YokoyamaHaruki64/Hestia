# ECSの基本設計

## 基本方針

- ECSの処理速度とOOPの開発イテレーションの回しやすさを両立する
- `NativeECS` と `ScriptSystem` を基盤とする
- `NativeECS`- Archetype ECS 詳細: [[#NativeECS]]
  - Transform更新やRendererの`RenderItem` / `FramePacket`生成など、Engine側の処理を行う
- `ScriptSystem` - 疑似SparseSet 詳細: [[#ScriptSystem]]
  - ゲームロジックを記述する。C#を想定しており、`ScriptStorage<T>`の形で保持、Script単位で連続処理を行う

## NativeECS

Archetype ECSをベースとしたNative側のECS基盤。
ComponentとSystemを分離し、Componentの組み合わせが同一のEntityを同じArchetypeへ配置することで、System実行時のメモリアクセス効率を高める。
主に以下のようなEngine内部の大量処理を担当する。

- Transform更新
- Rendererからの描画コマンド生成
- Animation更新
- Physics連携
- Culling
- その他、多数のEntityに対して同一処理を行うEngine内部処理
ゲーム固有のロジックについては基本的に`ScriptSystem`側で処理する。

### データ構造
```
Archetype
├─ Signature
├─ ComponentLayout[]
│   ├─ TypeId
│   ├─ Offset
│   ├─ Size
│   └─ Alignment
└─ Chunk[]

Chunk
├─ Data*
├─ Count
└─ Capacity
```
`Archetype`がComponentの組み合わせとChunk内のメモリレイアウトを保持し、`Chunk`は実際のComponentデータを保持する。

### Entity
Entity自体はComponentを直接保持せず、Entityを識別するための軽量なHandleとして扱う。
```
struct Entity
{
    uint64_t value;

    uint32_t Index() const;
    uint32_t Generation() const;
};
```

Handleの論理的な配置は`[generation:32bit][index:32bit]`とする。これは2次元配列ではなく、1つの64bit値を分割して扱う構成である。
`index`はEntity Slotを参照するために使用し、`generation`は削除済みEntityへの古いHandleを検出するために使用する。
永続化やシーン上の識別子が必要な場合は、ランタイムHandleとは別のIDを用意する。

Entityの実データ位置は`EntityLocation`によって管理する。

```
struct EntityLocation
{
    Archetype* archetype;
    Chunk* chunk;
    uint32_t row;
};
```
これによりNativeECS内部では、Entityから現在所属しているChunkおよびRowを取得し、Componentへ直接アクセスできる。外部からの読み取り・変更は後述の専用APIを経由する。

Entityの有効性や状態はHandle本体ではなく、Entity Slot側で管理する。
```cpp
enum class EntityFlags : uint8_t
{
    None           = 0,
    Alive          = 1 << 0,
    Active         = 1 << 1,
    PendingDestroy = 1 << 2,
};

struct EntitySlot
{
    EntityLocation location;
    uint32_t generation;
    EntityFlags flags;
};
```

`PendingDestroy`になったEntityは以降の公開APIからの変更対象にせず、後続Systemの`Execute`対象からも除外する。削除要求そのものはCommand Bufferへ記録し、実体の破棄はフレーム末尾にまとめる。
```
Entity
  ↓
EntityLocation
  ↓
Archetype
  ↓
Chunk + Row
  ↓
Component
```
### Archetype
Entityが持つComponentの組み合わせごとにArchetypeを生成する。
イメージ：
```
struct ComponentLayout
{
    ComponentTypeId type;
    uint32_t offset;
    uint32_t size;
    uint32_t alignment;
};

class Archetype
{
    ArchetypeSignature signature;
    std::vector<ComponentLayout> layouts;
    std::vector<Chunk*> chunks;
};
```

例えば、
```
Entity A
├─ Transform
└─ MeshRenderer

Entity B
├─ Transform
└─ MeshRenderer

Archetype A
[Transform, MeshRenderer]
```
は同一のArchetypeへ所属する。
一方、
```
Entity C
├─ Transform
├─ MeshRenderer
└─ BoxCollider

Archetype A
[Transform, MeshRenderer]

Archetype B
[Transform, MeshRenderer, BoxCollider]
```
は異なるArchetypeへ所属する。

ArchetypeはComponent Typeの集合を`Signature`として識別する。
Componentの並び順には依存せず、同一のComponent集合であれば同一Archetypeとして扱う。
`ComponentLayout`は、そのArchetypeに所属する全Chunkで共通のメモリレイアウトとして使用する。

### Chunk
Archetype内部ではEntityを一定容量ごとのChunkへ分割して保持する。
```
struct Chunk
{
    std::byte* data;
    uint32_t count;
    uint32_t capacity;
    Archetype* archetype;
};
```
ChunkのByteサイズは固定とし、Archetypeが保持する各ComponentのSize / Alignmentから格納可能なEntity数を算出する。
そのため`Capacity`はArchetypeごとに異なる。

Chunk内部はSoA形式で、1つの連続したメモリ領域へComponentごとの配列を順番に配置する。
```
Chunk Memory:

[Entity x Capacity]
[padding]
[Transform x Capacity]
[padding]
[MeshRenderer x Capacity]
[padding]
[BoxCollider x Capacity]
```
物理メモリ上では一次元に連続して配置される。
```
低アドレス
↓
[EEEEEEEE...][padding][TTTTTTTT...][padding][MMMMMMMM...][padding][CCCCCCCC...]
                                                                                  ↑
                                                                             高アドレス
```

各Component Columnの開始位置は`Archetype`が保持する`ComponentLayout::offset`によって決定する。
```
Transform* transforms =
    reinterpret_cast<Transform*>(
        chunk.data + transformLayout.offset);
```

実装上はTemplateを使用し、
```
Transform* transforms = chunk.GetColumn<Transform>();
```
のように取得できるAPIを用意する。

同じRow番号のComponentは同じEntityに対応する。
例えば`row = 5`の場合、
```
Entity Column       : Entity[5]
Transform Column    : Transform[5]
MeshRenderer Column : MeshRenderer[5]
BoxCollider Column  : BoxCollider[5]
```
はすべて同一Entityのデータとなる。

### Component
Componentは原則としてデータのみを保持する。
```
struct Transform
{
    Vector3 position;
    Quaternion rotation;
    Vector3 scale;
};

struct MeshRenderer
{
    bool enable;
    MeshHandle mesh;
    MaterialHandle material;
};

struct BoxCollider
{
    Vector3 center;
    Vector3 size;
};
```
Componentに対する処理はComponent自身ではなくSystem側に記述する。
Component TypeはRegistryへ登録し、ECS内部では`ComponentTypeId`を用いて識別する。

Registryには最低限以下の情報を保持する。
```
ComponentTypeInfo
├─ TypeId
├─ Size
├─ Alignment
├─ ReflectionInfo
├─ Construct
├─ MoveConstruct
├─ CopyConstruct
└─ Destroy

```

Chunk間のMigrationや削除で安全に寿命を管理するため、型操作情報はRegistryに保持する。
Componentに仮想親クラスを持たせて各要素から`Move` / `Destroy`を仮想呼び出しする構成にはしない。Componentはデータ主体のままにし、型ごとに生成した関数ポインタを`ComponentTypeInfo`へ登録する。
```
struct ComponentTypeInfo
{
    ComponentTypeId typeId;
    uint32_t size;
    uint32_t alignment;
    ReflectionInfo reflection;

    void (*construct)(void* destination);
    void (*moveConstruct)(void* destination, void* source);
    void (*copyConstruct)(void* destination, const void* source);
    void (*destroy)(void* value);
};
```
`Transform`のようなTriviallyCopyableな型は高速なパスを利用できる。非自明な型は登録された操作関数を必ず通し、Migration・Swap Remove・Chunk破棄で同じ寿命規則を適用する。

### 公開API
OOP的なScriptやEngine外部からComponentを操作するため、ComponentおよびSystemとは別に公開APIを用意する。
公開APIはSystemの代替ではなく、Componentへのアクセスと状態変更を安全に行うための窓口とする。

基本的な責務は以下とする。
```
Public API
├─ Componentの値の取得
├─ Componentへの値の設定
├─ Validation
├─ Dirty通知
└─ Command / 更新要求の発行
```

大量のEntityを走査する処理や、依存するデータの再計算などは公開API内では行わず、System側でまとめて処理する。
```
Script / External API
        ↓
値変更
Validation
Dirty / Command発行
        ↓
System
        ↓
一括更新
```

例えばTransformのPositionを変更する場合、
```
void TransformAPI::SetPosition(
    Entity entity,
    const Vector3& position)
{
    auto* transform =
        world.TryGetComponentForWrite<Transform>(entity);

    if (!transform)
        return;

    transform->position = position;

    MarkTransformDirty(entity);
}
```
公開APIでは値の変更とDirty通知までを行い、World Transformの再計算やHierarchyへの変更伝播は`TransformSystem`側で行う。読み取りと変更は専用APIを分け、Native Componentの参照やポインタを外部へ返さない。
読み取りAPIは値または読み取り専用のViewを返し、変更APIはValidationとDirty通知を通して書き込む。
```
SetPosition()
    ↓
Transform変更
    ↓
Dirty通知
    ↓
TransformSystem
    ↓
World Transform更新
Hierarchy更新
```

Rendererについても同様に、公開APIでは状態変更のみを行う。
```
SetMesh()
SetMaterial()
SetEnabled()
SetCastShadow()
```

例えばMeshの変更によりBoundsや描画情報の再構築が必要な場合は、API内で即座に再計算するのではなくDirtyを通知する。
```
SetMesh()
    ↓
MeshHandle変更
    ↓
Renderer Dirty
    ↓
Render関連System
    ↓
Bounds更新
Render情報更新
RenderItem / FramePacket生成

```

公開API内で行う処理は、値のValidationなど即時実行が必要かつ軽量な処理に限定する。
例：
```
SetParent()
→ 自分自身をParentに指定していないか検証

SetMesh()
→ Handleの妥当性確認

SetScale()
→ 必要であれば値をClamp
```

Dirty状態についてはComponent内に直接Flagを保持するほか、
```
Dirty Flag
Dirty BitSet
Dirty Queue
Changed Entity List
```
など、対象Systemに適した管理方式を利用する。
特に変更対象が少ないSystemでは、全Componentを走査してDirty Flagを確認するのではなく、変更されたEntityのみを記録して処理する方式を検討する。

公開APIとSystemの数は対応しない。
例えば`TransformSystem`が1つであっても、公開APIとしては以下のような複数の操作を提供できる。
```
GetPosition
SetPosition
Translate

GetRotation
SetRotation
Rotate

GetScale
SetScale

GetParent
SetParent
```

ECS内部のComponent分割と、外部へ公開するAPIの単位も一致させる必要はない。
例えばRenderer内部が、
```
MeshRenderer
RenderBounds
ShadowState
```
など複数Componentへ分割されていたとしても、Script側には1つの`Renderer` APIとして公開できる。

```
ECS内部
→ メモリアクセスと処理効率を優先して分割

公開API
→ 利用者から見た機能単位で構成
```

### Reflection
Native ComponentにはReflection情報を持たせる。
ReflectionはECS内部だけでなく、以下の機能から共通利用する。
- Editor Inspector
- Serialization
- Scene / Prefab
- Debug表示
- Component複製
- C# Binding

通常のComponent GUIはReflection情報から自動生成する。
```
Reflection
   ↓
Default Inspector
```

特殊な表示や操作が必要な型についてのみCustom GUI Hookを使用する。
```
Reflection
   ↓
Custom Drawer
```

例：
- Transform
- AssetHandle
- Material
- Color
- その他専用UIを必要とする型

### System
Systemは特定のComponent集合を持つEntityに対して処理を実行する。

例：
```
TransformSystem

Write:
WorldTransform

Read:
LocalTransform
TransformParent
```

Systemが要求するComponent情報からQueryを生成し、条件を満たすArchetypeのみを処理する。
```
System
  ↓
Query
  ↓
Matching Archetype
  ↓
Chunk
  ↓
Component Column
  ↓
Execute
```
System内部ではEntityごとのComponent検索を行わず、Chunkから取得したComponent Columnを直接走査する。


### Query
QueryはSystemが必要とするComponentと、そのアクセス方法を表す。
初期実装では以下のみを扱う。
```
Read<T>
Write<T>
```
`Read<T>`はComponentを要求し、読み取りのみ行うことを表す。
`Write<T>`はComponentを要求し、書き込みを行うことを表す。

Systemが宣言するすべての`Read` / `Write` Componentを持つArchetypeが処理対象となる。
```
Write<WorldTransform>
Read<LocalTransform>
Read<TransformParent>
```

Componentの存在のみを条件とする`With`や、特定Componentを持つEntityを除外する`Exclude`等は初期設計では用意しない。
Systemの処理対象は、必要なComponentを適切に分離・付与することで表現する。
`Read` / `Write`情報はQueryだけでなく、将来的なSystem間の依存関係解析や並列実行判定にも利用する。
Archetype生成時またはQuery登録時にQueryとの対応関係をキャッシュし、毎フレームすべてのArchetypeと完全比較することは避ける。

### System記述支援
Engine内部では多数のSystemを実装するため、Query生成・Chunk取得・Row走査などの定型処理をMacroまたはTemplateによって簡略化する。

例：
```
HE_SYSTEM(TransformUpdateSystem,
    HE_WRITE(WorldTransform),
    HE_READ(LocalTransform))
{
    static constexpr SystemPhase Phase =
        SystemPhase::PostUpdate;

    void Execute(
        WorldTransform& world,
        const LocalTransform& local)
    {
        // 1 Entity分の処理
    }
};
```

内部では以下の処理を行う。
```
System登録
 ↓
Query生成
 ↓
Matching Archetype取得
 ↓
Chunk走査
 ↓
Component Column取得
 ↓
Row走査
 ↓
Execute(Component...)
```
System実装側では、可能な限り1 Entity分の処理のみを記述できる形を目指す。

### SystemPhase
Systemの大まかな実行タイミングは`SystemPhase`として定義する。

例：
```
enum class SystemPhase
{
    PreUpdate,
    Update,
    PostUpdate,

    PrePhysics,
    Physics,
    PostPhysics,

    PreRender,
    RenderSubmission
};
```

各Systemは所属するPhaseを自身で宣言する。
```
static constexpr SystemPhase Phase =
    SystemPhase::PostUpdate;
```

SchedulerはSystem登録時にこの情報を取得し、PhaseごとにSystemを分類する。
```
System登録
 ↓
SystemPhase取得
 ↓
Phaseごとに分類
 ↓
Execution Plan生成
```
毎フレームは生成済みのExecution Planに従ってSystemを実行する。
Scheduler側に個々のSystem名や実行順を直接ベタ書きすることは避ける。

### System間の実行順序
同一Phase内でも明確な実行順序が必要になる場合がある。
例えば、
```
AnimationSystem
       ↓
TransformUpdateSystem
       ↓
RenderSubmissionSystem
```
のような依存関係が存在する。

この場合は固定の数値Orderだけに依存せず、必要に応じてSystem間の相対依存を宣言できる構造を検討する。
```
HE_AFTER(AnimationSystem);
HE_BEFORE(RenderSubmissionSystem);
```

Schedulerはこれらの情報から依存関係を構築し、実行順序を決定する。
```
System Metadata
├─ Phase
├─ Read
├─ Write
├─ Before
└─ After
        ↓
Scheduler
        ↓
Execution Plan
```
明示的な依存がないSystemについては、登録順などに依存しない実行を基本とする。

### 並列実行
将来的にはSystemが宣言する`Read` / `Write`情報を利用して、System間のデータ依存を解析する。

例えば、
```
System A
Write<WorldTransform>

System B
Read<WorldTransform>
```
は同時実行できない。

一方、
```
System A
Write<WorldTransform>

System B
Write<AnimationPose>
```
のようにアクセス対象が競合せず、かつ明示的な実行依存も存在しない場合は並列実行の候補とする。

初期実装では直列実行を基本とし、SchedulerのMetadata設計だけ並列化へ拡張できる形にする。

### Componentの追加・削除
Archetype ECSではComponentの追加・削除によってEntityの所属Archetypeが変化する。
```
Before
Archetype
[Transform, MeshRenderer]

        ↓ Add<BoxCollider>()

After
Archetype
[Transform, MeshRenderer, BoxCollider]
```

Component追加時は以下のMigrationを行う。
1. 移動先Archetypeを取得または生成
2. 移動先Chunkに新しいRowを確保
3. 共通ComponentをCopyまたはMove
4. 新規Componentを初期化
5. 旧ChunkからEntityを削除
6. `EntityLocation`を更新
7. 移動元・移動先のComponent寿命処理を完了
Component削除時も同様に、削除後のComponent構成を持つArchetypeへMigrationする。

System実行中にChunkの構造を変更すると、Systemが保持しているColumnやRowが無効化される可能性がある。
そのためSystem実行中の、
```
AddComponent
RemoveComponent
CreateEntity
DestroyEntity
```
などの構造変更は即時反映せず、Command Buffer等へ記録して安全なタイミングで遅延反映する。

### Entity削除
Destroy要求は即時にStorageから取り除かず、対象Entityの`PendingDestroy`を立ててCommand Bufferへ記録する。
フレーム末尾のFlushでは、次の順序で処理する。

1. Scriptの`OnDestroy`を呼び出す
2. 外部APIやScript Storageから登録を解除する
3. Componentの`Destroy`を呼び出す
4. ChunkからSwap Removeする
5. 移動したEntityの`EntityLocation`を更新する
6. 削除EntityのLocationを無効化し、generationを更新する
7. Entity IndexをFree Listへ戻す

削除済みHandleはgenerationの不一致で無効と判定する。`PendingDestroy`後のComponent取得・変更は失敗として扱い、同じフレーム内で削除対象を再利用しない。

Chunk内からEntityを削除する場合は空きを残さず、末尾Entityを削除位置へ移動するSwap Removeを使用する。
```
Before

row 0 : A
row 1 : B  ← Remove
row 2 : C

After

row 0 : A
row 1 : C
```

すべてのComponent Columnについて同じRow移動を行う。
```
Entity[2]       → Entity[1]
Transform[2]    → Transform[1]
MeshRenderer[2] → MeshRenderer[1]
BoxCollider[2]  → BoxCollider[1]
```
移動したEntity Cについては`EntityLocation::row`を更新する。
これによりChunk内部を常に密な状態に保つ。

### 実行フロー
NativeECSのSystemはEngine全体のフレーム処理の中でSchedulerから実行する。
概念的な流れは以下とする。
```
Frame

Input
 ↓
Script Update
 ↓
Native ECS Begin
 ↓
Physics
 ↓
Post Physics
 ↓
Render Submission
 ↓
Native ECS End
 ↓
Rendering
```

`Native ECS End`では、そのフレームに記録された構造変更Commandを安全な順序で反映する。Destroy要求の実体削除もこのフレーム末尾のFlushに含める。
NativeECS内部ではさらに`SystemPhase`によって実行タイミングを分類する。
```
PreUpdate
 ↓
Update
 ↓
PostUpdate
 ↓
PrePhysics
 ↓
Physics
 ↓
PostPhysics
 ↓
PreRender
 ↓
RenderSubmission
```

実際のPhase構成についてはPhysics・Animation・Rendering Pipelineとの統合設計に合わせて調整する。
SchedulerはPhase・System依存関係・Read / Write情報を元にExecution Planを構築し、将来的には依存関係のないSystemおよびChunkの並列実行へ拡張できる構造とする。
## ScriptSystem
ゲームロジックの記述を目的とした、C#上で動作するScript実行基盤。
`NativeECS`がデータ指向による実行性能を優先するのに対し、`ScriptSystem`ではオブジェクト指向によるコードの集約と開発イテレーションの回しやすさを優先する。
Scriptは`class`として定義し、状態と振る舞いを同一の型に保持できる。
### Sample
```csharp
public sealed class Enemy : Script
{
    private EnemyState _state;
    private float _attackTimer;

    private readonly List<Entity> _targets = new();

    public override void Update()
    {
        var transform = GetTransform();
        transform.Position += ComputeMovement(transform.Position);

        // Enemy固有のゲームロジック
    }
}
```

### ScriptStorage
Scriptは型ごとに`ScriptStorage<T>`へ格納する。
```text
ScriptStorage<Enemy>
├─ DenseEntities
│  ├─ Entity 12
│  ├─ Entity 35
│  └─ Entity 81
│
├─ DenseScripts
│  ├─ Enemy ref
│  ├─ Enemy ref
│  └─ Enemy ref
│
└─ Sparse
   └─ EntityIndex → (Generation, DenseIndex)
```

`DenseScripts`を先頭から走査することで、同一型のScriptをまとめて実行する。
ScriptはC#の`class`であるため、配列上で連続するのはScriptへの参照であり、Scriptインスタンスそのもののメモリ連続性は保証しない。
これは実行性能よりも、

- 継承
- `List<T>`や`Dictionary<TKey, TValue>`
- Script内への状態保持
- Hot Reload
- 一つの型へのゲームロジック集約

といったC#による開発効率を優先するための設計。
### Sparse Index
Entityから特定型のScriptを高速に取得するため、`ScriptStorage<T>`はSparse Indexを持つ。
```text
Entity Handle
    ↓
Sparse
    ↓
DenseIndex
    ↓
DenseScripts[DenseIndex]
```
これにより
```csharp
entity.GetScript<Enemy>();
entity.TryGetScript<Enemy>(out var enemy);
```
といったアクセスをO(1)で行える。

Sparse領域はEntity Index全体を一つの巨大な配列として確保せず、一定数のEntityごとにページ分割して必要なページのみ確保する。
```text
SparsePages
├─ Page 0 → uint[PageSize]
├─ Page 1 → null
├─ Page 2 → uint[PageSize]
└─ ...
```
Entity HandleのIndexからページとページ内Indexを算出し、Sparseに保存されたGenerationも照合する。
```text
PageIndex  = EntityIndex / PageSize
LocalIndex = EntityIndex % PageSize
```
`PageSize`を2の累乗とする場合はビット演算で算出可能。

### Native Componentへのアクセス
Script自身の状態はManaged側に保持するが、TransformやRigidbodyなどのComponentは`NativeECS`が所有する。
ScriptからComponentへアクセスする場合はEntityを経由したAPI proxyを使用する。Script側はNativeの参照・ポインタ・Chunk位置を保持しない。

```text
Script
  ↓ Entity
EntityLocation
  ↓
Archetype / Chunk / Row
  ↓
Component
```

```csharp
public readonly struct TransformProxy
{
    private readonly Entity _entity;

    public Vector3 Position
    {
        get => NativeAPI.GetPosition(_entity);
        set => NativeAPI.SetPosition(_entity, value);
    }
}
```

ProxyはEntity Handleを保持し、アクセスのたびにNative APIへ処理を委譲する。Archetype MigrationやHot ReloadでNative側の配置が変わっても、古い参照を保持し続けない。長期間保持するScript間参照も、ScriptインスタンスではなくEntityを保存して必要時に再取得する。

`EntityLocation`から所属ChunkとRowを取得できるため、Native API内部のComponentアクセスは基本的にO(1)で行う。
このためストレージ構造は、
```text
Script
    → Managed Sparse Storage

Component
    → Native Archetype Storage
```
というハイブリッド構成となる。
### Script間アクセス
別ScriptへのアクセスもEntityを経由して行う。

```csharp
if (entity.TryGetScript<HealthReaction>(out var reaction))
{
    reaction.Play();
}
```

内部では対象型の`ScriptStorage<T>`からSparse Indexを使用して取得する。
長期間保持する参照についてはHot ReloadやScriptの削除を考慮し、必要に応じてScriptインスタンスそのものではなくEntityを保持し、使用時にScriptを再取得する。
### Lifecycle
ScriptのLifecycleはManaged側で一括管理する。
例：
```text
OnCreate
Start
Update
FixedUpdate
LateUpdate
OnDestroy
```
Native側から各Scriptを個別に呼び出すことはせず、Lifecycle単位でManaged側へ処理を委譲する。
```text
Engine
  ↓
ScriptSystem.Update()
  ↓
ScriptStorage<Player>.Update
ScriptStorage<Enemy>.Update
ScriptStorage<Boss>.Update
...
```
これによりNative / Managed境界をScript単位で往復することを避ける。
### NativeECSとの使い分け
`ScriptSystem`は、単一Entity内の状態や処理が複雑なゲームロジックに使用する。

例：
- Player
- Enemy AI
- Boss AI
- ギミック
- ステージ進行
- イベント制御

将来的にScriptSystemでは実行性能が不足するゲーム固有処理が必要になった場合は、C#からNative Archetypeを直接Chunk単位で処理する高速実行経路の追加を検討する。
