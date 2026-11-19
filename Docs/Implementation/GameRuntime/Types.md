# GameRuntime・World・Script の型

[処理・構造](README.md) / [Process の登録と公開](../ECS/DevelopmentGuide.md#公開processのapiとfacade)

| 型・入口 | 所有と操作 | 定義 |
| --- | --- | --- |
| `GameRuntimeAPI` | Game 側が持つ関数テーブル。`Create`、`Destroy`、`FixedUpdate`、`Update`、`LateUpdate`、`EndFrame` を Engine が呼ぶ | [GameRuntimeAPI.h](../../../Common/Include/GameRuntimeAPI.h) |
| `HestiaGame::Setup` | Game が実装する初期化の入口。GameCore が型登録の `RegisterScripts` と初期配置の `SetupScene` を順に呼ぶ | [GameSetup.h](../../../Game/Include/GameSetup.h) |
| `GameEngineAPI` | Engine が所有する `TimeAPI`、`EntityAPI`、`ECSProcessAPI`、`LogAPI`、`AssetAPI` への非所有ポインタをまとめる | [GameEngineAPI.h](../../../Engine/Include/API/GameEngineAPI.h) |
| `EntityHandle` | Entity の Index と Generation を値で渡す。操作ごとに Engine 内で解決する | [EntityHandle.h](../../../Common/Include/EntityHandle.h) |
| `EntityAPI` | Entity の生成・破棄、Active、名前、タグ、検索、Process の追加・検索、破棄予約の読み取り、Active 通知の登録を公開する | [EntityAPI.h](../../../Engine/Include/API/EntityAPI.h) |
| `World` | Entity と Process の操作を Engine に、Script の操作を GameRuntime に委譲する | [World.h](../../../Game/Include/World.h) |
| `ScriptState<T>` | 所属 Handle、参照先、追加順の番号、enabled、最後に受け取った Entity の Active 設定、削除フラグを保持する | [ScriptStorage.h](../../../Game/Include/ScriptStorage.h) |
| `ScriptReference<T>` | 現在の `T*` と通常の整数の参照カウントを持つ。Storage 側と `ScriptRef<T>` が参照を所有し、最後の参照が解放されたときに自身を破棄する | [ScriptRef.h](../../../Game/Include/ScriptRef.h) |
| `ScriptRef<T>` | `AddScript`／`GetScript` から値で返る。`operator bool` で参照可否を判定し、無効な参照への `operator->` は `std::abort` を呼ぶ | [ScriptRef.h](../../../Game/Include/ScriptRef.h) |

## Entity 検索結果の受け渡し

名前検索は最初の一致を `EntityHandle` で返す。一致がなければ無効 Handle を返す。タグと Process の検索は Engine が ECS を走査し、一致した Handle を再利用可能な作業配列に集める。非 Active の Entity も含み、生成待ちと破棄予約済みを除く。Process は要求 Component の構成で判定する。 [ECS の Entity 検索](../../../Engine/Source/ECS/ECSWorld.cpp)

`EntitySearchResult` の `data` と `size` は借用。次の複数件検索、明示クリア、または Engine 終了まで有効。`World` は借用中に Game 側の vector にコピーし、ゼロ件の場合も `ClearEntitySearchResults` を呼ぶ。 [検索の境界型](../../../Engine/Include/API/EntityAPI.h) ・ [Game 側のコピー](../../../Game/Source/World.cpp)

## Game 向けの共通 Facade

| Facade | 利用する API と動作 |
| --- | --- |
| `HestiaGame::Time` | `TimeAPI` を借用する。Getter は Engine の `TimeData` を読み、倍率設定は Engine の関数ポインタを呼ぶ |
| `HestiaGame::Logger` | `LogAPI` の4本の関数ポインタを呼ぶ。既定引数により Game の呼び出し位置を記録し、未接続では記録しない |
| `HestiaGame::Assets` | `AssetAPI` から Mesh／Texture のテーブルを選ぶ。未接続なら `Load`／`Register` は無効 Handle、`Get` は `nullptr` を返す。`Register` が受け取った Asset は未接続でも破棄する |

[Time](../Time/Types.md) ・ [Log](../Log/Types.md) ・ [Asset](../Asset/Types.md)
