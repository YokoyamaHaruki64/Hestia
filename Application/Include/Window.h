/*=============================================================================

 File   : Window.h
 Desc   : Win32 ウィンドウの生成と基本操作を行う Window クラスを宣言する。

------------------------------------------------------------------------------

 Date   : 2026/10/05
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _WINDOW_H_
#define _WINDOW_H_

#include "Common/Include/WindowsHeaders.h"
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
