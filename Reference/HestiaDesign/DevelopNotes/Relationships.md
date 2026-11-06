# クラス間の関係
---
ClassDesign の指針・Class 配下の宣言とは別に、所有・参照・呼び出しの関係を示す。
図は更新ノートと確認済みの決定を反映する。各 System の内部状態・具体的な機能 API は未確定。

## 所有と非所有参照
---
`*--` は所有・寿命管理、`-->` は非所有参照、`..>` は API を通じた呼び出しを表す。GameRuntime の実メモリは Game.dll 内で生成・破棄する。

```mermaid
classDiagram
    class Application
    class Window
    class Editor
    class ApplicationAPI
    class EngineAPI
    class EngineHandle {
        <<Alias>>
    }
    class TimeSystem {
        <<Hestia>>
    }
    class TimeData {
        <<Hestia>>
    }
    class TimeAPI {
        <<Hestia>>
    }
    class GameTime {
        <<HestiaGame>>
    }
    class Engine {
        <<Hestia>>
    }
    class GameEngineAPI {
        <<Hestia>>
    }
    class GameRuntimeAPI {
        <<Hestia>>
    }
    class GameRuntime
    class AssetSystem {
        <<Hestia>>
    }
    class GraphicsSystem {
        <<Hestia>>
    }
    class InputSystem {
        <<Hestia>>
    }
    class AudioSystem {
        <<Hestia>>
    }
    class PhysicsSystem {
        <<Hestia>>
    }
    class AssetAPI {
        <<Hestia>>
    }
    class GraphicsAPI {
        <<Hestia>>
    }
    class InputAPI {
        <<Hestia>>
    }
    class AudioAPI {
        <<Hestia>>
    }
    class PhysicsAPI {
        <<Hestia>>
    }
    class Assets {
        <<HestiaGame>>
    }
    class Graphics {
        <<HestiaGame>>
    }
    class Input {
        <<HestiaGame>>
    }
    class Audio {
        <<HestiaGame>>
    }
    class Physics {
        <<HestiaGame>>
    }

    Application *-- Window
    Application *-- Editor : HESTIA_EDITOR
    Application *-- Engine : EngineHandle / Create / Destroy
    Application --> EngineAPI : DLL table
    Application --> EngineHandle : m_engine
    EngineAPI ..> EngineHandle : arguments / return
    Application ..> ApplicationAPI : create
    Engine *-- ApplicationAPI : value
    ApplicationAPI --> Application : context

    Engine *-- AssetSystem
    Engine *-- GraphicsSystem
    Engine *-- InputSystem
    Engine *-- AudioSystem
    Engine *-- PhysicsSystem
    Engine *-- TimeSystem
    TimeSystem *-- TimeData : m_data

    Engine *-- AssetAPI
    Engine *-- GraphicsAPI
    Engine *-- InputAPI
    Engine *-- AudioAPI
    Engine *-- PhysicsAPI
    Engine *-- TimeAPI
    TimeAPI --> TimeSystem : m_system
    TimeAPI --> TimeData : const m_data
    AssetAPI --> AssetSystem : m_system
    GraphicsAPI --> GraphicsSystem : m_system
    InputAPI --> InputSystem : m_system
    AudioAPI --> AudioSystem : m_system
    PhysicsAPI --> PhysicsSystem : m_system

    Engine *-- GameEngineAPI : value
    GameEngineAPI --> AssetAPI : m_assets
    GameEngineAPI --> GraphicsAPI : m_graphics
    GameEngineAPI --> InputAPI : m_input
    GameEngineAPI --> AudioAPI : m_audio
    GameEngineAPI --> PhysicsAPI : m_physics
    GameEngineAPI --> TimeAPI : m_time

    Engine --> GameRuntimeAPI : DLL table
    Engine *-- GameRuntime : opaque handle lifetime
    Engine ..> GameRuntimeAPI : Create / Update / Destroy

    Assets --> AssetAPI : s_api
    Graphics --> GraphicsAPI : s_api
    Input --> InputAPI : s_api
    Audio --> AudioAPI : s_api
    Physics --> PhysicsAPI : s_api
    GameTime --> TimeAPI : s_api
```

