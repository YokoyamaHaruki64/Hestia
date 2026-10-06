#include "pch.h"
#include "Application.h"

bool Application::Initialize(int nCmdShow)
{
	// Windowの初期化
	if (!m_window.Initialize(nCmdShow, WindowProc, this, L"Hestia Application", 1600, 900))
	{
		return false;
	}

	// Waitable Timer作成
	m_frameTimer = CreateWaitableTimerEx(
		nullptr,
		nullptr,
		CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
		TIMER_ALL_ACCESS
	);

	m_accumulatedTime = 0.0;
	m_lastFrameTime = Clock::now();
	m_running = true;

	return true;
}

int Application::Run()
{
	while (m_running)
	{
		Clock::time_point currentTime = Clock::now();

		if (!m_running) break;

		double deltaTime = 
			std::chrono::duration_cast<std::chrono::duration<double>>(currentTime - m_lastFrameTime).count();

		m_accumulatedTime += deltaTime;
		m_lastFrameTime = currentTime;

		while( m_accumulatedTime >= m_fixedDeltaTime)
		{
			// Engineの固定更新処理
			
			// Editorの固定更新処理

			m_accumulatedTime -= m_fixedDeltaTime;
		}

		// Engineの更新処理

		// Editorの更新処理


		// フレームレート制御
		if(!m_unlimitedFrameRate)
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
		if( msg.message == WM_QUIT)
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
	{
		return;
	}

	if( now < spinTarget)
	{
		while (true)
		{

			// すでにspinTargetを過ぎている場合はSpinWaitに移行する
			if (now >= spinTarget)
				break;

			LARGE_INTEGER dueTime;
			// dueTimeは100ナノ秒単位で指定する必要があるため、std::chrono::nanosecondsに変換してから100ナノ秒単位に変換する
			dueTime.QuadPart = 
				-std::chrono::duration_cast<std::chrono::microseconds>(target - now).count() * 10; // 100ナノ秒単位に変換するために10を掛ける

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
				continue;
			}

			break;
		}
	}
	else
	{
		// targetまでSpinWait
		while (Clock::now() < target)
		{
		}
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

	/*
	* m_engineAPI->ProcessWindowMessage(m_engine, hwnd, message, wParam, lParam);
	* */

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


ApplicationAPI Application::CreateApplicationAPI()
{
	ApplicationAPI api;

	api.m_context = this;

	// 関数ポインタを設定

	api.m_setTargetFPS = [](void* context, double fps)
	{
		static_cast<Application*>(context)->SetTargetFPS(fps);
	};

	api.m_getTargetFPS = [](void* context)
	{
		return static_cast<Application*>(context)->GetTargetFPS();
	};

	api.m_setUnlimitedFrameRate = [](void* context, bool unlimited)
	{
		static_cast<Application*>(context)->SetUnlimitedFrameRate(unlimited);
	};

	api.m_isUnlimitedFrameRate = [](void* context)
	{
		return static_cast<Application*>(context)->IsUnlimitedFrameRate();
	};

	api.m_setFixedDeltaTime = [](void* context, double deltaTime)
	{
		static_cast<Application*>(context)->SetFixedDeltaTime(deltaTime);
	};

	api.m_getFixedDeltaTime = [](void* context)
	{
		return static_cast<Application*>(context)->GetFixedDeltaTime();
	};

	api.m_requestQuit = [](void* context)
	{
		static_cast<Application*>(context)->RequestQuit();
	};

	return api;
}

void ApplicationAPI::SetTargetFPS(double fps) const
{
	if (m_setTargetFPS)
	{
		m_setTargetFPS(m_context, fps);
	}
}
double ApplicationAPI::GetTargetFPS() const
{
	if (m_getTargetFPS)
	{
		return m_getTargetFPS(m_context);
	}

	return 0.0;
}

void ApplicationAPI::SetUnlimitedFrameRate(bool unlimited) const
{
	if (m_setUnlimitedFrameRate)
	{
		m_setUnlimitedFrameRate(m_context, unlimited);
	}
}
bool ApplicationAPI::IsUnlimitedFrameRate() const
{
	if (m_isUnlimitedFrameRate)
	{
		return m_isUnlimitedFrameRate(m_context);
	}
	return false;
}

void ApplicationAPI::SetFixedDeltaTime(double deltaTime) const
{
	if (m_setFixedDeltaTime)
	{
		m_setFixedDeltaTime(m_context, deltaTime);
	}
}
double ApplicationAPI::GetFixedDeltaTime() const
{
	if (m_getFixedDeltaTime)
	{
		return m_getFixedDeltaTime(m_context);
	}
	return 0.0;
}

void ApplicationAPI::RequestQuit() const
{
	if (m_requestQuit)
	{
		m_requestQuit(m_context);
	}
}