# Engine
---
Engine.dllの中核となるクラス群。更新ノートと今回の回答に基づき、所有とDLL境界を整理する。
Engineが各SubsystemとGame.dllのライフタイム・更新を管理し、
ApplicationおよびGame.dllとの境界には各種APIを使用する。

## Engine
---
**役割**

各Subsystemを保持・更新し、Applicationからのコールバック、Window Message、Fixed Update、フレーム更新を各Subsystemへ分配する。

Game.dllのロード・アンロードとGameRuntimeの寿命管理も担当する。

```cpp
namespace Hestia
{
    class Engine
    {
    public:
        Engine();

        bool Initialize(const ApplicationAPI& applicationAPI);
        void Finalize();

        void ProcessMessage(
            HWND hwnd,
            UINT message,
            WPARAM wParam,
            LPARAM lParam);

        void FixedUpdate(float deltaTime);
        void FrameExecute(float deltaTime);

    private:
        bool LoadGame();
        void UnloadGame();

        GameEngineAPI CreateGameEngineAPI();

    private:
        // 各Subsystem
        AssetSystem m_assets;
        GraphicsSystem m_graphics;
        InputSystem m_input;
        AudioSystem m_audio;
        PhysicsSystem m_physics;
        TimeSystem m_time;
        // ...

        // Gameへ公開するBoundary APIの実体
        AssetAPI m_assetAPI;
        GraphicsAPI m_graphicsAPI;
        InputAPI m_inputAPI;
        AudioAPI m_audioAPI;
        PhysicsAPI m_physicsAPI;
        TimeAPI m_timeAPI;

        // Application機能へのアクセス
        ApplicationAPI m_applicationAPI;

        // Game.dll実体
        HMODULE m_gameModule = nullptr;

        // Game.dllへ公開するEngine機能
        GameEngineAPI m_gameEngineAPI;

        // GameRuntime機能へのアクセス
        const GameRuntimeAPI* m_gameRuntimeAPI = nullptr;

        // GameRuntime実体
        // 寿命管理兼、GameRuntimeAPIへ渡すハンドル
        void* m_gameRuntime = nullptr;
    };
}
```