[Application](./Class/Application.md)  
[Engine](./Class/Engine.md)  
[AssetSystem／API／Facade](./Class/AssetSystem.md)  
[その他 System／API／Facade](./Class/Subsystems.md)  
[Time](./Class/Time.md)  
[EngineHandle](./Class/Engine.md#enginehandle)

Engine が Boundary API を所有し、API は Initialize(XxxSystem*) で非所有ポインタを受け取る。Application が持つ EngineHandle は void* の alias であり、Engine の型定義を共有 Header に公開しない。Facade は Game.dll 内の static ポインタを設定して利用する。GameTime は HestiaGame::Time を表す。

**未確定（U08）**：Editor の実装先 DLL と Engine／Game の情報経路はノートにない。

**提案**：この図では Application の所有だけを示す。情報経路は編集する対象が決まってから追加する。

## DLL 越しの操作方向
---
```mermaid
flowchart LR
    Application["Application.exe / Application"]
    Engine["Engine.dll / Hestia::Engine"]
    Game["Game.dll / GameRuntime"]
    Application -->|"EngineAPI: Create・Destroy・ProcessMessage・FixedUpdate・FrameExecute"| Engine
    Engine -->|"ApplicationAPI: FPS・Unlimited・Fixed設定・Quit・HWND取得"| Application
    Engine -->|"GameRuntimeAPI: Create・Destroy・FixedUpdate・Update"| Game
    Game -->|"Facade → Boundary API"| Engine
    Game -.->|"別APIを渡す方向・具体形は未確定"| Application
```

[ApplicationAPI](./Class/Application.md#applicationapi)  
[EngineAPI](./Class/Engine.md#engineapi)  
[GameRuntimeAPI](./Class/Engine.md#gameruntimeapi)  
[SubsystemApiFacadeDesign](./SubsystemApiFacadeDesign.md)

EngineAPI::Create は Engine の Initialize、Destroy は Finalize までを含む。Initialize／Finalize の独立した API テーブル項目は今回の決定 U02 により置かない。

## 生成・終了の通常経路
---
```mermaid
sequenceDiagram
    participant App as Application
    participant EA as EngineAPI
    participant E as Engine
    participant GA as GameRuntimeAPI
    participant G as GameRuntime
    participant F as Game Facades

    App->>App: Engine.dll ロード
    App->>EA: GetEngineAPI()
    EA-->>App: EngineAPI table
    App->>App: ApplicationAPI 作成
    App->>EA: Create(ApplicationAPI)
    EA->>E: new Engine(), Initialize(ApplicationAPI)
    E->>E: System 利用準備、API.Initialize(System*)
    E->>E: GameEngineAPI に各 API pointer を設定
    E->>GA: Create(GameEngineAPI)
    GA->>G: 生成・初期化
    G->>F: private Bind(API pointers) via friend
    EA-->>App: EngineHandle
    App->>EA: FixedUpdate / FrameExecute(EngineHandle, dt)
    EA->>E: 更新
    E->>GA: FixedUpdate / Update
    GA->>G: 更新
    App->>EA: Destroy(EngineHandle)
    EA->>E: Finalize()
    E->>GA: Destroy(GameRuntime)
    GA->>G: 終了処理
    G->>F: private Unbind() via friend
    GA->>G: 破棄
    E->>E: Game.dll Unload
    EA->>E: delete Engine
    App->>App: Engine.dll Unload
```

[GameRuntime](./Class/GameRuntime.md)  
[Engine](./Class/Engine.md)

各 EngineAPI の入口で EngineHandle を Engine* に戻して内部処理を呼ぶ。Engine 内部の Initialize／Finalize は残し、Create／Destroy がそれらをまとめて呼ぶ。

## Time の接続
---
更新ノートの所有・参照と、通常の更新を FrameExecute に置く決定を示す。

```mermaid
flowchart LR
    App["Application: double時間・Fixed蓄積"]
    E["Engine: FrameExecute / FixedUpdate"]
    T["TimeSystem"]
    D["TimeData"]
    API["TimeAPI"]
    GAPI["GameEngineAPI"]
    F["HestiaGame::Time"]
    G["GameRuntime"]
    App -->|"計測値・固定刻み"| E
    E -->|"FrameExecute: Update / FixedUpdate: UpdateFixed"| T
    T -->|"m_data を所有・更新"| D
    API -->|"m_system 非所有"| T
    API -->|"const m_data / inline Getter"| D
    GAPI -->|"m_time 非所有"| API
    F -->|"s_api 非所有 / private Bind"| API
    G -->|"時間取得・SetTimeScale"| F
```

[Time](./Class/Time.md)  
[OpenQuestions U03](./OpenQuestions.md#u03time-とフレーム順序)

TotalTime は TimeSystem::Update でのみ加算する。通常の時間更新は FrameExecute 内で GameRuntime::Update より前に行う。FixedUpdate の回数は Application が蓄積時間を消化して決める。

## 固定更新とフレーム更新
---
```mermaid
sequenceDiagram
    participant A as Application
    participant API as EngineAPI
    participant E as Engine
    participant T as TimeSystem
    participant G as GameRuntime
    A->>A: 実経過時間を m_accumulatedTime に加算
    loop 固定刻みを消費できる間
        A->>API: FixedUpdate(EngineHandle, fixedDeltaTime)
        API->>E: FixedUpdate(fixedDeltaTime)
        E->>T: UpdateFixed(fixedDeltaTime)
        E->>G: GameRuntimeAPI::FixedUpdate
        A->>A: m_accumulatedTime から固定刻みを減算
    end
    A->>API: FrameExecute(EngineHandle, deltaTime)
    API->>E: FrameExecute(deltaTime)
    E->>T: Update(deltaTime)
    E->>G: GameRuntimeAPI::Update
```

図では固定更新の消化 → FrameExecute の通常経路を示す。TimeSystem::Update の位置は FrameExecute 内とし、各 System の詳細な前後関係はまだ決めない。

**決定（U03）**：固定時間更新は TimeScale によって増減する。

**未確定**：Game から Application へ渡す API の具体形は未確定。

**提案**：各 System の用途と Game から必要な Application 操作を整理する段階で、対応する経路を確定する。
