# Chunk
---
固定バイト数の保存領域にEntityとComponent列を保持するChunkのクラスモック。
Archetypeが決めたLayoutと、Chunkごとに変わる有効Entity数を分ける。宣言は概念設計用のAPIスケッチ。

## Chunk
---
**役割**

Entity列とComponent列の保存領域を所有し、有効な行数を保持する。Componentの寿命管理には登録済みの型操作を使用する。

```cpp
namespace Hestia
{
    class Chunk
    {
    public:
        explicit Chunk(const ChunkLayout& layout);
        ~Chunk();

        Chunk(const Chunk&) = delete;
        Chunk& operator=(const Chunk&) = delete;

        size_t GetEntityCount() const;
        size_t GetCapacity() const;

        std::span<const Entity> GetEntities() const;

        void* GetColumn(size_t columnIndex);
        const void* GetColumn(size_t columnIndex) const;

    private:
        const ChunkLayout* m_layout = nullptr;
        std::byte* m_data = nullptr;
        size_t m_entityCount = 0;
    };
}
```

[ChunkLayout](./Archetype.md#chunklayout)  
[ComponentLayout](./Archetype.md#componentlayout)  
[ComponentInfo](./ComponentRegistry.md#componentinfo)

Entity列には識別情報・active・破棄予約を持つ実体を配置する。[Entity／EntityHandle／EntitySlot](./Entity.md)にビット構成と参照方法をまとめる。

**所有・参照**：ArchetypeがChunkを所有する。Chunkは`m_data`の保存領域を所有し、同じArchetypeで共有するChunkLayoutを非所有で参照する。保存領域はLayoutのアラインメントに従って確保・解放する。Chunkの浅いコピーによる保存領域の共有は行わない。

**APIとメンバ**：

- `GetEntityCount()`は有効な行数を返す。Systemコールバックへ渡す件数はこの値を使う。
- `GetCapacity()`は共有Layoutの容量を返す。
- `GetEntities()`は有効な行に対応するEntity列を返す。
- `GetColumn(columnIndex)`はLayoutの列番号からComponent配列の先頭を返す。Component IDを列番号として直接使用しない。
- `m_entityCount`はChunkごとに異なる。共通の容量や列配置を変更する値ではない。

追加・削除APIはEntity位置の更新方法と合わせて後続で決める。このモックでは保存構造と列アクセスを先に示す。

## 保存領域
---
全ChunkにEntity列と[Name・Tag・Layer](./Entity.md#全entity共通のcomponent)の基本Component列を置く。以下はTransform／Renderer／MeshRendererの列配置に注目する図で、基本Component列は省略している。列の間には必要なPaddingを置き、容量計算には基本Componentも含める。

```text
ChunkLayout（Archetypeで共有する設計図）
  Entity列の先頭はm_dataに固定（Offsetは保持しない）
  m_components[t].m_offset     ──→ Transform列の先頭
  m_components[r].m_offset     ──→ Renderer列の先頭
  m_components[m].m_offset     ──→ MeshRenderer列の先頭

各Chunkのm_data（別々に確保する固定バイト数の領域）
  ┌──────────┬─────────┬─────────────┬─────────┬────────────┬─────────┬────────────────┬────────┐
  │ Entity列 │ Padding │ Transform列  │ Padding │ Renderer列 │ Padding │ MeshRenderer列 │ 余り   │
  └──────────┴─────────┴─────────────┴─────────┴────────────┴─────────┴────────────────┴────────┘
  ↑ m_data
  Entity列の先頭    = m_data
  Component列の先頭 = m_data + その列のOffset
```

[Transform / Renderer / MeshRenderer](./Component.md)

列順は説明用。t／r／mは各型に対応する列番号。実際のComponent列順はArchetypeのLayoutに従う。Paddingは配置条件に応じて入り、不要なら0バイトとなる。

例えば容量4・有効数2なら、各列の状態は次のようになる。数値は図のための例で、実際の容量は固定バイト数から計算する。

```text
添字             0       1       2       3
Entity列       [ E0  ][ E1  ][ 未使用 ][ 未使用 ]
Transform列    [ T0  ][ T1  ][ 未構築 ][ 未構築 ]
Renderer列     [ R0  ][ R1  ][ 未構築 ][ 未構築 ]
MeshRenderer列 [ M0  ][ M1  ][ 未構築 ][ 未構築 ]
                └─ 有効数2 ─┘
                └──────── 容量4 ────────────┘
```

同じ添字のComponentとEntityは同じ行に属する。各Component列は連続し、有効範囲は`[0, m_entityCount)`とする。残りの容量は未構築の領域として扱う。

Component列の先頭は`m_data + ComponentLayout::m_offset`から求める。列内の各要素のアラインメントを満たす配置はArchetype側で計算する。

## 型操作とSystemへの受け渡し
---
非自明なComponentはComponentInfoの構築・Move構築・破棄を通して扱う。Chunk破棄時は有効範囲のComponentを破棄してから保存領域を解放し、未構築の領域へデストラクタを呼ばない。

[ECSProcess](./ECSProcess.md#ecsprocess)が起動する処理関数には、有効行数・Entity列・要求されたComponent列を渡す。有効行数には非activeなEntityも含み、各処理関数がEntityのIsActiveを確認する。条件に合うArchetypeと列番号の対応はECSProcess側で保持し、登録・取得APIの詳細は後続で定める。

列ポインタを使用している間に行の追加・削除・移動を行うと、参照先の寿命や行対応が変わり得る。Component追加・削除は実行外なら即時、実行中なら現在のECSProcess::Run全体の終了後に[ECSWorld](./ECSWorld.md)が反映する。Entityの実破棄はフレーム末尾とする。Run中のEntity生成も予約だけ行い、現在のRun全体の終了後に反映する。

**未確定**：Swap Remove時の行移動、移動したEntityの位置更新、Archetype間移動、空Chunkの回収。これらのAPIや所有者は、この保存構造のモックでは確定しない。
