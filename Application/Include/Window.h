#ifndef _WINDOW_H_
#define _WINDOW_H_

#include <Windows.h>
#include <cstdint>
#include <string>

class Window
{
public:
    bool Initialize(
        int nCmdShow,
        WNDPROC windowProc,
        void* userData,
        const wchar_t* title,
        int width,
        int height);

    void Finalize();

    HWND GetHandle() const { return m_handle; }

private:
    HWND m_handle = nullptr;
    std::wstring m_title;
};

#endif // _WINDOW_H_
