/*=============================================================================

 File   : Application.cpp
 Desc   : Application の初期化、メインループ、Engine連携を実装する。

------------------------------------------------------------------------------

 Date   : 2026/10/05
 Author : Yokoyama Haruki

=============================================================================*/

#include "pch.h"
#include "Application.h"

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

    if(!m_frameTimer)
        return false;

    if(!LoadEngine())
        return false;

    m_accumulatedTime = 0.0;
    m_lastFrameTime = Clock::now();
    m_running = true;

    s_instance = this;

    return true;
}

int Application::Run()
{
    while (m_running)
    {
        Clock::time_point currentTime = Clock::now();

        ProcessMessages();

        if (!m_running) break;

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
    // Windowのメッセージ処理
    // 基本的にEngineやEditorのメッセージ処理に委譲する

    if (s_instance && s_instance->m_engineAPI && s_instance->m_engine)
    {
        s_instance->m_engineAPI->ProcessMessage(s_instance->m_engine, hwnd, msg, wParam, lParam);
    }

    switch (msg)
    {
    case WM_CLOSE:
        DestroyWindow(hwnd);
        break;

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

