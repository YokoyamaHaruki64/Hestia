/*=============================================================================

 File   : Application.h
 Desc   : Application クラスと実行時設定のインターフェースを宣言する。

------------------------------------------------------------------------------

 Date   : 2026/10/05
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _APPLICATION_H_
#define _APPLICATION_H_

#include <cstdint>
#include <chrono>

#include "ApplicationAPI.h"
#include "Window.h"

#include "Engine/Include/EngineAPI.h"

class Application
{
    static constexpr double SPIN_WAIT_THRESHOLD = 0.002; // 2ms

    inline static Application* s_instance = nullptr;

    using Clock = std::chrono::steady_clock;
    Window m_window;
	int m_exitCode = 0;

    HMODULE m_engineModule = nullptr;
    const Hestia::EngineAPI* m_engineAPI = nullptr;
    Hestia::EngineHandle m_engine = nullptr;

#if HESTIA_EDITOR
    // Editor m_editor;
#endif

    double m_targetFPS = 120.0;
	double m_targetFrameTime = 1.0 / m_targetFPS;

    double m_fixedDeltaTime = 1.0 / 60.0;
    double m_accumulatedTime = 0.0;

    bool m_unlimitedFrameRate = false;
    bool m_running = false;

    Clock::time_point m_lastFrameTime;

    HANDLE m_frameTimer = nullptr;

    bool LoadEngine();
    void UnloadEngine();

    Hestia::ApplicationAPI CreateApplicationAPI();

    void ProcessMessages();
    void WaitUntil(Clock::time_point target);
	Clock::duration ToDuration(double seconds) const
    { 
		return std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(seconds));
    }

public:

    bool Initialize(int nCmdShow);
    int Run();
    void Finalize();

    HWND GetWindowHandle() const { return m_window.GetHandle(); }

    void SetTargetFPS(double fps)
    {
        m_targetFPS = fps;
        m_targetFrameTime = 1.0 / m_targetFPS;
	}

    double GetTargetFPS() const{ return m_targetFPS; }

	void SetUnlimitedFrameRate(bool unlimited) { m_unlimitedFrameRate = unlimited; }
	bool IsUnlimitedFrameRate() const { return m_unlimitedFrameRate; }

	void SetFixedDeltaTime(double deltaTime) { m_fixedDeltaTime = deltaTime; }
	double GetFixedDeltaTime() const { return m_fixedDeltaTime; }

    void RequestQuit();

    static LRESULT CALLBACK WindowProc(
        HWND hwnd,
        UINT message,
        WPARAM wParam,
        LPARAM lParam);

};


#endif // _APPLICATION_H_
