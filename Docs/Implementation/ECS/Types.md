# ECSの型

[処理・構造](README.md) / [Process型](Process.md)

## 識別と標準Component

| 型 | データ・契約 | 定義 |
| --- | --- | --- |
| `EntityHandle` | uint32 indexとgeneration。indexは下位30bitのみ、generation 0は無効。生成元World内の識別子でWorld識別を内包しない | [Entity.h](../../../Engine/Include/ECS/Entity.h) |
| `Entity` | index30bit、active/破棄予約の各1bit、generation32bit。合計8byte。IsActiveはactiveかつ未破棄予約を意味する | 同上 |
| `EntitySlotState` | Free / PendingCreate / Alive。破棄予約はEntity側のbitで表す | 同上 |
| `EntitySlot` | generation、状態、Archetype*、Chunk*、row。現在位置を表すWorld内部データ | 同上 |
| `Name` | std::stringのm_value。必須Component | [Component.h](../../../Engine/Include/ECS/Component.h) |
| `Tag` | uint32のm_value、既定0。必須Component | 同上 |
| `Layer` | uint32のm_value、既定0。必須Component。衝突マスク等の用途を実装以上に規定しない | 同上 |

## 型登録と配置

| 型 | データ・契約 | 定義 |
| --- | --- | --- |
| `ComponentTypeId` | uint64の型ID。明示したUTF-8登録名をFNV-1a 64bitでハッシュ化し、名前変更後の互換性は保証しない | [ComponentRegistry.h](../../../Engine/Include/ECS/ComponentRegistry.h) |
| `ComponentInfo` | ID、名前、size、alignment、既定構築・破棄・Move構築の関数ポインタ | 同上 |
| `ComponentRegistry` | ID→情報とC++ type_index→IDを所有。同型同名の再登録は同じID、別名・別型・ID衝突は拒否 | 同上 |
| `ArchetypeSignature` | 昇順の型ID vector。同一構成を識別 | [Archetype.h](../../../Engine/Include/ECS/Archetype.h) |
| `ComponentLayout` | 型ID、列のbyte offset、Registry内ComponentInfoへの参照 | 同上 |
| `ChunkLayout` | byteサイズ、最大row数、全体alignment、各列Layout | 同上 |
| `Archetype` | signature、共通Layout、Chunk群を所有し、空きChunkの選択と列番号検索を担当 | 同上 |
| `Chunk` | 固定byte領域を所有しEntityとComponentをSoA配置。構築、Move、破棄、行数の確定をWorldから利用する | [Chunk.h](../../../Engine/Include/ECS/Chunk.h) |

Registryが受理するComponentは既定構築可能、Move構築と破棄がnoexceptの型。const/volatile修飾した型の登録は受け付けない。GetIdは型のcvを除いて検索する。既定構築は失敗し得るため、Worldの移行処理は構築途中の後片付けも扱う。

## 統括

| 型 | データ・契約 | 定義 |
| --- | --- | --- |
| `ECSSettings` | Chunk byteサイズ。既定16×1024 | [ECSSystem.h](../../../Engine/Include/ECS/ECSSystem.h) |
| `ECSSystem` | Registryと単一Worldの所有者。初期化状態と登録期間を管理 | 同上 |
| `ECSWorld` | Registryを借用し、slot表、Archetype、Process、構造変更要求、破棄予約を所有 | [ECSWorld.h](../../../Engine/Include/ECS/ECSWorld.h) |
| `ECSWorld::ChangeKind` | Create / AddProcess / RemoveProcess / CancelCreateの内部要求区分 | 同上 |
| `ECSWorld::ChangeRequest` | 対象Handle、Process共通入口、要求区分 | 同上 |
| `ScopedFlag` | boolへの非所有参照と変更前の値。構築時にtrueへ設定し、スコープ終了時に元の値へ戻す内部RAII型 | [ECSWorld.cpp](../../../Engine/Source/ECS/ECSWorld.cpp)の無名名前空間 |
| `ScopeExit<Function>` | Releaseされなければスコープ終了時にnoexceptの後始末を実行する内部RAII型。ApplyCreateの部分構築の解放に使用 | 同上 |

System終了はProcess実行中、構造変更適用中、EndFrame処理中には拒否される。GetEntity/GetComponentのnullは未接続、無効Handle、生成待ち、Component不在等で起こる。実在をHandle.IsValidだけで判定しない。
