# ECS World

EntitySlot、Archetype群、ECSProcess群を保持するWorldのクラスモック。
ECSSystemが1つのWorldを所有し、static APIからWorldのpublic操作を呼ぶ。宣言は概念設計用のスケッチ。

## ECSWorld

**役割**

Entityの識別と保存位置、ArchetypeとChunkへの配置、Process登録と逐次実行を管理する。
Componentの型情報は、ECSSystemが所有するRegistryを参照する。

```cpp
namespace Hestia
{
    class ECSWorld
    {
    public:
        explicit ECSWorld(const ComponentRegistry& registry);

        EntityHandle CreateEntity();
        void DestroyEntity(EntityHandle handle);

        Entity* GetEntity(EntityHandle handle);
        const Entity* GetEntity(EntityHandle handle) const;

        template<auto Function>
        void RegisterProcess(ECSProcessPhase phase);
        void AddProcess(EntityHandle handle, ECSProcessInvoke process);
        void RemoveProcess(EntityHandle handle, ECSProcessInvoke process);

        void Run(ECSProcessPhase phase);
        void EndFrame();

    private:
        void RegisterProcess(ECSProcess process);
        Archetype& GetOrCreateArchetype(ArchetypeSignature signature);

        void FlushChanges();
        void FlushDestroy();

        const ComponentRegistry* m_components = nullptr;

        std::vector<EntitySlot> m_entitySlots;
        std::vector<uint32_t> m_freeIndices;
        std::vector<std::unique_ptr<Archetype>> m_archetypes;
        std::vector<ECSProcess> m_processes;
        std::unordered_map<ECSProcessInvoke, size_t> m_processIndices;

        std::vector<ECSChangeRequest> m_pendingChanges;
        std::vector<EntityHandle> m_pendingDestroy;
        bool m_isRunningProcess = false;

        size_t m_chunkByteSize;
    };
}
```

