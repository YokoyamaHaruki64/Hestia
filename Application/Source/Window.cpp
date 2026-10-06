#include "pch.h"

#include "Window.h"
#include "Application.h"

bool Window::Initialize(
	int nCmdShow,
	WNDPROC windowProc,
	void* userData,
	const wchar_t* title,
	int width,
	int height)
{
	HINSTANCE hInstance = GetModuleHandle(nullptr);
	m_title = title;

	// ウィンドウクラスの登録
	WNDCLASSEX wcex;
	{
		wcex.cbSize = sizeof(WNDCLASSEX);
		wcex.style = 0;
		wcex.lpfnWndProc = windowProc;
		wcex.cbClsExtra = 0;
		wcex.cbWndExtra = 0;
		wcex.hInstance = hInstance;
		wcex.hIcon = nullptr;
		wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
		wcex.hbrBackground = nullptr;
		wcex.lpszMenuName = nullptr;
		wcex.lpszClassName = title;
		wcex.hIconSm = nullptr;

		RegisterClassEx(&wcex);


		RECT rc = { 0, 0, width, height };
		AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

		m_handle = CreateWindowEx(0, title, title, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
			rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInstance, nullptr);
	}

	CoInitializeEx(nullptr, COINITBASE_MULTITHREADED);

	ShowWindow(m_handle, nCmdShow);
	UpdateWindow(m_handle);

	return true;
}

void Window::Finalize()
{
	UnregisterClass(m_title.c_str(), GetModuleHandle(nullptr));

	CoUninitialize();
}