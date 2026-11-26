# Sparse Script
---
通常の C++ 型で書いた Script を Entity に関連付け、GameRuntime が Game.dll 内で所有・実行する。`Assets/` 内の登録済み `.cpp` から Script の C++ ヘッダー・ソースと `RegisterScripts()` を生成する。Scene は `.cpp.meta` の GUID で型を参照する。 [C++ Script のコード生成](CppScript.md)

[Game.dll の境界](./GameDLL.md) ・ [World](./World.md) ・ [GameRuntime の実装仕様](../../Implementation/GameRuntime/README.md)

## 型登録と所有
---

Script の具象型は Game プロジェクトでビルドする。生成した Setup は `World::RegisterScript<T>(guid)` を呼び、GameCore は GameRuntime 初期化時の型登録中だけ登録を受け付ける。登録には有効かつ一意のGUIDを必須とし、登録後は `World::AddScript<T>()` でアタッチできる。Storage は `requires` で `script.Update(self)` の式が有効かをコンパイル時に確認する。各コールバックは省略でき、呼び出し結果は使わない。`self` は所属 Entity の Handle で、Script は `World` から Facade や Script を取得できる。時間は `HestiaGame::Time` から取得する。

GameRuntime は `typeid(T)` と GUID を同じ Storage の添字に結び付け、更新時は登録順に Storage を呼ぶ。Scene復元ではGUIDでStorageを引き、`IScriptStorage::AddDefault()` で具象Scriptを生成する。`IScriptStorage` による仮想呼び出しは Storage 単位で行い、ページ走査では `T` のメソッドを直接呼ぶ。Script 本体は Game.dll が所有し、Engine は phase 単位で GameRuntime を呼ぶ。

```text
ScriptStorage<Enemy>
├─ Pages         固定容量ページの所有ポインタ列
│  ├─ Scripts  std::vector<Enemy>
│  └─ States   std::vector<ScriptState<Enemy>>
└─ Sparse        EntityIndex → Generation、DenseIndex
```

- Script 本体と管理情報は同じ DenseIndex で対応する。一ページは128要素分を確保し、容量が必要になったときだけ新しいページを追加する。既存ページの Script は移動しない。`enabled` の初期値は `true` で、各 Script 型に状態フィールドを定義する必要はない。
- 同じ Entity に同型の Script は一つ。Sparse Index は Entity の Generation も照合する。Script 本体への生ポインタや参照は Storage の構造変更まで借用できる。長期保持には `ScriptRef<T>` を使う。
- 削除はページをまたいで Swap Remove する。末尾要素を削除位置に移し、管理情報と Sparse Index を更新する。Script はコピー構築または noexcept ムーブ構築ができ、ムーブかコピー代入が必要。順序は削除で変わる。

## ScriptRef と参照先の所有
---

`ScriptRef<T>` は、移動する Script 本体を継続して参照する値型。Script ごとにアドレス固定の `ScriptReference<T>` を一つ確保し、Storage と発行済みの参照が共有する。

| 型 | 内部メンバ | 役割 |
| --- | --- | --- |
| `ScriptState<T>` | `EntityHandle m_entity`、`bool m_enabled`、`bool m_entityActive`、`bool m_removed`、`ScriptReference<T>* m_reference`、`std::size_t m_attachment` | 有効状態、削除予約、追加順を保持し、Storage 参照を一つ所有する |
| `ScriptReference<T>` | `T* m_script`、整数の `m_refCount` | Script 本体の現在アドレスを非所有で保持し、所有参照を数える |
| `ScriptRef<T>` | `ScriptReference<T>* m_reference` | 参照先を一つ所有し、値としてコピー・ムーブできる。空の値では `m_reference` が `nullptr` |

`World::GetScript<T>(entity)` と `World::AddScript<T>(entity, ...)` は `ScriptRef<T>` を値で返す。取得を繰り返しても、同じインスタンスに対しては `ScriptState<T>` が持つ同じ参照先を共有する。対象が見つからない場合や追加に失敗した場合は空の値を返す。

- **コピー・破棄**。コピーで参照カウントを増やし、ムーブでは参照の所有権を移す。破棄でカウントを減らし、ゼロで `ScriptReference<T>` を解放する。初版は同一 GameRuntime スレッドで扱うため、カウントは通常の整数とする。カウントの型は内部実装に閉じる。
- **実体の移動**。保留領域からページへ移すときとSwap Removeで同じ参照先の `m_script` を新しいアドレスに更新する。
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

