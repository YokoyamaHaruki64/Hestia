/*=============================================================================

 File   : Application.cpp
 Desc   : Application の初期化、メインループ、Engine連携を実装する。

------------------------------------------------------------------------------

 Date   : 2026/10/05
 Author : Yokoyama Haruki

=============================================================================*/

#include "pch.h"
#include "Application.h"

#include <format>

bool Application::Initialize(int nCmdShow)
{
    // Windowの初期化
    if (!m_window.Initialize(nCmdShow, WindowProc, this, L"Hestia Application", 1600, 900))
        return false;

    // Waitable Timer作成
    m_frameTimer = CreateWaitableTimerEx(
        nullptr,
        nullptr,
        CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
        TIMER_ALL_ACCESS
    );

    if (m_frameTimer == nullptr || !LoadEngine())
    {
        Finalize();
        return false;
    }

    m_accumulatedTime = 0.0;
    m_lastFrameTime = Clock::now();
    m_running = true;

    s_instance = this;

    return true;
}

int Application::Run()
{
    double titleSampleSeconds = 0.0;
    unsigned int titleFrameCount = 0;

    while (m_running)
    {
        ProcessMessages();

        if (!m_running) break;

        // メニューコマンドの処理時間を除いた時刻差を使う
        const auto currentTime = Clock::now();
        double deltaTime =
            std::chrono::duration_cast<std::chrono::duration<double>>(currentTime - m_lastFrameTime).count();

        m_accumulatedTime += deltaTime;
        m_lastFrameTime = currentTime;

        while (m_accumulatedTime >= m_fixedDeltaTime)
        {
            // Engineの固定更新処理
            m_engineAPI->FixedUpdate(m_engine, static_cast<float>(m_fixedDeltaTime));

            // Editorの固定更新処理

            m_accumulatedTime -= m_fixedDeltaTime;
        }

        // Engineの更新処理
        m_engineAPI->FrameExecute(m_engine, static_cast<float>(deltaTime));
        OutputDebugString((L"DeltaTime= " + std::to_wstring(deltaTime) + L"\n").c_str());
        // Editorの更新処理

        // 約1秒分のフレームを集計してタイトルに表示する
        titleSampleSeconds += deltaTime;
        ++titleFrameCount;
        if (titleSampleSeconds >= 0.25)
        {
            const double averageFrameTime = titleSampleSeconds / titleFrameCount;
            const auto title = std::format(L"FPS {:.1f} | FrameTime {:.2f} ms",
                1.0 / averageFrameTime, averageFrameTime * 1000.0);
            SetWindowTextW(m_window.GetHandle(), title.c_str());

            titleSampleSeconds = 0.0;
            titleFrameCount = 0;
        }

        // フレームレート制御
        if (!m_unlimitedFrameRate)
        {
            Clock::time_point targetTime = m_lastFrameTime + ToDuration(m_targetFrameTime);
            WaitUntil(targetTime);
        }

    }

    return m_exitCode;
}

void Application::Finalize()
{
    // Engine が Game と Graphics を終了するまで Window を維持する
    m_running = false;
    UnloadEngine();
    if (m_frameTimer != nullptr)
    {
        CloseHandle(m_frameTimer);
        m_frameTimer = nullptr;
    }

    if (s_instance == this)
        s_instance = nullptr;
    m_window.Finalize();
}

void Application::ProcessMessages()
{
    MSG msg;

    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
    {
        if (msg.message == WM_QUIT)
        {
            m_exitCode = static_cast<int>(msg.wParam);
            m_running = false;
        }

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

void Application::WaitUntil(Clock::time_point target)
{
    // 直前までWaitableTimerで待機し、残り時間がSPIN_WAIT_THRESHOLD以下になったらSpinWaitする
    auto spinTarget = target - ToDuration(SPIN_WAIT_THRESHOLD);
    auto now = Clock::now();

    // すでにtargetを過ぎている場合は即座に戻る
    if (now >= target)
        return;

    if (now < spinTarget)
    {
        while (true)
        {

            // すでにspinTargetを過ぎている場合はSpinWaitに移行する
            if (now >= spinTarget)
                break;

            LARGE_INTEGER dueTime;
            // dueTimeは100ナノ秒単位で指定する必要があるため、std::chrono::nanosecondsに変換してから100ナノ秒単位に変換する
            dueTime.QuadPart =
                -std::chrono::duration_cast<std::chrono::microseconds>(spinTarget - now).count() * 10; // 100ナノ秒単位に変換するために10を掛ける

            // WaitableTimerをセットする
            SetWaitableTimer(
                m_frameTimer,
                &dueTime,
                0,
                nullptr,
                nullptr,
                FALSE
            );

            // targetまではWaitableTimer
            // Timer または Window Message のどちらかで起床
            DWORD result = MsgWaitForMultipleObjectsEx(
                1,
                &m_frameTimer,
                INFINITE,
                QS_ALLINPUT,
                MWMO_INPUTAVAILABLE
            );

            // Timerが起床した場合はメッセージ処理してループ継続
            if (result == WAIT_OBJECT_0 + 1)
            {
                ProcessMessages();
                now = Clock::now();

                continue;
            }

            break;
        }
    }

    // targetまでSpinWait
    while (Clock::now() < target)
    {
    }

}

void Application::RequestQuit()
{
    PostQuitMessage(0);
}

LRESULT Application::WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    // App のメニュー操作は Window が登録したコマンドへ配送する
    if (msg == WM_COMMAND && HIWORD(wParam) == 0 && lParam == 0 && s_instance &&
        s_instance->m_window.GetHandle() == hwnd)
    {
        // コマンドがモーダル表示で待機しても、次のフレームで待機分を進めない
        const auto startedAt = Clock::now();
        if (s_instance->m_window.ExecuteMenuCommand(LOWORD(wParam)))
        {
            s_instance->m_lastFrameTime += Clock::now() - startedAt;
            return 0;
        }
    }

    // Windowのメッセージ処理
    // 基本的にEngineやEditorのメッセージ処理に委譲する

    if (s_instance && s_instance->m_engineAPI && s_instance->m_engine)
    {
        s_instance->m_engineAPI->ProcessMessage(s_instance->m_engine, hwnd, msg, wParam, lParam);
    }

    switch (msg)
    {
    case WM_CLOSE:
        // Engine の終了後に Finalize が Window を破棄する
        PostQuitMessage(0);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}


bool Application::LoadEngine()
{
    // Dllロード
    m_engineModule = LoadLibraryW(L"Engine.dll");
    if (!m_engineModule)
        return false;

    // GetEngineAPI関数の取得
    auto getEngineAPIFunc = reinterpret_cast<const Hestia::EngineAPI*(*)()>(
        GetProcAddress(m_engineModule, "GetEngineAPI")
    );
    if (!getEngineAPIFunc)
        return false;

    // EngineAPIの取得
    m_engineAPI = getEngineAPIFunc();
    if (!m_engineAPI)
        return false;

    // Engineを初期化して作成
    Hestia::ApplicationAPI appAPI = CreateApplicationAPI();
    m_engine = m_engineAPI->Create(&appAPI);
    if (!m_engine)
        return false;

    return true;
}

void Application::UnloadEngine()
{
    // メニューは Engine.dll の関数を借用しているため、先に解除する
    m_window.ClearMenuCommands();
    if (m_engine)
    {
        if (m_engineAPI)
            m_engineAPI->Destroy(m_engine);
        m_engine = nullptr;
    }

    m_engineAPI = nullptr;

    if (m_engineModule)
    {
        FreeLibrary(m_engineModule);
        m_engineModule = nullptr;
    }
}

