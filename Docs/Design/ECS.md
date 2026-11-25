# ECS の設計

Engine は `ECSSystem` を所有し、`ECSSystem` は Component の登録情報と単一の `ECSWorld` を所有する。World は Entity、Archetype、Process を管理する。Game の Script は GameRuntime が所有し、同じ Entity を Handle で参照する。

## Entity と配置

- `EntityHandle` は Index と Generation を持つ。World は Index から Slot を引き、Generation を照合して現在の Archetype、Chunk、行を解決する。行が移動したら Slot を更新し、Slot を再利用したときは世代照合で古い Handle を拒否する。
- Archetype は同じ Component 型の集合を持つ Entity をまとめる。Chunk 内で Entity と型ごとの Component をそれぞれ連続した列に置き、同じ行番号で対応させる。Component のアドレスを借用できるのは構造変更まで。長期保持には Handle を使う。
- Entity は `Name`、`Tag`、`Layer` と [Transform を構成する三つの Component](Transform.md) を持つ。生成に成功すると Active になり、移行時も必須 Component と既存値を保持する。
- `CreateEntity()` は必須 Component の構成を使う。`CreateEntity(componentTypes)` は必須型を含む全 Component の型 ID を受け取り、欠けている必須型または未登録の型があれば生成を拒否する。どちらも構成に対応する Archetype の Chunk に直接生成する。Prefab や Scene を復元するときは、保存された型名を Registry で ID に解決してから渡す。初期値と親子関係は生成後に復元する。 [Componentのシリアライズ](Serialize/ComponentSerialization.md)

### Archetypeクエリ

`ECSWorld::QueryArchetypes(std::span<const ComponentTypeId> required)` は、指定されたComponentをすべて含むArchetypeを `std::vector<const Archetype*>` として返す。`ECSSystem` はEngine内部からクエリを呼ぶ入口を提供する。照合はArchetypeの署名を対象とし、各Entityの状態による絞り込みは呼び出し側で行う。

ArchetypeとChunkはWorldが所有する。呼び出し側は構造変更のない区間で結果を使い切る。Scene保存では `TransformHierarchy` を持つArchetypeのChunkを順に読み、親を持たないEntityのHandleを集める。 [Sceneの保存・読み込み](Serialize/SceneSerialization.md)

## Process と構造変更

Process は phase と関数を初期化時に登録する。関数は `count`、Entity 列、要求する Component 列を受け取る。要求 Component をすべて含む Archetype が対象になり、同じ phase では登録順に Chunk 単位で呼ぶ。Active な Entity だけを処理する Process は、各行の `IsActive()` を確認する。

`AddProcess` は不足する要求 Component を補う。ECS は Component 構成から実行対象を決めるため、構成が同じならどの入口で追加したかにかかわらず対象になる。Engine 内部の `RemoveProcess` では必須 Component と、変更前の構成で一致する他の Process が使う Component を維持する。構成が変わらなければ削除対象の Process にも一致し続ける。

Process 実行中は生成と Component 構成変更を予約し、その Process が全 Chunk の走査を終えてから反映する。Archetype を移すときは新規 Component を先に構築し、成功したら既存値を移して Slot を切り替える。失敗時は元の行と Slot を維持する。

## 破棄と Game との受け渡し

生成待ちの Entity に対する破棄は生成を取り消す。生成済みの Entity に対しては子孫も含めて破棄を予約し、`IsActive()` はすぐに `false` を返す。描画後に GameRuntime が所属 Script を除去してから ECS が実体を破棄し、移動した行の Slot を更新する。

Game は `World` から Handle と値で操作する。公開する Process だけを型付きの API と Facade に結び付ける。 [World の設計](GameRuntime/World.md) ・ [公開 Process の設計](GameRuntime/APIArchitecture.md)

[ECS の実装仕様](../Implementation/ECS/README.md) ・ [ECS の開発手引き](../Implementation/ECS/DevelopmentGuide.md)
