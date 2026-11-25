# Hestia の全体設計

## 責務と所有

| 実行単位 | 所有するもの | 責務 |
| --- | --- | --- |
| Application.exe | Window、フレームタイマー、Engine.dll のロード状態と `EngineHandle` | メッセージ処理と時間計測から固定更新・通常フレームを駆動する |
| Engine.dll | Engine 実体、各 System、境界 API、Game.dll のロード状態と Runtime Handle | ECS・描画を動かし、GameRuntime に API と更新タイミングを渡す |
| Game.dll | GameRuntime、型ごとの Script Storage、ゲーム状態 | C++ のゲームコードを実行し、Facade で Engine を操作する |

Engine は `ApplicationAPI` を値で保持する。Window の HWND は必要時に取得し、Window の寿命は Application が管理する。Engine と GameRuntime は各 DLL 内で生成・破棄し、呼び出し側は不透明 Handle と非所有の関数テーブルを使う。

## DLL の接続

Application は `GetEngineAPI`、Engine は `GetGameRuntimeAPI` を動的に取得する。GameRuntime の生成時に Engine が `GameEngineAPI` を渡す。Game は Time、Entity、公開 Process、Log、Asset の API を借り、`HestiaGame` Facade から利用する。System 実体と Entity の解決は Engine が所有する。

Editor・Game 構成では App、Engine.dll、Game.dll を同じ出力先に置く。Game は Engine.lib に、Engine は Game.lib にリンクしない。両 DLL は Common.lib を利用する。Game は共通 Runtime を静的 GameCore.lib として取り込み、具象 Script と Setup を加えて Game.dll を生成する。Test 構成ではテスト exe が GameCore を含む静的 Game.lib をリンクし、同じ Runtime API を直接呼ぶ。共有境界型は `Hestia`、Script 用 Facade は `HestiaGame` に置く。 [Game.dll](Design/GameRuntime/GameDLL.md) ・ [API 構成](Design/GameRuntime/APIArchitecture.md)

## System の分担

| System | 契約 |
| --- | --- |
| Time | Application が渡す経過秒を保持し、倍率を scaled 時間に反映する |
| ECS | 単一 World の Entity と Component を所有し、Component 構成に基づき Process を実行する |
| Scene | Engineから受けた保存・切り替え要求をフレーム末尾で処理する。 [SceneSystemの設計](Design/SceneSystem.md) |
| Asset | Asset参照の解決とCPU Assetの寿命を管理する。GPU資源はGraphicsが所有する。 [AssetSystem設計](Design/AssetSystem.md) ・ [AssetDB設計](Design/Asset/AssetDB.md) |
| Graphics | ECS の描画提出を受け、Backend で記録して表示する |
| Log | Engine と Game のログを受け、System 終了中も利用できる |

System ごとに必要な初期化・終了処理を持つ。Game に公開する操作は境界 API に置く。 [ECS](Design/ECS.md) ・ [実装仕様書](Implementation/README.md)

## 起動・更新・終了

1. Application は Window とタイマーを準備し、Engine.dll をロードして Engine を生成する。Engine は各 System と境界 API の後に Game.dll と GameRuntime を起動する。失敗時は準備済み資源を解放し、無効 Handle を返す。
2. Application はメッセージを処理し、実経過時間の蓄積に応じて固定更新を呼び、通常フレームを一度実行する。
3. Engine は Time を先に更新する。固定更新は ECS の後に GameRuntime。通常更新は FrameBegin、ECS Update、GameRuntime Update、ECS LateUpdate、GameRuntime LateUpdate、ECS PreRender、描画の順に進む。PreRender で Transform が World 行列を確定し、MeshRenderer が描画データを提出する。
4. 描画後、GameRuntime の EndFrame で破棄予約 Entity の Script を除き、保留 Script 変更を反映する。その後 ECS が Entity 実体を破棄する。
5. Engine は Script の終了処理、Facade の切断、GameRuntime 破棄、Game.dll アンロードを済ませてから各 System を終了する。Graphics が借りる CPU Asset は Graphics の終了後に解放する。Application は Engine.dll をアンロードし、Window を終了する。

現行の挙動は [実装仕様書](Implementation/README.md)、未決事項は [OpenDecisions](OpenDecisions.md) を参照する。
