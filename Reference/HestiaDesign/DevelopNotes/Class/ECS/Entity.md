# Entity

Chunk内のEntity実体、長期間保持するEntityHandle、現在位置を管理するEntitySlotのクラスモック。
activeはEntity内に保持し、name・tag・layerはすべてのArchetypeに含める基本Componentとする。宣言は概念設計用のAPIスケッチ。

## 基本方針

- ChunkのEntity列にEntityの実体を配置し、同じ行のComponentと対応させる。
- 上位2bitを破棄予約とactive、下位30bitをEntity管理表のindexに使う。generationは32bitを保持する。
- 外部で長期間保持する識別子はEntityHandleとし、状態bitを含めない。
- EntitySlotはHandleの有効性と現在のArchetype／Chunk／rowを管理する。
- Name・Tag・Layerは全ArchetypeのSignatureに含め、各ChunkにComponent列として配置する。
- ECSProcessはEntity列を直接受け取り、各処理関数がIsActiveを確認して非activeの行をスキップする。

## Entity

**役割**

Entityの識別情報とactiveを保持するChunk内の実体。Systemの走査中は管理表を引かずにactiveを読める。

```cpp
namespace Hestia
{
    class Entity
    {
    public:
        explicit Entity(EntityHandle handle, bool active);

        EntityHandle GetHandle() const;
        uint32_t GetIndex() const;

        bool IsActive() const;
        bool IsDestroy() const;
        void SetActive(bool active);

    private:
        uint32_t m_indexAndFlags;
        uint32_t m_generation;
    };
}
```

