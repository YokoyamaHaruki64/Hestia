# Engine Subsystems
---
ノートにある Graphics／Input／Audio／Physics の型名と三層の関係を整理する。
各 System の private s_instance、Boundary API のポインタ接続、Facade の friend は確認済み。以下の lifecycle・処理入口と内部状態の型は未確定の追加草案であり、一律の Initialize を要求しない。

## GraphicsSystem
---
**役割**

Engine の描画処理と描画状態を保持する。Game からの描画要求を受け、Engine のフレーム処理から描画を実行する。

```cpp
namespace Hestia
{
    class GraphicsSystem
    {
    public:
        bool Initialize(HWND window);
        void Shutdown();

        void BeginFrame();
        void Render();
        void Present();

    private:
        static GraphicsSystem* s_instance;

        // 未確定：Device、SwapChainなどの描画実体
        // 未確定：Gameから受け取る描画要求の格納メンバ
    };
}
```

[Engine](./Engine.md#engine)  
[GraphicsAPI](#graphicsapi)

**所有・参照**：Engine が System を値として所有する。System が GPU 描画に必要な実体と現在の描画要求を保持する追加草案。HWND は ApplicationAPI::GetWindowHandle() から取得して渡し、Application の Window を非所有で利用する。Engine 自体は HWND を保持しない。Boundary API は描画実体を所有しない。

**API とメンバ**：Initialize／Shutdown は描画実体の利用開始・終了、BeginFrame は今フレームの描画要求の準備、Render は要求の実行、Present は Window への表示に対応する。ノートはこの入口の選択を指定していないため、採用済み API ではない。

**未確定（U12）**：Game の描画要求の型、各 GPU 実体のメンバ型、描画方式、Game 向け公開操作はノートにない。

**提案**：次の設計で「Game が何を渡し、Graphics が何を描くか」を一つ決めてから、対応する要求型と Queue member を追加する。現在は RenderItem、RenderGraph、Pass、Material、Mesh の表現を過去資料から補わない。BeginFrame／Render／Present の三つの入口は見直せる仮置きとする。

### GraphicsAPI
---
**役割**

GraphicsSystem のうち Game に必要な操作だけを公開する Boundary API。

```cpp
namespace Hestia
{
    class ENGINE_API GraphicsAPI
    {
    public:
        void Initialize(GraphicsSystem* system);

        // 未確定：Gameから描画要求を渡すPublic API

    private:
        GraphicsSystem* m_system = nullptr;
    };
}
```

[GraphicsSystem](#graphicssystem)  
[GameEngineAPI](./Engine.md#gameengineapi)

**所有・参照**：Engine が所有し、m_system は非所有ポインタ。Render／Present は Engine が駆動する操作と考え、必要性がない段階で Game に公開しない。

**未確定（U12）**：Game からの描画要求が未定のため、操作の引数・返り値を宣言できない。

**提案**：要求型と同時に公開操作を定め、同じ操作を Graphics Facade に置く。

### Graphics
---
**役割**

Game Script の描画機能への入口。GraphicsAPI を非所有で参照する。

```cpp
class GameRuntime;

namespace HestiaGame
{
    class Graphics
    {
    public:
        // 未確定：Game向け描画API

    private:
        static void Bind(Hestia::GraphicsAPI* api);
        static void Unbind();

        inline static Hestia::GraphicsAPI* s_api = nullptr;

        friend class ::GameRuntime;
    };
}
```

[GraphicsAPI](#graphicsapi)  
[GameRuntime](./GameRuntime.md#gameruntime)

Bind／Unbind は private とし、GameRuntime のみ friend とする決定を反映した。s_api 以外に描画状態を持たない。

**未確定（U12）**：Script から利用する描画機能は未記載。

**提案**：GraphicsAPI の要求型を決める際に Facade の引数も決める。

## InputSystem
---
**役割**

Application から Engine に届く Window Message を受け、Game が読む入力状態を保持する。

```cpp
namespace Hestia
{
    class InputSystem
    {
    public:
        void Initialize();
        void Shutdown();

        void ProcessMessage(
            HWND hwnd,
            UINT message,
            WPARAM wParam,
            LPARAM lParam);

        void Update();

    private:
        static InputSystem* s_instance;

        // 未確定：キー・マウスなど、必要な入力状態のメンバ
    };
}
```

[Engine](./Engine.md#engine)  
[InputAPI](#inputapi)

**所有・参照**：Engine が値として所有する。現在の入力状態は System が所有し、API／Facade に同じ状態を複製しない。Window Message の値は ProcessMessage の入力であり、Window を所有しない。

**API とメンバ**：ProcessMessage は入力状態を更新する。Update は Game が読むフレームの状態を整える仮置き。メッセージ受付と状態のフレーム更新を分ける必要性は未確定。

**未確定（U12）**：入力デバイス、キー型、押下・解放・保持の区別、Game 更新との順序はノートにない。

**提案**：必要なキー／マウスの問い合わせを先に列挙し、そのための member と query API を設計する。InputSnapshot、Action Mapping、Raw Input 専用層などは先回りして追加しない。

### InputAPI
---
**役割**

Game が必要な入力状態を問い合わせる Boundary API。

```cpp
namespace Hestia
{
    class ENGINE_API InputAPI
    {
    public:
        void Initialize(InputSystem* system);

        // 未確定：入力状態の問い合わせPublic API

    private:
        InputSystem* m_system = nullptr;
    };
}
```

[InputSystem](#inputsystem)  
[GameEngineAPI](./Engine.md#gameengineapi)

**所有・参照**：Engine が所有し、m_system は非所有ポインタ。ProcessMessage は Application → Engine の経路に留める追加草案。

**未確定（U12）**：問い合わせに必要な入力型は未定。

**提案**：Game の入力要件を決めてから query API を宣言する。Game に入力状態の可変コンテナを公開しない。

### Input
---
**役割**

Game Script に InputAPI の問い合わせを static API として提供する。

```cpp
class GameRuntime;

namespace HestiaGame
{
    class Input
    {
    public:
        // 未確定：入力状態の問い合わせstatic API

    private:
        static void Bind(Hestia::InputAPI* api);
        static void Unbind();

        inline static Hestia::InputAPI* s_api = nullptr;

        friend class ::GameRuntime;
    };
}
```

[InputAPI](#inputapi)  
[GameRuntime](./GameRuntime.md#gameruntime)

s_api は非所有。Facade 自身は入力状態を所有しない。ノートの HestiaGame::Input という型名を引き継ぎ、宣言は追加草案とする。

**未確定（U12）**：公開操作が未定。

**提案**：InputAPI と同じ問い合わせを必要な範囲だけ公開する。

## AudioSystem
---
**役割**

Engine の音声再生に関する実体と状態を保持する。

```cpp
namespace Hestia
{
    class AudioSystem
    {
    public:
        void Initialize();
        void Update();
        void Shutdown();

    private:
        static AudioSystem* s_instance;

        // 未確定：音声出力の実体、再生中の音声状態のメンバ
    };
}
```

[Engine](./Engine.md#engine)  
[AudioAPI](#audioapi)

**所有・参照**：Engine が値として所有する。再生状態は System 側が持つ追加草案。音声データを AssetSystem とどう関係づけるかは未確定。

**API とメンバ**：Initialize／Shutdown は音声機能の開始・終了、Update は再生状態の更新を行う仮置き。再生要求・停止要求は Game の用途が決まった段階で追加する。

**未確定（U12）**：音声ライブラリ、データ型、再生 ID、更新の必要性はノートにない。

**提案**：まず Game の一つの再生用途を決め、必要な再生操作と状態 member を置く。Audio Voice 管理層や AssetHandle との結合を推測で設けない。

### AudioAPI
---
**役割**

Game に許可する音声操作を公開する Boundary API。

```cpp
namespace Hestia
{
    class ENGINE_API AudioAPI
    {
    public:
        void Initialize(AudioSystem* system);

        // 未確定：再生・停止などのPublic API

    private:
        AudioSystem* m_system = nullptr;
    };
}
```

[AudioSystem](#audiosystem)  
[GameEngineAPI](./Engine.md#gameengineapi)

**所有・参照**：Engine が所有し、m_system は非所有ポインタ。

**未確定（U12）**：共有する音声型と Game の操作が未定。

**提案**：System の再生用途が決まった時点で、Game が使う操作だけをここへ置く。

### Audio
---
**役割**

Game Script の音声操作を AudioAPI に委譲する static Facade の追加草案。

```cpp
class GameRuntime;

namespace HestiaGame
{
    class Audio
    {
    public:
        // 未確定：Game向け音声static API

    private:
        static void Bind(Hestia::AudioAPI* api);
        static void Unbind();

        inline static Hestia::AudioAPI* s_api = nullptr;

        friend class ::GameRuntime;
    };
}
```

[AudioAPI](#audioapi)  
[GameRuntime](./GameRuntime.md#gameruntime)

s_api は非所有。音声データや再生状態を Facade に置かない。

**未確定（U12）**：具体的な static API は未定。

**提案**：AudioAPI と同時に必要な操作だけを宣言する。

## PhysicsSystem
---
**役割**

Engine の物理状態を保持し、固定更新から物理処理を進める追加草案。

```cpp
namespace Hestia
{
    class PhysicsSystem
    {
    public:
        void Initialize();
        void FixedUpdate(float deltaTime);
        void Shutdown();

    private:
        static PhysicsSystem* s_instance;

        // 未確定：物理実体とBodyなどの状態メンバ
    };
}
```

[Engine](./Engine.md#engine)  
[PhysicsAPI](#physicsapi)

**所有・参照**：Engine が値として所有する。物理実体は System が保持する案。Game のオブジェクト／World との所有関係はノートにない。

**API とメンバ**：FixedUpdate は System の物理状態を一定刻みで進める仮置き。GameRuntime の FixedUpdate より前か後かは未確定。

**未確定（U12）**：物理ライブラリ、Body の識別・所有、Transform との同期、衝突通知は未記載。

**提案**：対象ゲームの物理用途を決めた時点で共有型と公開操作を定める。Jolt、ECS、World、BodyHandle、同期ジョブなどを過去の設計から引き継がない。

### PhysicsAPI
---
**役割**

Game に許可する物理操作を公開する Boundary API。

```cpp
namespace Hestia
{
    class ENGINE_API PhysicsAPI
    {
    public:
        void Initialize(PhysicsSystem* system);

        // 未確定：Gameが必要な物理操作のPublic API

    private:
        PhysicsSystem* m_system = nullptr;
    };
}
```

[PhysicsSystem](#physicssystem)  
[GameEngineAPI](./Engine.md#gameengineapi)

**所有・参照**：Engine が所有し、m_system は非所有ポインタ。物理実体の所有は System に留める案。

**未確定（U12）**：Game の要求する物理操作が未定。

**提案**：最初の用途に合わせて公開型と操作を決め、汎用 Physics Interface は先に作らない。

### Physics
---
**役割**

Game Script の物理操作を PhysicsAPI に委譲する static Facade の追加草案。

```cpp
class GameRuntime;

namespace HestiaGame
{
    class Physics
    {
    public:
        // 未確定：Game向け物理static API

    private:
        static void Bind(Hestia::PhysicsAPI* api);
        static void Unbind();

        inline static Hestia::PhysicsAPI* s_api = nullptr;

        friend class ::GameRuntime;
    };
}
```

[PhysicsAPI](#physicsapi)  
[GameRuntime](./GameRuntime.md#gameruntime)

s_api は非所有。物理状態を Facade に保持しない。

**未確定（U12）**：具体的な static API は未定。

**提案**：PhysicsAPI と同じ操作を必要な範囲だけ公開する。

private s_instance は各 System の自己参照用の非所有ポインタ。static 入口が未定の System にも置き、System 実体やその Getter は public に公開しない（U06）。登録・解除の具体的な時点は各 System の lifecycle 設計時に決める。

各 Boundary API は System の利用準備後に Initialize(XxxSystem*) で接続し、GameEngineAPI への格納後に GameRuntime へ渡す。Game 向けの機能 API は未確定のままにする（U12）。
