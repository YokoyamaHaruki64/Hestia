# ECS Process

処理関数を指定してECSProcessを作り、対象Archetypeを対応付け、RunでChunk単位に実行するクラスモック。
本資料では責務と保持データ、生成から実行までを示す。Templateと関数呼び出しの詳細は[ECSProcessExecution](./ECSProcessExecution.md)に分ける。

## 基本方針

- 登録・実行の単位はECSProcess。処理関数をクラスへまとめることは要求しない。
- 処理関数をTemplateの値引数で指定し、Component条件と共通入口を持つECSProcessを作る。
- ECSSystemが対象フェーズのProcessを順次実行し、各ProcessもArchetype／Chunkを順次処理する。
- ArchetypeがLayoutとChunk群を所有する。ECSProcessはArchetypeを非所有で参照する。
- 並列化と自動ジョブ化は後続で検討する。宣言は概念設計用のAPIスケッチ。

## ECSProcess

**役割**

処理関数の実行タイミング、Component条件、対象Archetype、共通入口の関数ポインタを保持する。
Runで対象Chunkを走査し、有効行数・Entity列・必要なComponent配列を元の処理関数へ渡す。

```cpp
namespace Hestia
{
    class ECSProcess
    {
    public:
        template<auto Function>
        static ECSProcess Create(
            const ComponentRegistry& registry,
            ECSProcessPhase phase);

        ECSProcessPhase GetPhase() const;
        ECSProcessInvoke GetInvoke() const;
        std::span<const ECSProcessComponent> GetComponents() const;
        void RegisterArchetype(Archetype& archetype);
        void Run();

    private:
        ECSProcess() = default;

        ECSProcessPhase m_phase;
        std::vector<ECSProcessComponent> m_components;
        std::vector<ECSProcessArchetype> m_archetypes;
        ECSProcessInvoke m_invoke = nullptr;
    };
}
```