[EntityHandle](#entityhandle)  
[Chunk](./Chunk.md#chunk)

**所有・参照**：Entityの実体はChunkの保存領域に置く。処理中のEntity参照は非所有。Name・Tag・Layerは別のComponent列に置き、Entity本体へ文字列や属性値を埋め込まない。

```text
Entity：uint32_tを2つ持つ8バイト想定の構成
  m_indexAndFlags [ pendingDestroy 1bit | active 1bit | index 30bit ]
  m_generation   [ generation 32bit                                ]
```

ビットフィールドの宣言ではなく、32bit整数のMaskで扱う。IsActiveは「activeがON、かつ破棄予約がOFF」の場合だけtrueを返す。SetActiveはactiveのbitだけを変更する。GetIndexとGetHandleは上位2bitを除いたindexを返す。実際のsizeofとアラインメントは実装時に確認する。

破棄要求では破棄予約bitだけを立て、設定されたactiveは変更しない。予約後にSetActive(true)を呼んでもIsActiveはfalseのままで、破棄予約は取り消されない。IsDestroyは破棄予約bitが立っていればtrueを返す。実破棄済みEntityを参照して確認するAPIではない。予約bitの変更はWorldが管理し、一般の利用側から解除するAPIは設けない。Worldから変更する内部APIの形は未確定。

activeがfalseでもEntityは存在し、Handleも有効。非activeは更新・描画を含む通常のECSProcessの処理対象から除外する意味とする。最初は各処理関数の判定で実現し、ECSProcess自体が非active行を自動除外する実装にはしない。

### EntityHandle

**役割**

Entityの移動後も、管理表から現在位置を取得できる識別子。

```cpp
namespace Hestia
{
    struct EntityHandle
    {
        uint32_t m_index;
        uint32_t m_generation;
    };
}
```

[Entity](#entity)  
[EntitySlot](#entityslot)

Handleのindexは30bitの識別番号として扱い、上位2bitは0にする。状態変更で識別が変わらないよう、EntityからHandleを取得するときにactiveと破棄予約を除く。

HandleのコピーはEntity実体のコピーではなく、同じEntityの識別子を保持する操作。[ECSSystem::GetEntity](./ECSSystem.md#staticからworldへの委譲)からECSWorldのGetEntityを呼び、現在位置を解決する。

### EntitySlot

**役割**

indexから参照する管理表の要素。generationとFree／PendingCreate／Aliveの状態を保持し、Entityの現在の保存位置を対応付ける。

```cpp
namespace Hestia
{
    enum class EntitySlotState
    {
        Free,
        PendingCreate,
        Alive,
    };

    struct EntitySlot
    {
        uint32_t m_generation;
        EntitySlotState m_state = EntitySlotState::Free;

        Archetype* m_archetype = nullptr;
        Chunk* m_chunk = nullptr;
        size_t m_row = 0;
    };
}
```

[Archetype](./Archetype.md#archetype)  
[Chunk](./Chunk.md#chunk)  
[EntityHandle](#entityhandle)

Archetype／Chunkへのポインタは非所有。Slotはactiveやname・tag・layerを重複して保持しない。[ECSWorld](./ECSWorld.md#ecsworld)がSlot表と再利用indexの一覧を所有する。

```text
EntityHandle { index, generation }
  ↓ indexで管理表を参照
EntitySlot
  ├─ 状態とgenerationで予約・生存を確認
  └─ Archetype・Chunk・row
       ↓
ChunkのEntity列[row] → Entity実体・active
Chunkの各Component列[row] → name・tag・layerなど
```

PendingCreateはHandleを予約済みで、ChunkにEntity実体がまだ存在しない状態。Archetype／Chunkはnullptrのままにし、rowを参照しない。GetEntityはnullptrを返すが、そのHandleへのAddProcess／RemoveProcessは生成後に反映できる。生成が完了した時点でAliveへ移す。生成待ちでDestroyEntityを要求された場合は生成を取り消し、Entity／Componentを構築せずにFreeへ戻してgenerationを更新する。旧Handleを持つ保留要求は反映しない。

indexとrowは別の値。indexは識別番号として維持し、rowはChunk内の保存位置として移動時に更新する。

破棄予約時点ではSlotとHandleは有効なまま維持する。フレーム末尾の実破棄時にSlotを無効にしてgenerationを更新し、indexを再利用候補に戻す。再利用されたSlotに古いHandleを使っても、generationの不一致で検出する。無効値、generationの周回時の扱いは未確定。

## 全Entity共通のComponent

Name・Tag・Layerは通常のComponentとしてComponentRegistryへ登録する。Entity生成時に付与し、すべてのArchetypeのSignatureとLayoutに含める。初期activeはON、Nameはindexを十進表記した`Entity{index}`（indexが12なら`Entity12`）、Tag・Layerは0相当で初期化する。

処理関数が要求しなくても、その列はChunkに存在する。要求するかどうかで変わるのは処理関数へ渡す列であり、保存の有無ではない。これらの存在要求だけでは対象Archetypeを絞れない。

### Name

**役割**

Entityの名前を保持する。

```cpp
namespace Hestia
{
    struct Name
    {
        std::string m_value;
    };
}
```

[ComponentRegistry](./ComponentRegistry.md#componentregistry)

Chunkにはstringオブジェクト本体を置く。文字列の外部確保領域までChunk内で連続することは保証しない。登録された非自明な構築・Move・破棄を通して扱う。

### Tag

**役割**

Entityのtagを保持する。TagIdは表現未確定の仮の型名。

```cpp
namespace Hestia
{
    struct Tag
    {
        TagId m_value;
    };
}
```

[ComponentRegistry](./ComponentRegistry.md#componentregistry)

初期値は0相当。単一／複数tagとIDの割り当ては未確定。

### Layer

**役割**

Entityのlayerを保持する。LayerValueは表現未確定の仮の型名。

```cpp
namespace Hestia
{
    struct Layer
    {
        LayerValue m_value;
    };
}
```

[ComponentRegistry](./ComponentRegistry.md#componentregistry)

単一layerのIDにするか複数所属のMaskにするか、種類数は未確定。初期値は0相当とする。確定しているのは独立したComponentとして保存すること。

## ChunkとECSProcessからの利用

```text
Chunk
├─ Entity列    [ Handle用の識別情報・active ]
├─ Name列      [ 名前 ]
├─ Tag列       [ tag ]
├─ Layer列     [ layer ]
└─ 追加のComponent列
```

各列は同じrowで対応する。Name・Tag・Layerも固定バイト数からの容量計算に含める。

処理関数の先頭引数は有効行数、その次はEntity列、その後がQueryで要求するComponent列という方向に揃える。

```cpp
void Update(
    size_t count,
    const Entity* entities,
    Transform* transforms)
{
    for (size_t i = 0; i < count; ++i)
    {
        if (!entities[i].IsActive())
        {
            continue;
        }

        // transforms[i]を処理
    }
}
```

[Transform](./Component.md#transform)  
[ECSProcess](./ECSProcess.md#ecsprocess)  
[ECSProcessExecution](./ECSProcessExecution.md)

Entity列は全Chunk共通の引数とし、Component条件やComponent列番号の並びには含めない。countは構築済みの行数であり、activeな行だけの件数ではない。IsActiveは破棄予約も確認するため、予約済みの行もこの判定でスキップする。実行中の関数自体を中断するものではない。判定を省いた関数は非active行や破棄予約済みの行も処理してしまうため、標準の更新・描画処理で判定を揃える。

## 移動と参照の有効期間

Archetype間移動やSwap Removeでは、Entityの識別情報・active・破棄予約をComponentとともに移し、該当Slotの保存位置を更新する。Handleの値は変更しない。

Entity／Componentの非所有参照は構造変更をまたいで保持しない。外部ではHandleを保持し、必要な時点でEntityを取得する。System走査中はEntity列を直接参照し、行ごとにSlotを検索しない。

## 未確定事項

- GetEntityで取得した非所有参照と、構造変更を反映するタイミングの具体的な契約。
- 当面は1World内で識別する。複数Worldを扱う場合の識別は未確定。
- 生成時に既定値以外を指定するAPI。Name・Tag・Layerは追加・削除で除去しない。
- 行移動・参照更新の具体手順。追加・削除の反映時点とフレーム末尾の破棄は[ECSWorld](./ECSWorld.md)に従う。
- 保存データ上のEntity識別。実行時Handleの値をそのまま永続IDにするとは決めない。