[ComponentRegistry](./ComponentRegistry.md#componentregistry)  
[Entity / EntityHandle / EntitySlot](./Entity.md)  
[Archetype / ArchetypeSignature](./Archetype.md)  
[ECSProcess](./ECSProcess.md#ecsprocess)

**所有・参照**：ECSSystemがWorldを値として所有する。WorldはSlot表、再利用するindexの一覧、Archetype群、Process群を所有する。Registryへのポインタは非所有。Archetypeはunique_ptrで保持し、所有側のvectorが伸びてもArchetype実体のアドレスを維持する。

m_chunkByteSizeはArchetypeに渡す固定バイト数をWorldで保持する案。具体値と設定を受け取るAPIは未確定。WorldのコンストラクタはRegistryを接続し、必ずしもその時点でArchetypeやChunkを作らない。

## Entityの生成・取得・破棄

CreateEntityは、まずindexとgenerationを確保してSlotをPendingCreateにし、Handleを予約する。Process実行外では続けて即時生成し、実行中はCreateEntity要求をm_pendingChangesへ積んで予約Handleを返す。生成の反映は次の流れで行う。

1. 予約済みSlotを確認する。
2. Name・Tag・Layerを含む基本Archetypeを取得または生成する。
3. 空きのあるChunkにEntityと基本Componentを配置する。
4. SlotのArchetype／Chunk／rowを設定してAliveへ移す。予約したHandleの値は変えない。

初期activeはON、Nameは`Entity{index}`（indexが12なら`Entity12`）、Tag・Layerは0相当とする。Chunkの行確保・構築の具体APIは未確定。

生成待ちHandleへのAddProcess／RemoveProcessも保留する。同じ要求列で生成、その後の追加・削除を要求順に反映するため、CreateEntityの直後にAddProcessを呼べる。生成待ちHandleへのDestroyEntityは生成を取り消す。EntityとComponentを構築せず、SlotをFreeへ戻してgenerationを更新し、indexを再利用候補へ戻す。旧Handleに対する生成要求と後続の変更要求は、反映時にgeneration不一致としてスキップする。

GetEntityはHandleのindex、Slotの状態、generationを確認して現在位置からEntityを取得する。無効なHandleと生成待ちHandleにはnullptrを返す。生成待ちは予約として有効であり、GetEntityがnullptrでも直後のAddProcessをエラーにはしない。非activeは破棄済みではないため、Handleが有効なら取得できる。返す参照は非所有で、構造変更をまたいで保持しない。

生成済みEntityへのDestroyEntityはEntityの破棄予約bitを立て、Handleをm_pendingDestroyへ追加する。予約済みなら重複追加しない。activeの設定値は変更せず、IsActiveが破棄予約を含めてfalseを返す。

実際のComponentとEntityの破棄はフレーム末尾のEndFrameで行う。FlushDestroyがHandleから現在位置を引き直し、必要な行移動、移動したEntityのSlot更新、破棄Slotの無効化・generation更新、indexの再利用を調停する。予約中はGetEntityで取得でき、IsDestroyで状態を確認できる。EndFrameはProcess実行外で呼ぶ前提とし、Engine側の接続位置と行移動の具体手順は未確定。

## ArchetypeとProcessの接続

```text
初期化時の専用関数からworld.RegisterProcess<Function>(phase)
  ↓ Worldが参照するRegistryを使ってECSProcess::Create
World内部のRegisterProcess(process)
  ├─ 既存ArchetypeをProcessへ提示
  │    └─ 条件に一致する場合、参照と列番号を登録
  └─ WorldがProcessを所有

新しいArchetypeを生成
  └─ 登録済みProcessへ提示
       └─ 条件に一致する場合、参照と列番号を登録
```

publicのTemplate入口でProcessを生成し、privateの同名オーバーロードへ渡す。初期化時のみという制約を確認してから生成・登録し、呼び出された順序で保持する。

```cpp
template<auto Function>
void ECSWorld::RegisterProcess(ECSProcessPhase phase)
{
    // 登録可能な初期化段階であることを確認済みとする
    auto process = ECSProcess::Create<Function>(*m_components, phase);
    RegisterProcess(std::move(process));
}
```

[ECSProcess::Create](./ECSProcess.md#関数を指定して作る)

内部のRegisterProcessはProcessを保持し、GetInvokeを登録表のキーにする。CreateとAdd／Removeは同じProcessKeyを使う。

WorldがArchetype生成とProcess登録の両方を管理するため、対応付けを更新する契機もWorldが把握する。条件の照合と対象表の保持は[ECSProcess](./ECSProcess.md#ecsprocess)が担当する。

GetOrCreateArchetypeは構成を正規化し、Name・Tag・Layerを含むSignatureで検索する。新規生成時にはRegistryと固定バイト数を渡して共通Layoutを作る。Archetypeの検索を高速化する表は今回追加せず、具体方式は後続で定める。

## 関数をキーにしたProcessの参照

RegisterProcessでは、Processが保持する共通入口の関数ポインタをキー、m_processes内のindexを値としてm_processIndicesへ登録する。同じ関数を指定するAddProcess／RemoveProcessは、この表から登録済みProcessの条件を取得する。vectorの再確保をまたいでProcessへのポインタを保持しない。

キーは[ECSProcessInvoke](./ECSProcess.md#ecsprocessinvoke)型の専用入口で、元の異なる型の関数ポインタをvoid*へ変換しない。実行中だけの識別であり、保存用IDには使わない。同じ関数の重複登録はphaseが異なっても禁止する。登録時に共通入口のキーがm_processIndicesに存在するかを確認して判定する。重複登録や未登録関数の指定などのエラーは、当面エラーログのみで通知し、対象操作を行わない。

## EntityへのProcess追加・削除

WorldへのRegisterProcessは処理そのものの登録。EntityへのAddProcess／RemoveProcessは、そのProcessが要求するComponent構成を変更する操作とする。EntityにはProcessの所属一覧を持たせず、構成が条件を満たせば他のProcessも実行対象になる。

- AddProcess：要求されるComponentのうち、不足するものだけを追加する。既存のComponent値は維持する。
- RemoveProcess：対象Processが要求するComponentのうち、同じEntityに現在一致する他の登録済みProcessが要求するものを除いて削除する。Name・Tag・Layerは常に残す。
- 他Processによる要求はRead／Writeのどちらも保護対象に含める。Entityのactive状態ではなく、Component構成との一致で判断する。
- 削除できる固有Componentがなければ構成は変わらず、対象Processも引き続き一致する。個別に無効化したい処理には、その処理専用のComponentを用意する方向とする。個別に無効化できない操作の通知は、当面エラーログとする。

追加・削除では移動先Signatureを作り、Archetype間で保持するComponentを移し、不足分の構築と削除分の破棄を行う。Entityの状態bitも移し、Slotの現在位置を更新する。具体的な行操作APIと失敗時の復旧手順は未確定。エラー通知は当面ログのみとする。

### ECSChangeRequest

実行中の生成・構造変更要求は、Entityの参照ではなくHandleとProcessのキーで保持する。生成要求ではm_processを使わない。

```cpp
namespace Hestia
{
    enum class ECSChangeKind
    {
        CreateEntity,
        AddProcess,
        RemoveProcess,
    };

    struct ECSChangeRequest
    {
        EntityHandle m_entity;
        ECSProcessInvoke m_process = nullptr;
        ECSChangeKind m_kind;
    };
}
```

[EntityHandle](./Entity.md#entityhandle)  
[ECSProcessInvoke](./ECSProcess.md#ecsprocessinvoke)

Process実行外の追加・削除は即時反映する。m_isRunningProcessがtrueの間はm_pendingChangesへ要求順に蓄積し、現在のECSProcess::Run全体が終了した直後にFlushChangesで順次反映する。Chunk単位のコールバック間には反映しない。

反映時にHandleの有効性と現在位置を引き直し、その時点の構成で追加・削除を判断する。追加・削除用のEntity内フラグは設けない。破棄予約済みEntityへの追加・削除は受け付けず、予約前に保留された追加・削除も反映時にスキップする。破棄予約は取り消さない。無効Handleなどの不正な操作はエラーログで通知し、対象操作を行わない。

## フェーズの実行

WorldのRunは指定フェーズに属するProcessを選び、順次Runを呼ぶ。各Processは自分の対象ArchetypeとChunkを走査する。各Runの前後でm_isRunningProcessを切り替え、Run終了後にFlushChangesを呼んでから次のProcessへ進む。変更後の構成は次のProcessから見える。Entityの実破棄はここでは行わず、フレーム末尾まで保留する。

```text
Engine
  → ECSSystem::Run(phase)
  → ECSWorld::Run(phase)
  → 対象のECSProcess::Runを順次呼ぶ
  → 各ChunkからEntity列とComponent列を取得
  → 処理関数でactiveを確認して実行
```

同じphase内は登録順に実行し、大まかな実行分類はphaseで分ける。Processの保持はvectorとし、登録順に走査して指定phaseのものを選ぶ。phaseの初期案は[ECSProcessPhase](./ECSProcess.md#ecsprocessphase)にまとめる。Engineから呼ぶ具体的な位置・順序とFixedUpdateの時間管理は未確定。

Process登録は初期化時の専用関数に限定する。実行開始後の登録はエラーログを出して行わない。[ECSSystem::RegisterProcesses](./ECSSystem.md#staticからworldへの委譲)から専用関数へWorld参照を渡し、world.RegisterProcessを連続して記述する。

## 終了と未確定事項

Processが持つArchetypeへの非所有参照を破棄してから、ArchetypeとChunkを破棄する。Component破棄が完了するまでRegistryを存続させる。World単位の終了をEngineのどの処理へ接続するかは未確定。

- Archetype間移動の具体API、構築・移動の失敗時の扱い。
- OnAttach／OnStartの関連付けと特殊な実行管理。[ECSProcess](./ECSProcess.md#onattachonstartの検討)で通常phaseと分けて検討する。
- 空ChunkやArchetypeの回収、Process削除時の扱い。
- Slotの無効値、generation周回、index再利用の手順。
- Entity生成や配置失敗時の復旧と予約Slotの扱い。通知は当面エラーログのみとする。

当面は1Worldとし、Handleを複数Worldで使い分けるための識別情報は追加しない。