[ComponentRegistry](./ComponentRegistry.md#componentregistry)  
[ECSProcessComponent](#ecsprocesscomponent)  
[ECSProcessArchetype](#ecsprocessarchetype)  
[ECSProcessInvoke](#ecsprocessinvoke)

[ECSWorld](./ECSWorld.md#ecsworld)  
[Archetype](./Archetype.md#archetype)

ECSProcessPhaseは通常処理の実行タイミングを表す。種類は下記の初期案で検討する。GetInvokeはWorldの登録表で使う共通入口、GetComponentsはEntityへの追加・削除を判断するための要求条件を返す。

**所有・参照**：ECSSystemが所有するECSWorldがProcessを保持する。ProcessはComponent条件と対象表を所有し、Archetypeは非所有で参照する。ChunkやComponent配列のポインタは保持し続けず、実行時に取得する。

### 関数を指定して作る

利用側の入口は、例えば次の形とする。phaseは呼び出し側で選んだフェーズを表す説明用の値。

```cpp
void SubmitMesh(
    size_t count,
    const Entity* entities,
    const Transform* transforms,
    const Renderer* renderers,
    const MeshRenderer* meshRenderers);

auto process = ECSProcess::Create<&SubmitMesh>(registry, phase);
```

[Entity](./Entity.md#entity)  
[Transform / Renderer / MeshRenderer](./Component.md)  
[Createの内部](./ECSProcessExecution.md#createの内部)

`< >`には呼ぶ関数のアドレス、`( )`には実行時に使うRegistryとフェーズを渡す。Createは関数型からComponent条件を作り、その関数専用の共通入口のアドレスを保持したインスタンスを返す。

```text
Create<&SubmitMesh>(registry, phase)から返るprocess
├─ m_phase       = 指定したフェーズ
├─ m_components  = 必要なComponent IDとRead／Write
├─ m_invoke      = SubmitMesh用の共通入口へのポインタ
└─ m_archetypes  = まだ空
```

元のSubmitMeshのポインタを保存する別メンバは設けない。共通入口が呼ぶ関数はTemplate引数によってコンパイル時に決まり、その入口のポインタをm_invokeに保持する。

### 対象Archetypeを対応付ける

Createだけでは対象表は埋まらない。ECSWorld::RegisterProcessが既存ArchetypeをRegisterArchetypeへ渡し、Processが条件を照合する。一致した場合だけArchetypeへの参照と列番号を対象表へ追加する。新しいArchetypeの生成時にもWorldから同じ操作を呼ぶ。

```text
処理関数のComponent引数順：Transform、Renderer、MeshRenderer
対象Archetypeの列順の例  ：Renderer、MeshRenderer、Transform、Name、Tag、Layer
保存する列番号          ：[2, 0, 1]
```

新しいArchetypeを生成した場合も、登録済みProcessの条件と照合して対象表へ追加する方向とする。対象Archetypeは余分なComponentを持っていてもよい。全Archetypeに含めるName・Tag・Layerも、Processが要求した場合だけ関数へ渡す。

### Runで実行する

```text
ECSSystemが対象フェーズのProcessを順次実行
  （ECSWorld::Runへ委譲し、GetPhaseで対象を選ぶ）
  ↓
process.Run()
  ↓
対象表からArchetypeを順次取得
  ↓
所属Chunkを順次取得
  ↓
m_invokeへChunkと列番号を渡す
  ↓
共通入口がEntity列・Component配列・有効行数を取得
  ↓
SubmitMesh(count, entities, Component配列...)
```

有効数0のChunkは処理をスキップする。ここで有効数は構築済みの行数で、非activeなEntityも含む。各処理関数がEntity列からIsActiveを確認し、非active行と破棄予約済みの行をスキップする。渡す件数はChunkの有効Entity数であり、容量ではない。条件一致と列番号の解決は事前に済ませ、Runで型名やComponent IDを検索し直さない。

ループと共通入口の具体例は[ECSProcessExecutionのRunの流れ](./ECSProcessExecution.md#runの流れ)を参照。

### ECSProcessComponent

**役割**

処理関数が要求するComponent IDと、読み取り／書き込みの区別を保持する。

```cpp
namespace Hestia
{
    enum class ECSComponentAccess
    {
        Read,
        Write,
    };

    struct ECSProcessComponent
    {
        ComponentTypeId m_type;
        ECSComponentAccess m_access;
    };
}
```

[ComponentTypeId](./ComponentRegistry.md#componenttypeid)

Entity列は共通引数とし、Component条件や列番号には含めない。条件の並びは後続のComponent引数順に対応させる。Component引数のconst付きはRead、constなしはWriteとして作る。Read／Writeを保持するが、今回の実行は逐次とする。

### ECSProcessArchetype

**役割**

条件に一致したArchetypeへの非所有参照と、Component引数順に対応する列番号を保持する。

```cpp
namespace Hestia
{
    struct ECSProcessArchetype
    {
        Archetype* m_archetype = nullptr;
        std::vector<size_t> m_columnIndices;
    };
}
```

[Archetype](./Archetype.md#archetype)  
[ComponentLayout](./Archetype.md#componentlayout)  
[ECSProcessComponent](#ecsprocesscomponent)

m_columnIndices[i]は、Processのm_components[i]に対応するArchetype内の列番号。Component IDやバイトOffsetではない。

Archetypeごとに列配置が異なるため、対応もArchetypeごとに保持する。所属Chunkは共通Layoutを使うので、同じ列番号を利用できる。Runはその時点でのChunk群を取得し、Chunk追加ごとに対象表を作り直す必要はない。

### ECSProcessInvoke

**役割**

Component引数の構成が異なる処理関数へ、同じ形式で接続する関数ポインタ型。

```cpp
namespace Hestia
{
    using ECSProcessInvoke = void (*)(
        Chunk& chunk,
        std::span<const size_t> columnIndices);
}
```

[Chunk](./Chunk.md#chunk)  
[共通入口とTemplate](./ECSProcessExecution.md#共通入口のtemplateを書く)

実体を持つ管理クラスではなく型の別名。Component配列の取得、型変換、処理本体の呼び出しは共通入口が担当する。引数形式と取得APIの最終形は未確定。

## ECSProcessPhase

通常処理のphaseは、初期構想として次の5つを置く。列挙順はEngineの呼び出し順を確定するものではない。

```cpp
enum class ECSProcessPhase
{
    FrameBegin,
    Update,
    LateUpdate,
    PreRender,
    FixedUpdate,
};
```

同じphase内のProcessは初期化時の登録順で実行する。FixedUpdateは固定時間刻みでの更新を想定し、1フレーム内で0回から複数回の実行があり得る。時間刻み、追いつき処理、各phaseをEngineのどこで呼ぶかは未確定。

## OnAttach／OnStartの検討

通常phaseのChunk走査とは別に、1つのECSProcessへ任意のライフサイクル関数を関連付ける案を残す。現時点では登録APIやメンバを確定せず、通常phaseの列挙にも追加しない。

```text
ECSProcess
├─ 通常処理：指定phaseで実行
├─ OnAttach：任意の関連関数として検討
└─ OnStart ：任意の関連関数として検討
```

- OnAttach：Component構成が不一致から一致になったEntityへの通知を想定する。別Processの追加で条件が揃った場合も含めて検討する。
- OnStart：一致後、初めてactiveで通常処理を実行する直前の呼び出しを想定する。EntityとProcessの組ごとの実行待ち状態が必要になる。
- 通常処理は一致するChunk全体を走査するが、これらは対象になった一部のEntityだけを呼ぶ管理が必要になる。

関連付ける方法、各関数の引数とComponent条件、対象Entityの記録、状態の所有者、非active時の扱い、条件から外れて再び一致した場合の再実行規則は未確定。現在のモックは通常処理のみを対象とし、特殊な実行方式は後続で設計する。

## 未確定事項

- 5つのphase初期案とEngineの更新処理との具体的な対応。
- OnAttach／OnStartの任意関数の関連付け、引数、対象・実行待ち状態の管理。
- 対象表への重複登録の扱いと、Process削除やArchetype回収時の参照更新。保持と登録の調停はECSWorldが担当する。
- Archetype破棄時の非所有参照の解除。対象ArchetypeはRun中に有効である必要がある。
- 生成や構造変更失敗時の復旧手順。Run中のEntity生成・Component追加・削除は現在のRun全体の終了後、Entityの実破棄はフレーム末尾とする。WorldへのProcess登録は初期化時のみ。[ECSWorld](./ECSWorld.md)が要求を保持・反映する。
- 時間など、Component以外の入力の扱い。

詳細なTemplate構文と呼び出しの対応は[ECSProcessExecution](./ECSProcessExecution.md)を参照。外部のコード生成ツールは使用せず、通常のC++のTemplate具体化を使う草案とする。
