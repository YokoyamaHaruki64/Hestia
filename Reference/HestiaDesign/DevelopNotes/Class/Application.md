# Application
---
Application.exeの中核となるクラス群。EngineHandle による生成・更新・破棄と時間管理をまとめる。
ApplicationがWindow・Engine・Editorのライフタイムとメインループを管理し、
Window Messageの取得・配送とEngineへの橋渡しを担当する。

## Application
---
**役割**

Window・Engine・Editorのライフタイムを保持し、アプリケーション全体のメインループ、Window Messageの配送、フレーム時間およびFixed Updateの時間管理を行う。

```cpp
class Application
{
public:
    bool Initialize();
    void Run();
    void Finalize();

    void SetTargetFPS(double fps);
    double GetTargetFPS() const;

    void SetUnlimitedFrameRate(bool unlimited);
    bool IsUnlimitedFrameRate() const;

    void SetFixedDeltaTime(double deltaTime);
    double GetFixedDeltaTime() const;

    void RequestQuit();

    HWND GetWindowHandle() const;

private:
    bool LoadEngine();
    void UnloadEngine();

    Hestia::ApplicationAPI CreateApplicationAPI();

    void ProcessMessages();
    void WaitUntil(Clock::time_point target);

    static LRESULT CALLBACK WindowProc(
        HWND hwnd,
        UINT message,
        WPARAM wParam,
        LPARAM lParam);

private:
    Window m_window;

    HMODULE m_engineModule = nullptr;
    const Hestia::EngineAPI* m_engineAPI = nullptr;
    Hestia::EngineHandle m_engine = nullptr;

#if HESTIA_EDITOR
    Editor m_editor;
#endif

    double m_targetFPS = 120.0;
    double m_targetFrameTime = 1.0 / 120.0;

    double m_fixedDeltaTime = 1.0 / 60.0;
    double m_accumulatedTime = 0.0;

    bool m_unlimitedFrameRate = false;
    bool m_running = false;

    Clock::time_point m_lastFrameTime;

    HANDLE m_frameTimer = nullptr;
};
```

