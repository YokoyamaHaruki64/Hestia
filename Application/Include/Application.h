#ifndef _APPLICATION_H_
#define _APPLICATION_H_

#include <cstdint>
#include <chrono>

#include "Window.h"


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

private:

    // FPS目標値のプロパティを設定する関数ポインタ
    void (*m_setTargetFPS)(void* context, double fps) = nullptr;
    double (*m_getTargetFPS)(void* context) = nullptr;

    // 無制限フレームレートのプロパティを設定する関数ポインタ
    void (*m_setUnlimitedFrameRate)(void* context, bool unlimited) = nullptr;
    bool (*m_isUnlimitedFrameRate)(void* context) = nullptr;

    // 固定更新時間のプロパティを設定する関数ポインタ
    void (*m_setFixedDeltaTime)(void* context, double deltaTime) = nullptr;
    double (*m_getFixedDeltaTime)(void* context) = nullptr;
    // アプリケーションの終了を要求する関数ポインタ
    void (*m_requestQuit)(void* context) = nullptr;

    // Applicationクラスのインスタンスポインタ
    // dll側でApplication型が漏れないようにvoid*で保持する
    void* m_context = nullptr;

    friend class Application;
};


class Application
{
	static constexpr double SPIN_WAIT_THRESHOLD = 0.0000005; // 500ns

    using Clock = std::chrono::steady_clock;
    Window m_window;
	int m_exitCode = 0;
    /*
    HMODULE m_engineModule = nullptr;
    const Hestia::EngineAPI* m_engineAPI = nullptr;
    Hestia::Engine* m_engine = nullptr;
    */

#if HESTIA_EDITOR
    Editor m_editor;
#endif

    double m_targetFPS = 120.0;
	double m_targetFrameTime = 1.0 / m_targetFPS;

    double m_fixedDeltaTime = 1.0 / 60.0;
    double m_accumulatedTime = 0.0;

    bool m_unlimitedFrameRate = false;
    bool m_running = false;

    Clock::time_point m_lastFrameTime;

    HANDLE m_frameTimer = nullptr;

    /*
    bool LoadEngine();
    void UnloadEngine();
    */

    ApplicationAPI CreateApplicationAPI();

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

