# Time
---
Engine内部の時間状態と、Game Scriptから参照する時間APIを定義する。
Engineは `TimeSystem` を所有して更新し、Engine内部ではstatic Getter、Game.dll側では `HestiaGame::Time` から参照する。宣言は更新ノートに基づき、Getter の単純な本体は説明へまとめる。

## TimeSystem
---
**役割**

Engine内部の時間状態を保持・更新する。
更新はEngineが所有するインスタンスに対して行い、参照用の値はstatic Getterからも取得できる。

```cpp
namespace Hestia
{
    class TimeSystem
    {
    public:
        TimeSystem();
        ~TimeSystem();

        inline static float DeltaTime();

        inline static float UnscaledDeltaTime();

        inline static float TotalTime();

        inline static float UnscaledTotalTime();

        inline static float FixedDeltaTime();

        inline static float UnscaledFixedDeltaTime();

        inline static float TimeScale();

        void SetTimeScale(float scale);

        void Update(float deltaTime);
        void UpdateFixed(float fixedDeltaTime);

        inline const TimeData& GetData() const;

    private:
        TimeData m_data;

        static TimeSystem* s_instance;
    };
}
```

[TimeData](#timedata)  
[TimeAPI](#timeapi)

`Update()` はApplicationから渡された実時間をUnscaled値として保持し、`TimeScale` を適用したDeltaTimeとTotalTimeを更新する。

`UpdateFixed()` はFixed Updateに使用する時間値を更新する。TotalTimeは通常フレームの `Update()` 側でのみ加算する。

**所有・参照**：Engine が TimeSystem と TimeAPI を値として所有する。TimeSystem は TimeData を値保持し、private の s_instance は自身への非所有ポインタ。System 実体を公開する Getter は設けず、時間値の Getter を公開する。

**決定（U03）**：通常の Update は Engine::FrameExecute 内で GameRuntime::Update より前に行う。FixedUpdate の呼び出し回数は Application が蓄積時間を消費して決め、Engine の固定更新経路で UpdateFixed を呼ぶ。System の lifecycle に一律の Initialize は要求しない。

### TimeData
---
**役割**

高頻度に参照する時間値をまとめて保持する。
Game.dll側の `TimeAPI` から直接参照できるDLL境界共有データとする。

```cpp
namespace Hestia
{
    struct TimeData
    {
        float m_deltaTime = 0.0f;
        float m_unscaledDeltaTime = 0.0f;

        float m_totalTime = 0.0f;
        float m_unscaledTotalTime = 0.0f;

        float m_fixedDeltaTime = 0.0f;
        float m_unscaledFixedDeltaTime = 0.0f;

        float m_timeScale = 1.0f;
    };
}
```

[TimeSystem](#timesystem)

TimeData は TimeSystem が所有する。値は System の instance 状態であり、Game 側にコピーした別の時計を作らない。

### TimeAPI
---
**役割**

Game.dllからEngine側の時間情報へアクセスするためのDLL境界API。

Engineの初期化時に、初期化済みの `TimeSystem*` を設定する。
Getterは `TimeData` を直接参照し、DLL境界の関数呼び出しを行わない。
値を書き換える `SetTimeScale()` は `TimeSystem` へ処理を渡す。

```cpp
namespace Hestia
{
    class ENGINE_API TimeAPI
    {
    public:
        void Initialize(TimeSystem* system);

        inline float DeltaTime() const;

        inline float UnscaledDeltaTime() const;

        inline float TotalTime() const;

        inline float UnscaledTotalTime() const;

        inline float FixedDeltaTime() const;

        inline float UnscaledFixedDeltaTime() const;

        inline float TimeScale() const;

        void SetTimeScale(float scale);

    private:
        TimeSystem* m_system = nullptr;
        const TimeData* m_data = nullptr;
    };
}
```

[TimeSystem](#timesystem)  
[TimeData](#timedata)  
[HestiaGame::Time](#hestiagametime)

Engine.dll側では `Initialize()` で参照先を設定する。m_system と m_data はどちらも非所有。Getter は Header の inline 定義で m_data を読み、DLL 境界の関数呼び出しを行わない。上記は概念上の宣言だけを示す。

Initialize は m_system に対象 System、m_data に system->GetData() のアドレスを設定する。SetTimeScale は m_system に処理を渡す。

### HestiaGame::Time
---
**役割**

Game Scriptから時間情報へアクセスするためのstatic Facade。

```cpp
class GameRuntime;

namespace HestiaGame
{
    class Time
    {
    public:
        inline static float DeltaTime();

        inline static float UnscaledDeltaTime();

        inline static float TotalTime();

        inline static float UnscaledTotalTime();

        inline static float FixedDeltaTime();

        inline static float UnscaledFixedDeltaTime();

        inline static float TimeScale();

        inline static void SetTimeScale(float scale);

    private:
        inline static void Bind(Hestia::TimeAPI* api);

        inline static void Unbind();

        inline static Hestia::TimeAPI* s_api = nullptr;

        friend class ::GameRuntime;
    };
}
```

[TimeAPI](#timeapi)  
[GameRuntime](./GameRuntime.md#gameruntime)

**決定（U04）**：GameRuntime だけを friend とし、private Bind／Unbind で s_api を設定・解除する。Game Script は時間値 Getter と SetTimeScale を利用する。

参照経路は以下とする。

```text
HestiaGame::Time
        ↓ inline
Hestia::TimeAPI
        ↓ inline
Hestia::TimeData
```

`SetTimeScale()` のみEngine側へ処理を渡す。

```text
HestiaGame::Time::SetTimeScale()
        ↓
Hestia::TimeAPI::SetTimeScale()
        ↓ DLL Boundary
Hestia::TimeSystem::SetTimeScale()
```

**決定（U03）**：固定時間更新は TimeScale によって増減する。通常／固定更新の Scaled・Unscaled 値を区別するノートの方針を維持する。
