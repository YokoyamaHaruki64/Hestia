/*=============================================================================

 File   : Window.cpp
 Desc   : Win32 ウィンドウの生成、表示、終了処理を実装する。

------------------------------------------------------------------------------

 Date   : 2026/10/05
 Author : Yokoyama Haruki

=============================================================================*/

#include "pch.h"

#include "Window.h"

#include <limits>
#include <utility>

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

    // Editor はメニューバー分の高さも含めてウィンドウを作る
    HMENU menu = nullptr;
#if HESTIA_EDITOR
    menu = CreateMenu();
    if (menu == nullptr)
        return false;
#endif

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
        AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, menu != nullptr);

        m_handle = CreateWindowEx(0, title, title, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
            rc.right - rc.left, rc.bottom - rc.top, nullptr, menu, hInstance, nullptr);
    }

    if (m_handle == nullptr)
    {
        if (menu != nullptr)
            DestroyMenu(menu);
        UnregisterClass(m_title.c_str(), hInstance);
        return false;
    }

    // Editor の UI スレッドは STA で COM を初期化する
#if HESTIA_EDITOR
    const auto com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
#else
    const auto com = CoInitializeEx(nullptr, COINITBASE_MULTITHREADED);
#endif
    if (FAILED(com))
    {
        DestroyWindow(m_handle);
        m_handle = nullptr;
        UnregisterClass(m_title.c_str(), hInstance);
        return false;
    }

    ShowWindow(m_handle, nCmdShow);
    UpdateWindow(m_handle);

    return true;
}

void Window::Finalize()
{
    // ウィンドウに接続したメニューは DestroyWindow が解放する
    ClearMenuCommands();
    m_menuItems.clear();
    if (m_handle != nullptr)
    {
        DestroyWindow(m_handle);
        m_handle = nullptr;
    }
    UnregisterClass(m_title.c_str(), GetModuleHandle(nullptr));

    CoUninitialize();
}

bool Window::RegisterMenuItem(std::string_view path, void (*execute)(void*), void* context)
{
    HMENU menu = m_handle ? GetMenu(m_handle) : nullptr;
    if (menu == nullptr || execute == nullptr || path.empty() ||
        path.front() == '/' || path.back() == '/' || path.find("//") != std::string_view::npos ||
        path.find('\0') != std::string_view::npos || path.size() > std::numeric_limits<int>::max() ||
        m_menuCommands.size() >= std::numeric_limits<WORD>::max())
        return false;

    // UTF-8 の登録名を Win32 が受け取る UTF-16 に変換する
    const int byteCount = static_cast<int>(path.size());
    const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path.data(), byteCount, nullptr, 0);
    if (length == 0)
        return false;
    std::wstring widePath(length, L'\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path.data(), byteCount, widePath.data(), length) == 0)
        return false;

    // 共通のパスは既存のサブメニューを再利用する
    std::size_t start = 0;
    for (auto separator = widePath.find(L'/'); separator != std::wstring::npos; separator = widePath.find(L'/', start))
    {
        const auto prefix = widePath.substr(0, separator);
        const auto existing = m_menuItems.find(prefix);
        if (existing != m_menuItems.end())
        {
            if (existing->second == nullptr)
                return false;
            menu = existing->second;
        }
        else
        {
            const auto label = widePath.substr(start, separator - start);
            const auto submenu = CreatePopupMenu();
            if (submenu == nullptr)
                return false;
            if (!AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(submenu), label.c_str()))
            {
                DestroyMenu(submenu);
                return false;
            }
            m_menuItems.emplace(prefix, submenu);
            menu = submenu;
        }
        start = separator + 1;
    }

    // WM_COMMAND の ID はコールバックの添字に 1 を足した値を使う
    if (m_menuItems.contains(widePath))
        return false;
    const auto id = static_cast<UINT>(m_menuCommands.size() + 1);
    if (!AppendMenuW(menu, MF_STRING, id, widePath.c_str() + start))
        return false;
    m_menuCommands.push_back({ execute, context });
    m_menuItems.emplace(std::move(widePath), nullptr);
    DrawMenuBar(m_handle);
    return true;
}

bool Window::ExecuteMenuCommand(WORD id) const
{
    if (id == 0 || id > m_menuCommands.size())
        return false;

    const auto command = m_menuCommands[id - 1];
    command.m_execute(command.m_context);
    return true;
}

