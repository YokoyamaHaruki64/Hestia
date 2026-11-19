# GameRuntime の処理・構造

[全体索引](../README.md) / [型と API](Types.md) / [Process の登録と公開](../ECS/DevelopmentGuide.md#公開processのapiとfacade)

## プロジェクトとリンク

| プロジェクト | コンパイルする内容 | 出力 |
| --- | --- | --- |
| GameCore | DLL 入口、GameRuntime、World などの共通部分 | `Build/lib/x64/<構成>/GameCore.lib`。Test では `Test/Build/lib/x64/Test/GameCore.lib` |
| Game | 現在は回転 Script を使う `GameSetup.cpp`。GameCore と Common を参照する | `Build/bin/x64/<構成>/Game.dll`。Test では GameCore の内容を含む `Test/Build/lib/x64/Test/Game.lib` |

Editor・Game 構成では Game のリンク時に `GetGameRuntimeAPI` を参照させ、入口を含むオブジェクトを `GameCore.lib` から取り込む。実行時は App と同じ場所に Engine.dll・Game.dll を置く。Engine の Game への参照はビルド順を指定する。 [GameCore.vcxproj](../../../Game/GameCore.vcxproj) ・ [Game.vcxproj](../../../Game/Game.vcxproj) ・ [Engine.vcxproj](../../../Engine/Engine.vcxproj)

## DLL 接続と初期化

Editor・Game 構成では、Engine が App と同じディレクトリの `Game.dll` をロードし、`GetGameRuntimeAPI` を取得する。`GetGameRuntimeAPI` は Game.dll 内の `static const GameRuntimeAPI` を返す。`Test` 構成では、静的 `Game.lib` から同じ関数を直接呼ぶ。GameRuntime の生成・破棄は Game 側が行い、Engine は API テーブルと不透明なハンドルを持つ。 [Engine のロード処理](../../../Engine/Source/Engine.cpp) ・ [GameRuntimeAPI](../../../Common/Include/GameRuntimeAPI.h)

`Create` は `GameEngineAPI` と Time・Entity・ECSProcess のポインタ、および EntityAPI の必須関数を確認する。同じ Game 側で Runtime がすでに接続済みの場合も `nullptr` を返す。生成後は `World`、`Time`、`Logger`、`Assets` を接続し、ECS の Active 変更通知を購読する。Game で借りる API の寿命は Engine が管理する。 [GameRuntime の生成](../../../Game/Source/GameRuntime.cpp) ・ [GameEngineAPI](../../../Engine/Include/API/GameEngineAPI.h)

1. GameCore の DLL 入口は `GameRuntime::Create` に `HestiaGame::Setup::RegisterScripts` を渡す。GameRuntime はこの呼び出し中だけ `World::RegisterScript<T>()` を受け付ける。
2. Runtime の生成成功後、DLL 入口は `HestiaGame::Setup::SetupScene` を呼ぶ。Setup は渡された反映コールバックを初期配置の途中で呼べる。初期配置が失敗した場合は Runtime を終了・破棄して `nullptr` を返す。
3. 初期配置が完了すると DLL 入口が `EndFrame` を呼び、最初のフレームまでに残りの Script を Storage に反映する。

現在の手書き Setup は `RotateTransformScript` を登録し、Engine が先に作った立方体の Entity を `World::FindEntitiesWithProcess<Transform>()` で取得して関連付ける。保留中の Script の重複探索が積み上がるため、256件ごとに反映する。各 Script は `OnStart` で保持した Transform Facade を使い、毎 `Update` で Z 軸まわりに 0.01 ラジアンの回転を合成する。Engine の `MoveTransform` 登録はコメントアウトしている。 [DLL 公開入口](../../../Game/Source/GameEntry.cpp) ・ [Setup の契約](../../../Game/Include/GameSetup.h) ・ [初期配置](../../../Game/Scripts/GameSetup.cpp) ・ [回転 Script](../../../Game/Scripts/RotateTransformScript.h) ・ [ECS の登録](../../../Engine/Source/ECS/ECSSystem.cpp)

内部の `GameRuntime::Create` には型登録と初期配置の両方を渡すこともできる。スモーク用 DLL は公開テーブルの `Create` を差し替え、この内部入口で専用 Script を初期配置する。 [GameRuntime.h](../../../Game/Include/GameRuntime.h) ・ [スモーク用 Script](../../../Test/Smoke/GameCoreSmokeProbe.cpp)

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
