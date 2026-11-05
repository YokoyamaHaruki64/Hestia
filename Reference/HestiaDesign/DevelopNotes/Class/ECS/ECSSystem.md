# ECS System

Engineが保持するECSSystemを、static APIの入口として使用するクラスモック。
最初は1つのECSSystemが1つのECSWorldを所有する。インスタンスを取得するpublic APIは設けない。

## 基本方針

- EngineがECSSystemの実体と寿命を管理する。
- ECSSystemはComponentRegistryとECSWorldを値として所有する。
- privateのs_instanceから実体を参照し、static関数からWorldのpublic APIへ委譲する。
- Entity・Archetype・Processの具体的な管理処理はECSWorldに置く。
- 対象はEngine内部のECS。Game向けBoundary APIやFacadeは今回設計しない。

## ECSSystem

**役割**

1つのWorldとComponentの型登録情報を保持し、利用側へstaticの操作入口を提供する。

```cpp
namespace Hestia
{
    class ECSSystem
    {
    public:
        ECSSystem();
        ~ECSSystem();

        template<class T>
        static ComponentTypeId RegisterComponent(std::string_view name);

        static EntityHandle CreateEntity();
        static void DestroyEntity(EntityHandle handle);
        static Entity* GetEntity(EntityHandle handle);

        static void RegisterProcesses(
            void (*registerFunctions)(ECSWorld&));

        template<auto Function>
        static void AddProcess(EntityHandle handle);

        template<auto Function>
        static void RemoveProcess(EntityHandle handle);

        static void Run(ECSProcessPhase phase);
        static void EndFrame();

    private:
        static ECSSystem* s_instance;

        ComponentRegistry m_components;
        ECSWorld m_world;
    };
}
```

[ComponentRegistry / ComponentTypeId](./ComponentRegistry.md)  
[ECSWorld](./ECSWorld.md#ecsworld)  
[Entity / EntityHandle](./Entity.md)  
[ECSProcess](./ECSProcess.md#ecsprocess)

ECSProcessPhaseの初期案はFrameBegin／Update／LateUpdate／PreRender／FixedUpdateとする。上記は主要APIのモックであり、追加・削除時の行操作や失敗通知の詳細は後続で定める。

**所有・参照**：EngineがECSSystemを値として所有する。s_instanceはその実体への非所有ポインタ。ECSWorldは同じSystemのRegistryを非所有で参照する。RegistryをWorldより先に構築し、WorldとそのComponentの破棄後まで保持する。

ここでのシングルトンは「Engineが持つ実体は1つで、利用側はstaticから操作する」という意味。publicのインスタンスGetterやWorld Getterは置かず、独立した遅延生成の実体を追加しない。

## staticからWorldへの委譲

Entity取得の例。WorldがHandleの検証と現在位置の解決を行う。

```cpp
Entity* ECSSystem::GetEntity(EntityHandle handle)
{
    return s_instance->m_world.GetEntity(handle);
}
```

[ECSWorld](./ECSWorld.md#ecsworld)  
[Entity / EntityHandle](./Entity.md)

Process登録は、初期化中に専用関数へECSWorldの非所有参照を渡して行う。初期化完了後の登録は認めず、エラーログを出して操作しない。RegisterProcessesはその呼び出し入口のAPI案で、初期化状態の具体的な保持方法は未確定。

```cpp
void RegisterECSProcesses(ECSWorld& world)
{
    world.RegisterProcess<&Move>(ECSProcessPhase::Update);
    world.RegisterProcess<&SubmitMesh>(ECSProcessPhase::PreRender);
}

// Engineの初期化中に呼ぶ
ECSSystem::RegisterProcesses(&RegisterECSProcesses);
```

[ECSProcess](./ECSProcess.md#ecsprocess)  
[生成と実行の内部](./ECSProcessExecution.md#createの内部)

Entityの構成変更も、関数をTemplate引数で指定する。

```cpp
ECSSystem::AddProcess<&Move>(entityHandle);
ECSSystem::RemoveProcess<&Move>(entityHandle);
```

AddProcess／RemoveProcessでは、[ProcessKey](./ECSProcessExecution.md#processkey)へTraitsの記述を集約し、Handleとともに共通キーをWorldへ渡す。Worldは共通入口をキーに登録表を検索する。Componentが揃った別Processも実行対象となり、EntityごとのProcess所属は保存しない。

RegisterComponentはRegistryへ、CreateEntity／DestroyEntity／Run／EndFrameはWorldの対応する操作へ委譲する。System側にSlot検索やChunk走査の処理を重複して置かない。

登録関数へ渡すWorld参照は、その呼び出し中だけ使用し、利用側では保持しない。一般の利用側にWorld Getterは公開せず、通常操作はECSSystemのstatic APIから行う。登録・生成とMapへの追加は[ECSWorld](./ECSWorld.md#archetypeとprocessの接続)へまとめる。

## 所有関係と通常経路

```text
Engine
└─ ECSSystem
   ├─ ComponentRegistry
   └─ ECSWorld ── 非所有参照 ──→ ComponentRegistry
      ├─ EntitySlot表・再利用index
      ├─ Archetype群
      │  └─ Layout・Chunk群
      └─ ECSProcess群
         └─ 対象Archetypeへの非所有参照

利用側 → ECSSystemのstatic API → s_instance → ECSWorldのpublic API
```

[Engine](../Engine.md#engine)

ECSSystem生成時にWorldへRegistryを接続し、staticの入口を使用可能にする。Name・Tag・Layer等の型登録を済ませてからEntityやProcessを作る。World生成そのものと、Entityを保存するArchetypeの生成は分けて考える。

終了時はProcessの非所有参照、Archetype／Chunk／Componentを解放し、Registryの型操作が必要なくなってからRegistryを破棄する。s_instanceの接続と解除はECSSystemの生成・終了に対応させる。

## 未確定事項

- Engineの初期化・終了処理のどこで型登録とWorldの終了を行うか。他Subsystemとの終了順も含めて後続で定める。
- 5つのphase初期案をEngineから呼ぶ位置・順序とFixedUpdateの時間管理。通常phaseとOnAttach／OnStartは[ECSProcess](./ECSProcess.md#ecsprocessphase)に整理する。
- 構造変更の行操作と、失敗時の復旧手順。初期activeはON、NameはEntity{index}、Tag・Layerは0相当とする。
- Engineからフレーム末尾のEndFrameを呼ぶ位置。追加・削除は実行外で即時、実行中は現在のProcessのRun終了後に反映し、実破棄はフレーム末尾とする。
- staticの利用開始前・終了後の呼び出しを検出する具体的な仕組み。エラー時は当面エラーログのみとし、対象操作を行わない。
