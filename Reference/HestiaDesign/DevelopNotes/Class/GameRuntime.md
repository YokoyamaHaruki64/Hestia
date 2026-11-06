# Game Runtime
---
Engine.md の GameRuntimeAPI と Facade 注入の関係を Game.dll 内部の宣言へ展開する。
GameRuntime の具体的な宣言はノートにないため、以下は追加草案。ゲーム本体の所有構造は未確定とする。

## GameRuntime
---
**役割**

Engine から渡された GameEngineAPI を Game 側 Facade に Bind し、Game の固定更新・フレーム更新・終了をまとめる。Game.dll の内部型であり、Engine へ型定義を公開しない。

```cpp
class GameRuntime
{
public:
    explicit GameRuntime(const Hestia::GameEngineAPI& engineAPI);

    void Initialize();
    void Finalize();

    void FixedUpdate();
    void Update();

private:
    void BindFacades();
    void UnbindFacades();

    const Hestia::GameEngineAPI* m_engineAPI = nullptr;

    // 未確定：Gameが所有するゲーム状態の主要メンバ
};
```

[GameEngineAPI](./Engine.md#gameengineapi)  
[GameRuntimeAPI](./Engine.md#gameruntimeapi)  
[Assets](./AssetSystem.md#assets)  
[Graphics](./Subsystems.md#graphics)  
[Input](./Subsystems.md#input)  
[Audio](./Subsystems.md#audio)  
[Physics](./Subsystems.md#physics)  
[Time](./Time.md#hestiagametime)

**所有・参照**：GameRuntime の実体は Game.dll が生成・破棄し、Engine は GameRuntimeAPI を通じてその寿命を管理する。m_engineAPI は Engine が保持する公開面を非所有で参照する。ゲーム状態を GameRuntime が所有する案だが、その型・分割は未確定。

**API とメンバ**：constructor は m_engineAPI を関連付ける。Initialize は BindFacades とゲームの初期化をまとめる。Finalize はゲームの終了と UnbindFacades をまとめる。FixedUpdate／Update は Game の状態を進め、必要な時間は HestiaGame::Time から読む。引数なしの更新はノートに一致する。

GameRuntimeAPI::Create は生成と Initialize、Destroy は Finalize と破棄を行う。GameRuntimeAPI の関数テーブルは Game.dll 内の static const、Engine は非所有参照。具体的な trampoline の実装は書かない。

**決定（U04）**：この GameRuntime だけを各 Facade の friend とし、BindFacades／UnbindFacades から設定・解除する。ゲーム終了処理で Facade を利用する間は Bind を維持し、終了処理後に Unbind する。

**未確定（U10）**：GameRuntime の namespace、ゲーム状態の型、Game.dll 内のファイル・lib 分割は記載されていない。

**提案**：GameRuntime は上記のグローバル内部型を仮置きとし、ゲーム状態は用途が分かるまで未確定にする。SparceScript 等を保持する GameLogic 側の基盤になる方向は候補として残し、所有する型や API はまだ採用しない。GameAPI.lib／GameCore.lib、Script 基底クラス、World／Entity などを新資料へ自動的に追加しない。

**決定（U03）**：GameEngineAPI の m_time から TimeAPI を取得し、HestiaGame::Time へ Bind する。Time の Getter と SetTimeScale は更新ノートの API を使用する。

**未確定（U03）**：Game → Application の公開面は別途 GameRuntime に渡す想定。既存の ApplicationAPI を渡すか Game 用 API を作るか、その型・引数・保持メンバは未定。

**提案**：Game から操作する機能を決めてから別の API として宣言する。TimeAPI に FPS 設定や終了要求を混ぜない。
