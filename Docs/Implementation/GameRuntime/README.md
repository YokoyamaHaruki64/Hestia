# GameRuntime の処理・構造

[全体索引](../README.md) / [型と API](Types.md) / [Process の登録と公開](../ECS/DevelopmentGuide.md#公開processのapiとfacade)

## プロジェクトとリンク

| プロジェクト | コンパイルする内容 | 出力 |
| --- | --- | --- |
| GameCore | DLL 入口、GameRuntime、World などの共通部分 | `Build/lib/x64/<構成>/GameCore.lib`。Test では `Test/Build/lib/x64/Test/GameCore.lib` |
| Game | Script の生成済み C++ と `GameSetup.generated.cpp`。GameCore と Common を参照する | `Build/bin/x64/<構成>/Game.dll`。Test では GameCore の内容を含む `Test/Build/lib/x64/Test/Game.lib` |

Editor・Game 構成では Game のリンク時に `GetGameRuntimeAPI` を参照させ、入口を含むオブジェクトを `GameCore.lib` から取り込む。実行時は App と同じ場所に Engine.dll・Game.dll を置く。Engine の Game への参照はビルド順を指定する。 [GameCore.vcxproj](../../../Game/GameCore.vcxproj) ・ [Game.vcxproj](../../../Game/Game.vcxproj) ・ [Engine.vcxproj](../../../Engine/Engine.vcxproj)

## DLL 接続と初期化

Editor・Game 構成では、Engine が App と同じディレクトリの `Game.dll` をロードし、`GetGameRuntimeAPI` を取得する。`GetGameRuntimeAPI` は Game.dll 内の `static const GameRuntimeAPI` を返す。`Test` 構成では、静的 `Game.lib` から同じ関数を直接呼ぶ。GameRuntime の生成・破棄は Game 側が行い、Engine は API テーブルと不透明なハンドルを持つ。 [Engine のロード処理](../../../Engine/Source/Engine.cpp) ・ [GameRuntimeAPI](../../../Common/Include/GameRuntimeAPI.h)

`Create` は `GameEngineAPI` と Time・Entity・ECSProcess のポインタ、および EntityAPI の必須関数を確認する。同じ Game 側で Runtime がすでに接続済みの場合も `nullptr` を返す。生成後は `World`、`Time`、`Logger`、`Assets`、`Scene`、`Input` を接続し、ECS の Active 変更通知を購読する。Game で借りる API の寿命は Engine が管理する。 [GameRuntime の生成](../../../Game/Source/GameRuntime.cpp) ・ [GameEngineAPI](../../../Engine/Include/API/GameEngineAPI.h)

1. GameCore の DLL 入口は `GameRuntime::Create` に生成された `HestiaGame::Setup::RegisterScripts` を渡す。GameRuntime はこの呼び出し中だけ `World::RegisterScript<T>(guid)` を受け付け、重複する型や GUID、空の GUID を拒否する。
2. Engine は Runtime の生成後に `QueryScriptAttachments` を ECS に接続し、起動 Scene を復元する。SceneSystem が Entity を展開し、`AttachSceneScript` と `FlushScripts` で Script の追加を反映してから最初のフレームへ進む。 [DLL 入口](../../../Game/Source/GameEntry.cpp) ・ [Scene の保存と切り替え](../Engine/README.md#scene-の保存と切り替え)

スモーク用 DLL は公開テーブルの `Create` と `FlushScripts` を差し替え、起動 Scene の展開後に検証用 Script を追加する。 [スモーク用 Script](../../../Test/Smoke/GameCoreSmokeProbe.cpp)

## Script のコード生成

Game ビルド前に `Props/GameScripts.props` が、`Game.vcxproj` の `ClCompile` に登録された `Assets/` 配下の `.cpp` を順に列挙し、`GameCodeGenerator.exe --project <input-list>` を呼ぶ。Generator は `MetaTool.exe --ext .cpp` でメタデータを揃え、宣言・実装ブロックと型登録を `GeneratedScript/` に出力する。IntelliSense は元の `.cpp` を解析し、通常ビルドは生成した `.cpp` をコンパイルする。 [MSBuild 設定](../../../Props/GameScripts.props) ・ [Generator](../../../Tools/GameCodeGenerator/GameCodeGenerator.cpp)

Visual Studio の Developer PowerShell から `Tools/GameCodeGenerator/GameCodeGenerator.exe --watch` を起動できる。`.cpp`・`.meta` の通知を約200msでまとめ、プロジェクト設定が変わったときは MSBuild で登録一覧を評価し直す。監視とビルドは同じ生成先、差分キャッシュ、書き込みロックを使う。 [監視実装](../../../Tools/GameCodeGenerator/ScriptWatcher.cpp)

キャッシュは Script の相対パスごとに入力の更新時刻・サイズ、型名・GUID を保持する。変更通知や生成物の欠損があれば対象を読み直し、重複は登録一覧全体で検証する。生成ファイルは内容が変わったときだけ置き換える。監視が反映するのは生成済みソースまでで、Game.dll の読み直しは今後の機能。 [入力形式、ビルド、キャッシュの設計](../../Design/GameRuntime/CppScript.md)

## Script の登録と実行

GameRuntime は `typeid(T)` と Asset GUID から同じ型別 Storage を引く。型が持つコールバックは `requires` で検出し、呼び出すときは所属 Entity の `EntityHandle` を渡す。Runtime は型ごとの Storage を登録順に呼び、Storage はページ内の具象型を走査する。 [GameRuntime](../../../Game/Include/GameRuntime.h) ・ [ScriptStorage](../../../Game/Include/ScriptStorage.h)

| コールバック | 実行条件 |
| --- | --- |
| `OnEnable`／`OnDisable` | Entity の Active 設定と Script の enabled の両方が揃ったときに `OnEnable`、その状態から外れたときに `OnDisable`。Active な Entity への追加時も `OnEnable` を呼ぶ |
| `OnStart` | 追加時に一度呼ぶ。`OnEnable` があればその後に呼び、そこで Script が削除された場合は省く |
| `FixedUpdate`／`Update`／`LateUpdate` | 同名の ECS phase の後に実行する。Storage に反映済みで削除予約されておらず、Script が enabled かつ Entity が Active の場合に呼ぶ |
| `OnDestroy` | Script の削除要求時、所属 Entity を破棄するフレームの末尾、Scene 切り替え、または Runtime の終了時に一度呼ぶ |

Script の enabled は初期値が `true`。Entity の Active 設定の変更は ECS から届き、コールバック内でさらに変更した場合は順に処理する。各通知は発生時点までに追加された Script を対象にする。Entity の破棄予約後は Engine の `IsActive` が `false` を返すため、残りの更新をスキップする。 [Active の通知キュー](../../../Game/Source/GameRuntime.cpp) ・ [Active の変更入口](../../../Engine/Source/ECS/ECSWorld.cpp)

## 追加・削除と参照寿命

各型の実体と状態は、一ページあたり128要素までの並行する配列に入る。Entity の Index と Generation から Sparse Index で位置を引く。追加した Script は保留領域に置かれ、即時に取得できる。実行中の追加が更新対象になるのは `EndFrame` で反映された後のフレームから。ページを増やしても既存ページの Script は移動しない。 [Storage の配置と呼び出し](../../../Game/Include/ScriptStorage.h)

Script の削除要求はその場で `ScriptRef<T>` を無効にし、`OnDestroy` を呼ぶ。描画後に GameRuntime が Engine の破棄予約を Handle 単位で読み、所属 Script を削除する。`OnDestroy` 中に増えた破棄予約も同じ走査で処理する。続けて全 Storage の削除と追加を反映し、ECS が Entity の実体を破棄する。Storage は Swap Remove で移動した要素の Sparse Index と `ScriptRef<T>` の参照先を更新する。 [GameRuntime のフレーム末尾](../../../Game/Source/GameRuntime.cpp) ・ [Engine のフレーム末尾](../../../Engine/Source/Engine.cpp)

終了時は ECS の通知購読を解除し、Script の `OnDestroy` と Storage の破棄を済ませてから各 Facade の接続を解除する。その後 Engine が Game.dll をアンロードする。 [GameRuntime の終了処理](../../../Game/Source/GameRuntime.cpp) ・ [Engine のアンロード処理](../../../Engine/Source/Engine.cpp)

## Scene 用の Script 参照

`FlushScripts` の反映に合わせて、Entity Handle から Storage 添字列を引く索引を更新する。`QueryScriptAttachments` は指定された Handle の反映済み Script について、GUID をコールバックで Engine に返す。Scene の保存データは型参照を持ち、復元時は既定構築した Script が `OnStart` を迎える。Scene 切り替え時の `ClearSceneScripts` は実体と Entity 側の索引を消し、型登録と GUID 索引を維持する。 [GameRuntime.cpp](../../../Game/Source/GameRuntime.cpp) ・ [Scene の保存形式](../../Design/Serialize/SceneSerialization.md)
