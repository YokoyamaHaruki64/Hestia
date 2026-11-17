# World
---
`HestiaGame::World` は、ゲームコードから現在のゲーム世界を操作する入口とする。初版では Entity の生成・検索と、Script や Process に対応する操作用 Facade を扱う。Game 側では Unity の Component のように Entity 単位で操作し、Engine 側では ECS が Component の配置と Process の実行を管理する。

[Game.dll の境界](./GameDLL.md) ・ [Script の所有と更新](./Script.md) ・ [現行の ECS](../../Implementation/ECS/README.md)

## Entity の受け渡し
---

- Entity の識別には `Hestia::EntityHandle` を値で使う。Script の全コールバックは所属 Entity の Handle を `self` として受け取る。Handle の正当性は、操作を受ける Engine または Script Storage が Index と Generation で確認する。
- Entity の状態と配置は ECS が所有する。Game 側は `GameEngineAPI` の `EntityAPI` を通して、Handle に対応する状態を Engine から取得する。`IsActive` は呼び出した時点の状態を読み、破棄予約済みなら `false` を返す。DLL 境界に渡すのは Handle と各操作に必要な値で、`EntitySlot` や Chunk の配置は Engine 内部に置く。
- Active 設定の変更は Engine から GameRuntime に通知する。`Entity の Active && Script の enabled` が変化した場合に Script の `OnEnable` または `OnDisable` を呼ぶ。破棄予約後は Script の更新を止め、ECS が Entity の実体を破棄する前に GameRuntime が所属 Script を除去する。

[現行の Entity と Slot](../../../Engine/Include/ECS/Entity.h) ・ [Script と Entity の寿命](./Script.md#entity-との寿命の同期)

## World の操作
---

| 用途 | Game 向け API 案 | 動作 |
| --- | --- | --- |
| 生成・破棄 | `CreateEntity()` / `DestroyEntity(entity)` | 生成した Entity の Handle を返す。破棄は ECS に予約する |
| Active 状態 | `IsActive(entity)` / `SetActive(entity, active)` | 現在の実行可否を読み、Active 設定を変更する |
| 名前・タグ | `SetName(entity, name)` / `SetTag(entity, tag)` | Engine が持つ Name と Tag を設定する |
| 名前・タグで検索 | `FindFirstEntityByName(name)` / `FindEntitiesByTag(tag)` | 名前が一致する一件、またはタグが一致する全件を返す |
| Script の操作 | `GetScript<T>(entity)` / `AddScript<T>(entity, ...)` / `RemoveScript<T>(entity)` | GameRuntime が型ごとの Storage を操作する。取得と追加は `ScriptRef<T>` を返す |
| Script の有効設定 | `SetScriptEnabled<T>(entity, enabled)` | Script の `enabled` を変更する。Entity が Active な場合は変更に応じて `OnEnable` または `OnDisable` を呼ぶ |
| Script で検索 | `FindEntitiesWithScript<T>()` | `T` の Script を保持する Entity を返す |
| Process 用 Facade | `GetProcess<T>(entity)` / `AddProcess<T>(entity)` | 操作用 Facade を取得し、対応する Process に必要な Component を補う |
| Process で検索 | `FindEntitiesWithProcess<T>()` | 対応する Process の Component 条件を満たす Entity を返す |

検索結果は `EntityHandle` で返す。検索には非 Active の Entity も含め、生成待ちと破棄予約済みは除外する。名前検索は同名の Entity のうち一件を返し、同名間の順序は規定しない。タグは現行 ECS の `Tag` と同じ数値 ID を使う。 [現行の Name と Tag](../../../Engine/Include/ECS/Component.h)

`GetScript<T>` と `AddScript<T>` は `ScriptRef<T>` を値で返す。対象が見つからない場合や追加に失敗した場合は空の値を返す。保持した参照は Storage 内での実体の移動に追従し、Script の削除予約時に無効になる。走査中の追加は走査後に反映し、追加した Script の更新は次のフレームから始まる。 [ScriptRef の契約](./Script.md#scriptref-と参照先の所有) ・ [追加・削除の契約](./Script.md#走査中の追加削除)

## Process と操作用 Facade
---

`GetProcess<T>` の `T` は Game 向け Facade 型。既存の `HestiaGame::Transform` のように `EntityHandle` を保持する値として返し、`IsValid()` で Entity が生存し、操作に必要な Component が揃っているかを確認する。Facade は操作のたびに Handle から現在の対象を解決するので、Entity が Archetype を移っても同じ Facade を使える。 [現行の Transform Facade](../../Implementation/ECSProcess/Transform.md#hestiagametransform)

Facade は Script のメンバとして保持できる。Entity が破棄予約された場合や、必要な Component が外れた場合は無効になる。同じ Entity に必要な Component が揃えば、保持していた Facade を再び使える。Entity の破棄後に Slot が再利用された場合は、Generation の照合で古い Handle を拒否する。

`AddProcess<T>` は Facade 型に対応する登録済み Process を Engine で解決し、不足している要求 Component を追加する。既存の値は維持する。Transform のように生成時から要求 Component が揃っている場合は、生成直後に Facade を使える。追加が ECS で保留された場合は、構成が反映された時点で Facade が有効になる。 [現行の AddProcess](../../Implementation/ECS/README.md)

`FindEntitiesWithProcess<T>()` は現在の Component 構成で判定する。Process の要求が揃えば、別の操作で構成された Entity も検索対象に入る。ECS は既存どおり Component 条件で Process の対象を決める。

```cpp
void PlayerMove::OnStart(Hestia::EntityHandle self)
{
    m_transform = HestiaGame::World::GetProcess<HestiaGame::Transform>(self);
}

void PlayerMove::Update(Hestia::EntityHandle)
{
    m_transform.SetLocalPosition(m_position);
}
```

`World` の Script 操作は GameRuntime に委譲する。Entity 操作と Process の構成変更、Facade による Component 操作は、`GameEngineAPI` から受け取った関数ポインタ経由で Engine が行う。`World` は Facade の `API` 型から、操作テーブルと Process Handle の組を取得する。 [Engine と Game の API 構成](./APIArchitecture.md)

## 最初のプレイアブルでの確認
---

1. Game.dll から Entity を生成し、MeshRenderer に対応する Facade で Mesh と Material を設定する。
2. 操作用 Script が Input と Time を読み、`GetProcess<Transform>(self)` で取得した Facade を使って Entity を動かす。
3. Entity を破棄し、描画と Script の更新対象から外れることを確認する。

この経路を通すために必要な MeshRenderer、入力、アセット指定の操作を具体化し、その先はゲームの要件に合わせて Process と Facade を追加する。
