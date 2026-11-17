# GameRuntime の処理・構造

[全体索引](../README.md) / [型と API](Types.md) / [Process の登録と公開](../ECS/DevelopmentGuide.md#公開processのapiとfacade)

## DLL 接続と初期化

Editor・Game 構成では、Engine が App と同じディレクトリの `Game.dll` をロードし、`GetGameRuntimeAPI` を取得する。`GetGameRuntimeAPI` は Game.dll 内の `static const GameRuntimeAPI` を返す。`Test` 構成では、静的 `Game.lib` から同じ関数を直接呼ぶ。GameRuntime の生成・破棄は Game 側が行い、Engine は API テーブルと不透明なハンドルを持つ。 [Engine のロード処理](../../../Engine/Source/Engine.cpp) ・ [GameRuntimeAPI](../../../Common/Include/GameRuntimeAPI.h)

`Create` は `GameEngineAPI` と Time・Entity・ECSProcess のポインタ、および EntityAPI の必須関数を確認する。同じ Game 側で Runtime がすでに接続済みの場合も `nullptr` を返す。生成後は `World`、`Time`、`Logger`、`Assets` を接続し、ECS の Active 変更通知を購読する。Game で借りる API の寿命は Engine が管理する。 [GameRuntime の生成](../../../Game/Source/GameRuntime.cpp) ・ [GameEngineAPI](../../../Engine/Include/API/GameEngineAPI.h)

内部の `GameRuntime::Create` は型登録と初期配置の関数を受け取れる。型登録中だけ `World::RegisterScript<T>()` を許可し、登録が済んだら初期配置を実行する。初期配置で追加した Script は最初のフレームより前に Storage に反映する。現在の DLL 公開入口はこれらの関数を省略して空の Runtime を生成し、スモーク用 DLL は内部入口に登録と初期配置を渡して動作を検証する。 [型登録](../../../Game/Include/GameRuntime.h) ・ [DLL 公開入口](../../../Game/Source/GameRuntime.cpp) ・ [スモーク用 Script](../../../Test/Smoke/GameCoreSmokeProbe.cpp)

## Script の登録と実行

GameRuntime は `typeid(T)` から型ごとの `ScriptStorage<T>` を引く。型が持つコールバックは `requires` で検出し、呼び出すときは所属 Entity の `EntityHandle` を渡す。Runtime は型ごとの Storage を登録順に呼び、Storage はページ内の具象型を走査する。 [GameRuntime](../../../Game/Include/GameRuntime.h) ・ [ScriptStorage](../../../Game/Include/ScriptStorage.h)

| コールバック | 実行条件 |
| --- | --- |
| `OnEnable`／`OnDisable` | Entity の Active 設定と Script の enabled の両方が揃ったときに `OnEnable`、その状態から外れたときに `OnDisable`。Active な Entity への追加時も `OnEnable` を呼ぶ |
| `OnStart` | 追加時に一度呼ぶ。`OnEnable` があればその後に呼び、そこで Script が削除された場合は省く |
| `FixedUpdate`／`Update`／`LateUpdate` | 同名の ECS phase の後に実行する。Storage に反映済みで削除予約されておらず、Script が enabled かつ Entity が Active の場合に呼ぶ |
| `OnDestroy` | Script の削除要求時、所属 Entity を破棄するフレームの末尾、または Runtime の終了時に一度呼ぶ |

Script の enabled は初期値が `true`。Entity の Active 設定の変更は ECS から届き、コールバック内でさらに変更した場合は順に処理する。各通知は発生時点までに追加された Script を対象にする。Entity の破棄予約後は Engine の `IsActive` が `false` を返すため、残りの更新をスキップする。 [Active の通知キュー](../../../Game/Source/GameRuntime.cpp) ・ [Active の変更入口](../../../Engine/Source/ECS/ECSWorld.cpp)

## 追加・削除と参照寿命

各型の実体と状態は、一ページあたり128要素までの並行する配列に入る。Entity の Index と Generation から Sparse Index で位置を引く。追加した Script は保留領域に置かれ、即時に取得できる。実行中の追加が更新対象になるのは `EndFrame` で反映された後のフレームから。ページを増やしても既存ページの Script は移動しない。 [Storage の配置と呼び出し](../../../Game/Include/ScriptStorage.h)

Script の削除要求はその場で `ScriptRef<T>` を無効にし、`OnDestroy` を呼ぶ。描画後に GameRuntime が Engine の破棄予約を Handle 単位で読み、所属 Script を削除する。`OnDestroy` 中に増えた破棄予約も同じ走査で処理する。続けて全 Storage の削除と追加を反映し、ECS が Entity の実体を破棄する。Storage は Swap Remove で移動した要素の Sparse Index と `ScriptRef<T>` の参照先を更新する。 [GameRuntime のフレーム末尾](../../../Game/Source/GameRuntime.cpp) ・ [Engine のフレーム末尾](../../../Engine/Source/Engine.cpp)

終了時は ECS の通知購読を解除し、Script の `OnDestroy` と Storage の破棄を済ませてから各 Facade の接続を解除する。その後 Engine が Game.dll をアンロードする。 [GameRuntime の終了処理](../../../Game/Source/GameRuntime.cpp) ・ [Engine のアンロード処理](../../../Engine/Source/Engine.cpp)
