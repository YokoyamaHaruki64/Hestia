# Engine と Game の API 構成
---

Game コードは `HestiaGame` の Facade を通じて Engine の機能を使う。Engine は `GameEngineAPI` に各 API のアドレスをまとめ、GameRuntime の生成時に渡す。DLL を越える操作は、受け取った関数ポインタで呼ぶ。 [API 再設計の原案](../../../Reference/HestiaDesign/DevelopNotes/APIArchitectureRefactor.md) ・ [Game の起動・更新・終了](./GameDLL.md)

## DLL の接続と API の所有
---

| 呼び出し方向 | 接続方法 |
| --- | --- |
| App → Engine | `Engine.dll` をロードし、export された `GetEngineAPI` を `GetProcAddress` で取得する。戻り値の `EngineAPI` で起動・更新・終了を呼ぶ |
| Engine → Game | `Game.dll` をロードし、export された `GetGameRuntimeAPI` を `GetProcAddress` で取得する。戻り値の `GameRuntimeAPI` と不透明な Runtime Handle を使う |
| Game → Engine | `GameRuntimeAPI::Create` で `const GameEngineAPI*` を借用し、そこから各 API に接続する。Game は `Engine.lib` なしでビルドする |

通常構成で各 DLL が export するのは接続用の入口一つ。`Test` 構成では静的 `Game.lib` をリンクし、同じ `GetGameRuntimeAPI` を直接呼ぶ。API テーブルのレイアウトを共有するため、定義を変更したときは Engine と Game を同じヘッダーでビルドする。 [EngineAPI](../../../Engine/Include/API/EngineAPI.h) ・ [GameRuntimeAPI](../../../Common/Include/GameRuntimeAPI.h) ・ [GameEngineAPI](../../../Engine/Include/API/GameEngineAPI.h)

Engine と各 System が API を所有する。GameRuntime は Script の初期化前に `Time`、`Logger`、`Assets` を接続し、`World` からも Engine API を参照できるようにする。終了時は Script の `OnDestroy` が済んでから接続を解除する。Entity にひも付く Facade の値は Game 側で保持でき、API の借用期間は生成元の GameRuntime の寿命内とする。 [Engine の初期化と終了](../../../Engine/Source/Engine.cpp) ・ [GameRuntime の接続処理](../../../Game/Source/GameRuntime.cpp)

## Game から使う API
---

| `GameEngineAPI` のメンバ | Game 側の窓口 | 呼び出し方 |
| --- | --- | --- |
| `m_time` | `HestiaGame::Time` | Getter は Engine が所有する `TimeData` を読む。`SetTimeScale` は Engine の関数ポインタを呼ぶ |
| `m_entity` | `HestiaGame::World` | Entity の生成・破棄、Active、名前・タグ、検索、Process の追加・検索を `EntityAPI` の関数ポインタで呼ぶ |
| `m_ecsProcess` | `World` と `Transform`／`MeshRenderer` | `World` が Facade 型に対応する登録を取得し、Facade は個別操作の関数ポインタを呼ぶ |
| `m_log` | `HestiaGame::Logger` | `LogAPI` の `Log`／`Warning`／`Error`／`Fatal` を直接呼ぶ。Game 側の Facade が呼び出し位置を補う |
| `m_asset` | `HestiaGame::Assets` | `AssetAPI::GetOperations<T>()` で Mesh または Texture のテーブルを選び、`Load`／`Get`／`Register` を呼ぶ。未接続時の戻り値と `Register` が受け取る所有権は Facade が扱う |

`World` の Script 操作は GameRuntime 内で処理する。複数 Entity の検索結果は Engine の作業配列から借用し、`World` が Handle を Game 側の配列へコピーしてから借用を終了する。 [World](./World.md) ・ [EntityAPI](../../../Engine/Include/API/EntityAPI.h) ・ [AssetAPI](../../../Engine/Include/API/AssetAPI.h) ・ [LogAPI](../../../Engine/Include/API/LogAPI.h)

## Process・API・Facade の登録
---

| 要素 | 責務 | MeshRenderer の例 |
| --- | --- | --- |
| Process | ECS が phase ごとに実行する一括処理 | `MeshRendererProcess::Run` が描画データを提出する |
| API | `EntityHandle` を受け取り、Engine 側で Entity と Component を解決して個別に操作する | `MeshRendererAPI::SetMaterial` が指定された MaterialSlot を更新する |
| Facade | Game が値として保持し、保持した `EntityHandle` と API ポインタを使って呼ぶ | `HestiaGame::MeshRenderer::SetMaterial(index, material)` |

公開する Process は ECS 初期化時に `REGISTER_GAME_PROCESS(API, 関数, phase)` で登録する。ECS 登録が成功したら、該当 API の `ECSProcessBinding<API>` に `ProcessHandle` を格納する。Handle は Engine の登録済み Process 配列の添字を表し、`EntityAPI` が追加・検索の際に解決する。Engine 内で使う Process は `REGISTER_PROCESS(関数, phase)` で登録する。現在は `Transform` と `MeshRenderer` を公開しており、`PreRender` では Transform を更新してから MeshRenderer が描画データを提出する。 [ECS 初期化](../../../Engine/Source/ECS/ECSSystem.cpp) ・ [型付き登録](../../../Engine/Include/ECS/ECSSystem.h) ・ [EntityAPI の接続](../../../Engine/Source/API/EntityAPI.cpp)

Facade 内の `using API = Hestia::MeshRendererAPI;` が、`World` に対応する型を伝える。`ECSProcessAPI::Get<API>()` は型に対応する生成済みの `Member` オーバーロードをコンパイル時に選ぶ。`GetProcess<T>` はその登録の API ポインタを使い、`AddProcess<T>` と `FindEntitiesWithProcess<T>` は同じ登録の `ProcessHandle` を使う。保持した Facade は操作のたびに Engine 側で Entity を解決するため、Archetype の移動をまたいで使える。 [ECSProcessAPI](../../../Engine/Include/API/ECSProcessAPI.h) ・ [World の取得・追加・検索](../../../Game/Include/World.h)

## 公開 Process の追加手順
---

Component と Process の実装、公開 API と Facade の接続、ビルド時の集約生成、Game 側の利用までを ECS 開発手引きに記載する。 [公開 Process の API と Facade](../../Implementation/ECS/DevelopmentGuide.md#公開processのapiとfacade)

## 初期の描画経路
---

初期段階では Game 向け `GraphicsAPI` の追加を保留する。Game は `Transform` と `MeshRenderer` を使って Entity の位置・姿勢と Mesh・Material・インスタンス色を設定する。`GetMaterial(index)` と `SetMaterial(index, material)` の `index` は MaterialSlot の添字で、Material は取得済みの `AssetHandle<Material>` を渡す。`SetMaterial` で配列を広げたときは、間のスロットを空にする。現行の `Assets` が公開する型は Mesh と Texture。 [MeshRenderer Facade](../../../Engine/Include/Game/MeshRenderer.h) ・ [AssetAPI](../../../Engine/Include/API/AssetAPI.h)

ビューと射影は Engine のデバッグシーンが設定し、`MeshRendererProcess` が描画データを `GraphicsSystem` に提出する。追従カメラや視点切り替えを扱う段階では、カメラの Transform や画角を `Camera` の Process／API／Facade から操作する案を検討する。画面全体の設定を Game が変更する要件が出た場合に、その操作を `GraphicsAPI` の候補とする。 [固定カメラの設定](../../../Engine/Source/Engine.cpp) ・ [描画の提出](../../../Engine/Source/Graphics/MeshRenderer.cpp)