[Clock](#clock)  
[Editor](./Editor.md#editor)  
[ApplicationAPI](#applicationapi)  
[Window](#window)  
[EngineHandle](./Engine.md#enginehandle)  
[EngineAPI](./Engine.md#engineapi)

**所有・参照**：Window と条件付き Editor は値として所有する。Engine.dll のロード状態と Engine の寿命は Application が管理し、EngineAPI は DLL 内テーブルを非所有で参照する。

Engine.dllはApplicationによってロード・破棄される。EngineAPI::Create は Engine の初期化まで、Destroy は終了処理と破棄までを行う。Application は DLL ロード → Create → 実行 → Destroy → DLL Unload の順で操作し、API テーブルの Initialize／Finalize は使用しない（今回の決定 U02）。ApplicationからEngineへの操作は `Hestia::EngineAPI`、EngineからApplicationへの操作は `Hestia::ApplicationAPI` を使用する。

**API とメンバ**：FPS 設定は `m_targetFPS` と `m_targetFrameTime`、Fixed 設定は `m_fixedDeltaTime`、Unlimited 設定は `m_unlimitedFrameRate`、終了要求は `m_running` を操作する。Run は `m_lastFrameTime`、`m_accumulatedTime`、`m_frameTimer` を使う。

時間計測・フレーム制御は `double` 精度で行い、Engine以下へ渡す際に `float` へ変換する。

フレーム先頭で `ProcessMessages()` により溜まったWindow Messageを処理する。フレーム待機中は高精度Waitable Timerと `MsgWaitForMultipleObjectsEx` を使用し、メッセージ到着時には起床して `ProcessMessages()` を実行する。

`ProcessMessages()` がMessage Queueを処理し、`DispatchMessage()` によって `Application::WindowProc()` を呼び出す。

`WindowProc()` は EngineHandle を使って EngineAPI::ProcessMessage へ通知し、Editor がある場合は Editor にも void の通知として配送する。Application 自身の処理を行い、最後に DefWindowProc の返り値を LRESULT として返す。Engine／Editor から処理結果を受け取る API は設けない。

目標時刻直前のみ短時間スピンし、スピン中に到着したメッセージは次フレーム先頭で処理する。

Unlimited時はフレーム待機のみを行わず、Fixed Updateの時間管理は通常通り継続する。

**決定（U03）**：Application が m_accumulatedTime に実経過時間を蓄積し、固定刻みを消費するたびに EngineAPI::FixedUpdate(m_engine, fixedDeltaTime) を呼ぶ。残りが固定刻み未満になるまで繰り返す。通常の Time 更新は Engine::FrameExecute に置き、Application は時間値の計測と渡し役に留まる。

**未確定（U08）**：Editor の初期化・フレーム・終了順序の詳細は後回しとする。

**提案**：Runtime と Game が形になってから、Editor の具体的な更新経路を検討する。

Engine の生成は LoadEngine にまとめる。

1. Engine.dll をロードし、m_engineModule にモジュールを保持する。
2. GetProcAddress で GetEngineAPI を取得し、その返り値を m_engineAPI に保持する。
3. CreateApplicationAPI で Application の操作入口と Context をまとめる。
4. m_engineAPI->Create に ApplicationAPI を渡し、初期化済み EngineHandle を m_engine に保持する。
5. Run 中は m_engine を各フレームの API とメッセージ通知へ渡す。
6. UnloadEngine は Destroy(m_engine) で終了・破棄を済ませ、その後 Engine.dll を Unload する。

GetEngineAPI 自体は関数テーブルを返すだけで、Create が Engine の生成と Initialize を行う。Destroy が Finalize と delete を行い、Application は Engine の型定義や内部処理にアクセスしない。

### ApplicationAPI
---
**役割**

EngineからApplicationのフレーム制御や終了要求を行うためのFacade。
内部でApplicationへのContextと関数ポインタを保持し、Engine側には通常のメンバ関数として公開する。

```cpp
class Application;

namespace Hestia
{
    struct ApplicationAPI
    {
    public:
        void SetTargetFPS(double fps) const;
        double GetTargetFPS() const;
    
        void SetUnlimitedFrameRate(bool unlimited) const;
        bool IsUnlimitedFrameRate() const;
    
        void SetFixedDeltaTime(double deltaTime) const;
        double GetFixedDeltaTime() const;
    
        void RequestQuit() const;
    
        HWND GetWindowHandle() const;
    
    private:
        using SetTargetFPSFunc = void (*)(void*, double);
        using GetTargetFPSFunc = double (*)(void*);
    
        using SetUnlimitedFrameRateFunc = void (*)(void*, bool);
        using IsUnlimitedFrameRateFunc = bool (*)(void*);
    
        using SetFixedDeltaTimeFunc = void (*)(void*, double);
        using GetFixedDeltaTimeFunc = double (*)(void*);
    
        using RequestQuitFunc = void (*)(void*);
        using GetWindowHandleFunc = HWND (*)(void*);
    
        void* m_context = nullptr;
    
        SetTargetFPSFunc m_setTargetFPS = nullptr;
        GetTargetFPSFunc m_getTargetFPS = nullptr;
    
        SetUnlimitedFrameRateFunc m_setUnlimitedFrameRate = nullptr;
        IsUnlimitedFrameRateFunc m_isUnlimitedFrameRate = nullptr;
    
        SetFixedDeltaTimeFunc m_setFixedDeltaTime = nullptr;
        GetFixedDeltaTimeFunc m_getFixedDeltaTime = nullptr;
    
        RequestQuitFunc m_requestQuit = nullptr;
        GetWindowHandleFunc m_getWindowHandle = nullptr;
    
        friend class ::Application;
    };
}
```

[Application](#application)

**所有・参照**：Application への `m_context` は非所有。各関数ポインタは Application の操作入口。Engine はこの Facade を値として保持する。

Applicationが生成時に自身をContextとして登録する。Engine側はContextを意識せず `ApplicationAPI::SetTargetFPS()` などを呼び出す。

**決定（U05／U14）**：ApplicationAPI は Hestia 名前空間に置く。GetWindowHandle は Application の Window の HWND を非所有で返す。Engine はこの API を必要な場面で呼び、HWND member を持たない。

Game → Application の操作は別途 API を GameRuntime に渡す方向とし、既存の ApplicationAPI をそのまま渡すか Game 向けに公開面を分けるかは未確定。

### Window
---
**役割**

Win32 Windowの生成・破棄と `HWND` の保持を担当する。

Window Messageの処理やMessage Pumpは持たず、Window生成時にApplicationの `WindowProc` を登録する。

```cpp
class Window
{
public:
    bool Initialize(
        WNDPROC windowProc,
        void* userData,
        const wchar_t* title,
        uint32_t width,
        uint32_t height);

    void Finalize();

    HWND GetHandle() const;

private:
    HWND m_handle = nullptr;
};
```

[Application](#application)

**所有・参照**：`m_handle` に対応する Window は Window が生成・破棄する。渡された `userData` は Application を指す非所有の関連付けであり、Window 自身は Application を所有しない。Initialize は `m_handle` を設定し、Finalize はその Window を破棄する。

Window生成時に渡された `WNDPROC` をWindow Classへ登録し、`userData` は `CreateWindowEx()` の生成引数として渡す。

Window Messageの流れは以下とする。

```text
Windows Message Queue
        ↓
Application::ProcessMessages()
        ↓
DispatchMessage()
        ↓
Application::WindowProc()
        ↓
Hestia::EngineAPI::ProcessMessage()
        ↓
Hestia::Engine
        ↓
各Subsystem
```

### Clock
---
**役割**

Application のフレーム時間と待機目標を表す時計の型。Clock 自体は時刻を所有せず、Application が time_point を保持する。

```cpp
using Clock = std::chrono::steady_clock;
```

[Application](#application)

**決定（U09）**：Clock は std::chrono::steady_clock の alias とする。フレーム待機の高精度 Waitable Timer とは別の時間計測用の型。
