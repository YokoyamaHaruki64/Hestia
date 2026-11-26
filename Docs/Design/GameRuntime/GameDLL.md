# Game.dll
---
GameRuntime は `Game.dll` に含まれ、型ごとの C++ Script を所有・実行する。Engine は関数テーブルで初期化、更新、Scene の Script 復元、終了を呼び出す。

[全体構造](../../Architecture.md) ・ [Script の設計](./Script.md) ・ [World](./World.md) ・ [Engine と Game の API 構成](./APIArchitecture.md)

## GameCore と Game の分担
---

| プロジェクト | 責務 | 成果物 |
| --- | --- | --- |
| GameCore | Engine との接続、DLL の入口、GameRuntime、Script の Storage、World などの共通 Facade を提供する | 静的ライブラリ `GameCore.lib` |
| Game | 具象 Script と、生成されたゲーム固有の型登録をコンパイルする | `GameCore.lib` を組み込んだ `Game.dll` |

GameCore は Facade を接続した後、Game が生成・コンパイルする `RegisterScripts()` を呼ぶ。初期配置は型登録が済んだ Runtime を使い、Engine の SceneSystem が起動 Scene から復元する。 [Script の型登録と所有](./Script.md#型登録と所有) ・ [C++ Script のコード生成](CppScript.md) ・ [起動Scene](../SceneSystem.md#起動scene)

## DLL 境界と所有
---

| 呼び出し方向 | 入口 | 所有と参照 |
| --- | --- | --- |
| App → Engine | 既存の `GetEngineAPI` を動的に取得 | App が `Engine.dll` をロードし、`EngineAPI` と `EngineHandle` を保持する |
| Engine → Game | `GetGameRuntimeAPI` を動的に取得 | Engine が `Game.dll` をロードし、DLL 内の関数テーブルを借用する。GameRuntime の実体は Game.dll 内で生成・破棄する |
| Game → Engine | `Create` に渡される `GameEngineAPI` | Engine が `GameEngineAPI` と各 Boundary API を所有し、GameRuntime が利用期間中だけ参照する |

`GetGameRuntimeAPI` は `extern "C"` と `HESTIA_GAME_API` で公開し、Game.dll 内の `static const GameRuntimeAPI` を返す。Engine は GameRuntime の実体を `void*` として保持し、更新と破棄のたびに関数テーブルへ渡す。共有する境界型は `Hestia` に置き、GameRuntime の型定義は GameCore に置く。

```cpp
namespace Hestia
{
    class TimeAPI;
    class LogAPI;
    class AssetAPI;
    struct SceneAPI;
    struct EntityAPI;
    struct ECSProcessAPI;

    struct GameEngineAPI
    {
        const TimeAPI* m_time = nullptr;
        const EntityAPI* m_entity = nullptr;
        const ECSProcessAPI* m_ecsProcess = nullptr;
        const LogAPI* m_log = nullptr;
        const AssetAPI* m_asset = nullptr;
        const SceneAPI* m_scene = nullptr;
    };

    // ScriptAttachmentQuery、SceneScriptResult、EntityHandle、AssetGuid は共有ヘッダーで定義
    struct GameRuntimeAPI
    {
        void* (*Create)(const GameEngineAPI* engineAPI);
        void (*Destroy)(void* runtime);
        void (*FixedUpdate)(void* runtime);
        void (*Update)(void* runtime);
        void (*LateUpdate)(void* runtime);
        void (*EndFrame)(void* runtime);
        ScriptAttachmentQuery QueryScriptAttachments;
        SceneScriptResult (*AttachSceneScript)(void* runtime, EntityHandle entity, AssetGuid guid);
        bool (*FlushScripts)(void* runtime);
        void (*ClearSceneScripts)(void* runtime);
    };
}

extern "C" HESTIA_GAME_API const Hestia::GameRuntimeAPI* GetGameRuntimeAPI();
```

GameCore と Game は共有 Header と、Engine が渡すデータおよび関数ポインタを使う。`Engine.lib` は Game のリンク入力に含めない。Game に許可する操作は `GameEngineAPI` からたどれる API に追加する。App 向けの入口は既存の `GetEngineAPI` を使う。 [現行の EngineAPI](../../../Engine/Include/API/EngineAPI.h)

`EntityAPI` は Game 側の Entity 操作と、Script の実行対象を判定するための Entity の状態、フレーム末尾で処理する破棄予約を `EntityHandle` 経由で扱う。GameRuntime は `EntityAPI` を通じて Active 設定の変更通知を購読する。`World` はこれらの Engine 側 API と、GameRuntime が所有する Script の操作をまとめて公開する。 [Entity の受け渡し](./World.md#entity-の受け渡し) ・ [Script と Entity の寿命](./Script.md#entity-との寿命の同期)

## Time の接続
---

- Engine は `TimeSystem` と `TimeAPI` を値として所有する。`TimeAPI` は System が所有する `TimeData` への非所有参照を保持し、既存の inline Getter がその値を読む。
- `TimeAPI::SetTimeScale` は inline で、`TimeAPI::Initialize` が設定した関数ポインタと参照先を使う。関数の本体は Engine.dll 内で `TimeSystem::SetTimeScale` に委譲する。倍率の検証と反映は既存の `TimeSystem` が担う。
- `HestiaGame::Time` は `TimeAPI` への非所有参照を持ち、Game コードに時間の取得と倍率設定を公開する。`Bind / Unbind` は private とし、GameRuntime だけが呼ぶ。ゲームの初期化から終了処理まで接続を維持する。
- 各 delta は App が渡した秒数を基にする。scaled 値はその秒数に `timeScale` を掛け、通常更新だけが累積時間を進める。倍率は有限かつ非負の値を受理し、0 で scaled 時間を止める。App が固定更新を呼ぶ頻度は実経過時間で決まる。

[Time の実装仕様](../../Implementation/Time/README.md)

## 起動・更新・終了
---

1. **起動**。Engine は System と Boundary API を準備し、`GameEngineAPI` を構成する。App の実行ファイルと同じディレクトリに配置した `Game.dll` をロードし、`GetGameRuntimeAPI` とテーブル内の関数ポインタを確認する。`Create` は必須 API を確認し、Runtime の生成、Facade の Bind、Script の型登録を行う。Engine は初期化済みのハンドルを使い、起動 Scene を復元する。
2. **固定更新**。`TimeSystem::UpdateFixed` → ECS `FixedUpdate` → GameRuntime `FixedUpdate`。ゲーム側は必要な刻みを `HestiaGame::Time::FixedDeltaTime` から取得する。
3. **通常更新**。`TimeSystem::Update` → ECS `FrameBegin` → ECS `Update` → GameRuntime `Update` → ECS `LateUpdate` → GameRuntime `LateUpdate` → ECS `PreRender` → 描画。ゲーム側は通常の経過時間を `HestiaGame::Time::DeltaTime` から取得する。Script の `LateUpdate` で変更した Transform は同じフレームの `PreRender` で参照できる。
4. **フレーム末尾**。GameRuntime の `EndFrame` が所属 Script の除去と保留分の反映を行い、ECS が予約済み Entity の実体を破棄する。続いて SceneSystem が保存要求と切り替え要求を順に処理する。
5. **終了**。Engine は System が利用できる間に `Destroy` を呼ぶ。GameRuntime はゲームの終了処理と残っている Script の破棄を行い、Facade を Unbind して実体を破棄する。Engine はハンドルと関数テーブルの参照を解除して `Game.dll` をアンロードし、その後で各 System を終了する。

Editor・Game の両構成で `Game.dll` を必須とする。DLL のロード、エクスポートの取得、関数テーブルの確認、`Create` のいずれかで失敗したら、Engine が失敗した段階をログに残し、既存の初期化失敗経路で App に返す。GameRuntime の生成や初期化が失敗した場合、Game.dll 内で Bind と生成済みの状態を解放し、`Create` は `nullptr` を返す。Engine は取得済みの DLL と初期化済みの System を解放する。

[現行の更新・終了経路](../../../Engine/Source/Engine.cpp) ・ [Engine 初期化結果の返し方](../../../Engine/Source/API/EngineAPI.cpp)

## ビルドと初版の確認
---

Editor・Game 構成では同じ出力先に App、Engine.dll、Game.dll を揃える。`GameCore.lib` と `Common.lib` はビルド時に使う。Engine から Game へのプロジェクト参照はビルド順に使い、DLL 間は実行時に接続する。`Test` 構成は GameCore を含む静的 `Game.lib` をテスト exe にリンクし、同じ `GetGameRuntimeAPI` を直接呼ぶ。DXC などの外部ランタイム依存は別途配置する。

- Editor・Game 構成で App から起動し、GameRuntime の初期化、通常更新、LateUpdate、固定更新、フレーム末尾の処理と終了を確認する。各更新で対応する `HestiaGame::Time` の値を取得する。
- Game のリンク入力と Game.dll のインポートを確認し、`Engine.lib` が不要で、`Engine.dll` への静的インポートがないことを確認する。
- `Game.dll` が未配置の場合は、Editor・Game の両構成で原因をログから確認でき、起動に失敗することを確認する。

動作確認用の立方体は、起動Sceneから復元した `DebugCubeGenerator` がGame側で生成する。回転には `RotateTransformScript` がTransform Facadeを使う。 [生成と描画](../../Implementation/ECSProcess/MeshRenderer.md#動作確認用シーン)