[AssetSystem](./AssetSystem.md#assetsystem)  
[GraphicsSystem](./Subsystems.md#graphicssystem)  
[InputSystem](./Subsystems.md#inputsystem)  
[AudioSystem](./Subsystems.md#audiosystem)  
[PhysicsSystem](./Subsystems.md#physicssystem)  
[TimeSystem](./Time.md#timesystem)  
[AssetAPI](./AssetSystem.md#assetapi)  
[GraphicsAPI](./Subsystems.md#graphicsapi)  
[InputAPI](./Subsystems.md#inputapi)  
[AudioAPI](./Subsystems.md#audioapi)  
[PhysicsAPI](./Subsystems.md#physicsapi)  
[TimeAPI](./Time.md#timeapi)  
[ApplicationAPI](./Application.md#applicationapi)  
[GameEngineAPI](#gameengineapi)  
[GameRuntimeAPI](#gameruntimeapi)

**所有・参照**：各 System、Boundary API、ApplicationAPI、GameEngineAPI を値として保持する。Boundary API は対応する System を非所有で参照し、GameEngineAPI は Boundary API への非所有ポインタを持つ。Game.dll のロード状態と GameRuntime の寿命を管理する。ApplicationAPI の Context、GameRuntimeAPI のテーブルは非所有参照であり、GameRuntime は Game.dll 側の Create／Destroy で生成・破棄する。

**API とメンバ**：Initialize は ApplicationAPI を保持し、各 System の利用準備 → 各 Boundary API への接続 → GameEngineAPI の設定 → Game.dll ロード → GameRuntime 生成の順で進める。TimeSystem は constructor で生成され、API 接続時に参照する。Finalize は GameRuntime と各 System の終了をまとめる。ProcessMessage は各 System に配送する。FixedUpdate／FrameExecute は System と GameRuntime の更新を分配する。CreateGameEngineAPI は Game に渡す公開面を構成する。TimeSystem::Update は FrameExecute 内で GameRuntime の Update より前に行う。FixedUpdate では固定更新用の時間値を更新してから GameRuntime の FixedUpdate を呼ぶ。その他の System の詳細な更新順序は未確定。

**今回の決定（U01）**：Engine が各 Boundary API を値として所有し、GameEngineAPI はそれらへのポインタを保持する。System と API は Engine の member として保持し、System の利用準備後に各 API::Initialize(XxxSystem*) で非所有ポインタを設定する。全 System に一律の Initialize を要求しない。これは更新された SubsystemApiFacadeDesign.md の方針に合わせる。

**今回の決定（U02）**：Create は引数なしで Engine を生成し、Engine::Initialize(applicationAPI) を呼んで初期化済みの EngineHandle を返す。Engine は Initialize 内で ApplicationAPI を値保持する。Destroy は Engine::Finalize() と delete までを担当する。Engine 自身の Initialize／Finalize は内部管理 API として残し、API テーブルには独立した Initialize／Finalize を置かない。

関係は以下のようになる。

```text
Application
    ↑
ApplicationAPI
    ↑
Hestia::Engine
    │
    ├─ AssetSystem
    ├─ GraphicsSystem
    ├─ InputSystem
    ├─ AudioSystem
    ├─ PhysicsSystem
    ├─ TimeSystem
    │
    ├─ GameEngineAPI
    │      ↓
    │   Game.dll
    │
    └─ GameRuntimeAPI
           ↓
       GameRuntime
```

Engine は HWND の member を持たず、必要な場面で ApplicationAPI::GetWindowHandle() を呼ぶ。GraphicsSystem への引数などとして受け渡すが、Graphics の詳細な保持方法は未確定。

方向としては、

```text
ApplicationAPI
Engine → Application

GameEngineAPI
Game.dll → Engine

GameRuntimeAPI
Engine → GameRuntime
```

GameRuntimeの破棄はGame.dllをUnloadする前に行う。

Destroy で GameRuntime を破棄し、そのハンドルと GameRuntimeAPI の参照を外してから Game.dll を Unload する。Facade は GameRuntime の終了処理で Unbind し、GameRuntime を破棄してから Game.dll を Unload する。

### EngineAPI
---
**役割**

ApplicationからEngine.dllを操作するためのAPIテーブル。
Engine.dllからは `GetEngineAPI` のみをC関数として公開し、Applicationは取得したAPIテーブル経由でEngineを操作する。

```cpp
namespace Hestia
{
    struct EngineAPI
    {
        using UpdateFunc = void (*)(EngineHandle, float);

        EngineHandle (*Create)(const ApplicationAPI*);
        void (*Destroy)(EngineHandle);

        void (*ProcessMessage)(
            EngineHandle,
            HWND,
            UINT,
            WPARAM,
            LPARAM);

        UpdateFunc FixedUpdate;
        UpdateFunc FrameExecute;
    };
}

extern "C"
{
    const Hestia::EngineAPI* GetEngineAPI();
}
```

[Engine](#engine)  
[EngineHandle](#enginehandle)  
[ApplicationAPI](./Application.md#applicationapi)

**所有・参照**：EngineAPI の関数テーブルは Engine.dll 側が保持する。Application は API テーブルを非所有で参照し、m_engine に EngineHandle を保持する。Create／Destroy によって Engine の寿命を管理する。

**今回の決定（U02）**：Create は new Engine() → Initialize(applicationAPI) → EngineHandle を返す、Destroy は Handle を Engine* に戻す → Finalize() → delete の順で処理する。GetEngineAPI はこの関数テーブルを返す入口であり、Engine の生成自体は Create の呼び出し時に行う。Application は EngineAPI::Initialize／Finalize を呼ばない。

Initialize の bool は原文の内部管理 API として残す。Create における失敗通知の詳細は現在決めず、専用 Result 型や追加状態を導入しない。

`GetProcAddress()` で取得するのは `GetEngineAPI` のみとし、それ以降のEngine操作は `EngineAPI` の関数ポインタを使用する。

### EngineHandle
---
**役割**

Application が Engine.dll 内の実体を保持し、API テーブルへ渡すための opaque handle。

```cpp
namespace Hestia
{
    using EngineHandle = void*;
}
```

[EngineAPI](#engineapi)  
[Application](./Application.md#application)

共有 API Header に alias を置く。Application は Engine の型定義を参照せず、Create で受け取った Handle を更新・メッセージ配送・Destroy の各 API に渡す。Engine.dll 内部では各 API の入口で static_cast<Engine*>(handle) に戻して操作する。

Engine の生成・初期化・終了・破棄は Engine.dll 内で行う。Application は Handle を通じて寿命を管理する。alias 自体に別種の void* と区別する機能は付けない（U15）。

### GameEngineAPI
---
**役割**

Game.dllから利用可能なEngine Subsystem APIをまとめて公開する。

```cpp
namespace Hestia
{
    struct GameEngineAPI
    {
        AssetAPI* m_assets = nullptr;
        GraphicsAPI* m_graphics = nullptr;
        InputAPI* m_input = nullptr;
        AudioAPI* m_audio = nullptr;
        PhysicsAPI* m_physics = nullptr;
        TimeAPI* m_time = nullptr;
        // ...
    };
}
```

[AssetAPI](./AssetSystem.md#assetapi)  
[GraphicsAPI](./Subsystems.md#graphicsapi)  
[InputAPI](./Subsystems.md#inputapi)  
[AudioAPI](./Subsystems.md#audioapi)  
[PhysicsAPI](./Subsystems.md#physicsapi)

各SubsystemがGame向けAPIを提供し、EngineがそれらをまとめてGame.dllへ渡す。

**今回の決定（U01）**：Engine.md 原文の値保持から、Engine 所有の API への非所有ポインタに変更した。CreateGameEngineAPI は Engine の各 API member のアドレスを格納する。通常 member の名前は NamingConvention に従う `m_lowerCamel` に統一する。

更新ノートでは version が削除され、通常 member の m_lowerCamel が揃っている（U11）。Game → Application の操作は別途公開 API を GameRuntime に渡す方向とし、この集約へ未確定の項目は加えない（U03）。

### GameRuntimeAPI
---
**役割**

EngineからGame.dll内部のGameRuntimeを生成・更新・破棄するためのDLL境界API。

```cpp
namespace Hestia
{
    struct GameRuntimeAPI
    {
        void* (*Create)(const GameEngineAPI*);
        void (*Destroy)(void*);

        void (*FixedUpdate)(void*);
        void (*Update)(void*);
    };
}

extern "C"
{
    __declspec(dllexport)
    const Hestia::GameRuntimeAPI* GetGameRuntimeAPI();
}
```

[GameEngineAPI](#gameengineapi)  
[GameRuntime](./GameRuntime.md#gameruntime)  
[HestiaGame::Time](./Time.md#hestiagametime)

`GameRuntime` 自体はGame.dll内部に隠し、Engine側では `void*` をopaque handleとして保持する。

`Create` はGameRuntimeの生成と初期化、`Destroy` は終了処理と破棄までを担当する。

`GameRuntimeAPI` の実体はGame.dll内の `static const` とし、Engineはそのポインタを非所有で保持する。

GameRuntimeの更新処理には `deltaTime` を直接渡さず、必要な時間情報は `HestiaGame::Time` から取得する。

GameRuntime の Facade Bind に TimeAPI を含める。GameRuntime の内部構造は仮置きを維持し、詳細は後続で検討する。
