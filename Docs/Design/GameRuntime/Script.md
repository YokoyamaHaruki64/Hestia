# Sparse Script
---
通常の C++ 型で書いた Script を Entity に関連付け、GameRuntime が Game.dll 内で所有・実行する。初版は `.h/.cpp` と明示的な型登録で構成する。`.cscript` とコード生成は、手書きの型登録を置き換える段階で扱う。

[Game.dll の境界](./GameDLL.md) ・ [World](./World.md) ・ [GameRuntime の原案](../../../Reference/HestiaDesign/DevelopNotes/Class/GameRuntime.md) ・ [ECS Plan2 の Sparse Script Storage](../../../Reference/HestiaDesign/ECS/ECSArchitecture_Plan2.md#sparse-script-storage)

## 型登録と所有
---

Script は継承や仮想関数を持たない具象型として書く。`RegisterScript<T>()` で `requires` を使い、呼び出し可能なコールバックをコンパイル時に検出する。各コールバックは `void Update(Hestia::EntityHandle self)` と同じ形式で、所属 Entity の Handle を値で受け取る。GameRuntime は Storage の管理情報から `self` を渡す。Script は `self` を使って `HestiaGame::World` から Facade や Script を取得できる。時間が必要な Script は `HestiaGame::Time` を使う。

GameRuntime は登録型ごとの `ScriptStorage<T>` と、各コールバックの型単位の呼び出し口を持つ。更新 phase では対応する関数を持つ型の Storage を呼び、走査中は `T` のメソッドを直接呼ぶ。GameRuntime と Script の実体は Game.dll 内で完結し、Engine は phase 単位で GameRuntime を呼ぶ。

```text
ScriptStorage<Enemy>
├─ DenseScripts  std::vector<Enemy>
├─ DenseStates   ScriptState<Enemy> の配列
└─ Sparse        EntityIndex → Generation、DenseIndex
```

- Script 本体と管理情報は同じ DenseIndex で対応する。`enabled` の初期値は `true`。追加時に所属 Entity の Active を読み、以降は ECS から受け取った変更を管理情報に反映する。各 Script 型はこれらのフィールドを定義しなくてよい。
- 同じ Entity に同型の Script は一つ。Sparse Index は Entity の Generation も照合する。Script 本体への生ポインタや参照は Storage の構造変更まで借用できる。長期保持には `ScriptRef<T>` を使う。
- 削除は Swap Remove を使う。末尾要素を削除位置に移すときは本体と管理情報を一緒に移し、移動した Entity の Sparse Index を更新する。配列の並びは削除で変わるため、Script の実行順序の指定には使わない。

## ScriptRef と参照先の所有
---

`ScriptRef<T>` は、移動する Script 本体を継続して参照する値型。Script インスタンスごとに、アドレスが固定された `ScriptReference<T>` を一つ確保する。Storage と発行済みの `ScriptRef<T>` がそれを共有する。Script 本体は引き続き `std::vector<T>` に値として格納する。

| 型 | 内部メンバ | 役割 |
| --- | --- | --- |
| `ScriptState<T>` | `EntityHandle m_entity`、`bool m_enabled`、`bool m_entityActive`、`ScriptReference<T>* m_reference` | Script の有効設定と受け取った Entity の Active 設定を保持し、Storage 側の参照を一つ所有する |
| `ScriptReference<T>` | `T* m_script`、整数の `m_refCount` | Script 本体の現在のアドレスと、自身を所有する `ScriptRef<T>` の数を持つ。`m_script` は非所有 |
| `ScriptRef<T>` | `ScriptReference<T>* m_reference` | 参照先を一つ所有し、値としてコピー・ムーブできる。空の値では `m_reference` が `nullptr` |

`World::GetScript<T>(entity)` と `World::AddScript<T>(entity, ...)` は `ScriptRef<T>` を値で返す。取得を繰り返しても、同じインスタンスに対しては `ScriptState<T>` が持つ同じ参照先を共有する。対象が見つからない場合や追加に失敗した場合は空の値を返す。

- **コピー・破棄**。コピーで参照カウントを増やし、ムーブでは参照の所有権を移す。破棄でカウントを減らし、ゼロで `ScriptReference<T>` を解放する。初版は同一 GameRuntime スレッドで扱うため、カウントは通常の整数とする。カウントの型は内部実装に閉じる。
- **実体の移動**。Swap Remove では末尾から移った Script の `m_script` を更新する。Dense 配列の再確保では移動した全要素の `m_script` を更新する。保留領域から Dense 配列へ移すときも、同じ参照先を維持して新しいアドレスを設定する。
- **実体の削除**。削除予約時に `m_script` を `nullptr` にする。Storage は `OnDestroy` を実体に直接呼び、走査後に `ScriptState<T>` を除去する。その時点で Storage の参照が外れ、最後の `ScriptRef<T>` が破棄されたときに参照先も解放される。同じ Entity に同型の Script を付け直すと、新しいインスタンス用の参照先を作る。

`explicit operator bool()` は `m_reference` と `m_script` の両方を確認する。`operator->()` も内部で null を確認し、有効なら `T*` を返す。無効な参照への `->` は契約違反として停止する。どちらの演算子も参照カウントの変更や Sparse Index の検索を行わない。

```cpp
if (m_health)
    m_health->ApplyDamage(damage);
```

## コールバック
---

| コールバック | 発火条件 |
| --- | --- |
| `OnStart` | 追加時に一度だけ呼ぶ。追加時の `OnEnable` があればその後に呼ぶ。非 Active の Entity に追加した場合も呼ぶ |
| `FixedUpdate` / `Update` / `LateUpdate` | 対応する ECS phase の直後に呼ぶ。Script が有効で、所属 Entity が Active のときに実行する |
| `OnEnable` / `OnDisable` | `Entity の Active && Script の enabled` が `false → true` で `OnEnable`、`true → false` で `OnDisable`。Active な Entity への追加時も `OnEnable` を呼ぶ |
| `OnDestroy` | Script の削除要求時、Entity の破棄前、または GameRuntime の終了時に、Script 実体ごとに一度だけ呼ぶ |

Script の `enabled` と Entity の Active 設定は独立している。enabled のまま Entity を非 Active にすると `OnDisable`、Active に戻すと `OnEnable` を呼ぶ。非 Active の Entity 上で `enabled` を切り替えた場合と、disabled の Script を持つ Entity の Active を切り替えた場合は通知しない。追加時の `OnEnable` で Script が削除された場合は `OnStart` を省く。

## 走査中の追加・削除
---

走査中は既存の Dense 配列を維持し、Script の追加・削除に伴うコールバックを要求時に実行する。構造変更はフレーム末尾にまとめて反映する。

1. **追加**。保留領域に Script 実体と管理情報を作り、Entity が Active なら `OnEnable` を呼ぶ。その後に `OnStart` を呼ぶ。フレーム末尾に本体と管理情報を Dense 配列に追加し、Sparse Index に登録する。
2. **削除**。Script を削除予定にして以降の更新対象から外し、その場で `OnDestroy` を一度だけ呼ぶ。実体と Sparse Index の除去はフレーム末尾に Swap Remove で行う。追加を反映する前に削除した場合は、保留実体に `OnDestroy` を呼んで追加を取り消す。

実行中に追加された Script が更新対象になるのは、**次の App フレーム**から。同一フレーム内に残る `FixedUpdate`、`Update`、`LateUpdate` はスキップし、GameRuntime の `EndFrame` で次フレームの対象を確定する。起動時の初期配置は最初のフレームの前に反映する。 [App のフレームループ](../../../Application/Source/Application.cpp) ・ [GameRuntime の更新順](./GameDLL.md#起動更新終了)

## Entity との寿命の同期
---

1. **呼び出し直前**。GameRuntime は `EntityHandle` を使って Engine 側に `IsActive` を問い合わせる。Engine は Handle を解決し、`Entity::IsActive()` に従って判定する。破棄予約された Entity はこの時点から更新対象外になる。
2. **描画後、ECS の実体破棄前**。GameRuntime は ECS の破棄予約を `EntityHandle` 単位で受け取り、各型の Sparse Index から所属 Script を引く。`OnDestroy` を呼び、Swap Remove で取り除く。`OnDestroy` の実行中に追加された Entity 破棄予約も、ECS が実体を破棄する前に同じ手順で処理する。
3. **GameRuntime の終了時**。残っている Script の `OnDestroy` と Storage の解放を済ませ、その後に Facade を Unbind する。

GameRuntime は初期配置前に ECS の Active 設定変更の通知先を登録し、終了時に解除する。受け取った変更は順に各 Script へ届け、有効状態が変わった場合に `OnEnable` または `OnDisable` を呼ぶ。非 Active の Entity も Script を保持し、再び Active になれば有効な Script の更新を再開する。破棄予約後は更新を止め、フレーム末尾に `OnDestroy` を呼ぶ。 [Entity のフラグ](../../../Engine/Include/ECS/Entity.h) ・ [ECS の破棄予約と EndFrame](../../Implementation/ECS/README.md)

## 初版の確認項目
---

- コールバックの一部だけを定義した型を登録し、定義した関数が対応するタイミングに呼ばれる。
- コールバックの `self` が所属 Entity を指し、`World` でその Entity の Facade を取得できる。
- Script の有効状態と Entity の Active 設定を個別に切り替え、対応する通知、更新の停止・再開、`OnStart` の一回性を確認する。
- `FixedUpdate` 中に追加した Script が同一フレームの後続 phase で更新されず、次のフレームで更新される。走査中の削除では `OnDestroy` が要求時に一度だけ発火し、Swap Remove で移動した Script を元の EntityHandle から引ける。
- 保持した `ScriptRef<T>` が Swap Remove と配列の再確保後も同じ実体を指す。削除予約後は無効になり、同型を再追加しても元の参照は無効のまま。最後の参照が消えた時点で参照先を解放する。
- Script を持つ Entity とその親子を破棄予約し、以降の Script 更新をスキップし、Entity の実体破棄前に所属 Script を除去する。