1. **追加**。保留領域に Script と状態を作り、`ScriptRef` を発行する。Active なら `OnEnable`、続いて削除されていなければ `OnStart` を呼ぶ。フレーム末尾にページへ移し Sparse Index に登録する。
2. **削除**。削除対象 Handle をキューに積み、参照を無効化し、その場で `OnDestroy` を一度呼ぶ。実体と Sparse Index はフレーム末尾に Swap Remove で除去する。

実行中に追加された Script が更新対象になるのは、**次の App フレーム**から。同一フレーム内に残る `FixedUpdate`、`Update`、`LateUpdate` はスキップし、GameRuntime の `EndFrame` で次フレームの対象を確定する。起動時の初期配置は最初のフレームの前に反映する。 [App のフレームループ](../../../Application/Source/Application.cpp) ・ [GameRuntime の更新順](./GameDLL.md#起動更新終了)

## Entity との寿命の同期
---

1. **呼び出し直前**。GameRuntime は `EntityHandle` を使って Engine 側に `IsActive` を問い合わせる。Engine は Handle を解決し、`Entity::IsActive()` に従って判定する。破棄予約された Entity はこの時点から更新対象外になる。
2. **描画後、ECS の実体破棄前**。GameRuntime は ECS の破棄予約を `EntityHandle` 単位で受け取り、各型の Sparse Index から所属 Script を引く。`OnDestroy` を呼び、Swap Remove で取り除く。`OnDestroy` の実行中に追加された Entity 破棄予約も、ECS が実体を破棄する前に同じ手順で処理する。
3. **GameRuntime の終了時**。残っている Script の `OnDestroy` と Storage の解放を済ませ、その後に Facade を Unbind する。

GameRuntime は初期配置前に ECS の Active 設定変更の通知先を登録し、終了時に解除する。受け取った変更は順に各 Script へ届け、有効状態が変わった場合に `OnEnable` または `OnDisable` を呼ぶ。非 Active の Entity も Script を保持し、再び Active になれば有効な Script の更新を再開する。破棄予約後は更新を止め、フレーム末尾に `OnDestroy` を呼ぶ。 [Entity のフラグ](../../../Engine/Include/ECS/Entity.h) ・ [ECS の破棄予約と EndFrame](../../Implementation/ECS/README.md)

## Scene保存用の逆引き

GameRuntimeは `EntityHandle → Storageインデックス列` を `std::unordered_map` で持つ。HandleのハッシュにはIndexとGenerationを使う。値には登録済みStorageの `std::size_t` インデックスを格納し、GUIDはStorageの登録情報から引く。

索引は `GameRuntime::FlushScripts()` が各Storageの `Flush()` を呼び、Scriptの追加・削除を反映したときに更新する。追加の反映に成功した時点でStorageインデックスを加え、削除を反映した時点で取り除く。最後のScriptが外れたEntityの項目は消す。Scene切り替えでScript実体を全削除するときも索引を空にする。

Engine側の `QueryScriptAttachments` は `std::span<const EntityHandle>` で対象Handle列を受け取り、成功時は `std::unordered_map<EntityHandle, std::vector<AssetGuid>, EntityHandleHash>` を返す。入力には `std::vector<EntityHandle>` を渡せる。Scriptが付いているEntityだけをmapに入れ、値にはそのEntityに付いているScript型のGUIDを格納する。

EngineとGameRuntimeの境界ではHandle列をポインタと件数で渡す。GameRuntimeは対象Handleの逆引き索引を引き、`{EntityHandle, AssetGuid}` をEngine側のコールバックへ渡す。Engineが結果のmapを組み立て、その所有権を呼び出し側へ渡す。Scene保存はフレーム末尾の反映後にこの照会を行う。 [Scene保存の手順](../Serialize/SceneSerialization.md#worldからの保存)

## 初版の確認項目
---

- コールバックの一部だけを定義した型を登録し、定義した関数が対応するタイミングに呼ばれる。
- コールバックの `self` が所属 Entity を指し、`World` でその Entity の Facade を取得できる。
- Script の有効状態と Entity の Active 設定を個別に切り替え、対応する通知、更新の停止・再開、`OnStart` の一回性を確認する。
- `FixedUpdate` 中に追加した Script が同一フレームの後続 phase で更新されず、次のフレームで更新される。走査中の削除では `OnDestroy` が要求時に一度だけ発火し、Swap Remove で移動した Script を元の EntityHandle から引ける。
- 保持した `ScriptRef<T>` がページ追加とページをまたぐ Swap Remove 後も同じ実体を指す。削除予約後は無効になり、同型を再追加しても元の参照は無効のまま。
- Script を持つ Entity とその親子を破棄予約し、以降の Script 更新をスキップし、Entity の実体破棄前に所属 Script を除去する。
