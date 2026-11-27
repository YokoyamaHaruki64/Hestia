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
#include <map>
#include <string>
#include <string_view>
#include <vector>

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

    /// @brief パスの途中をサブメニューとし、末尾に実行項目を登録する。
    bool RegisterMenuItem(std::string_view path, void (*execute)(void*), void* context);

    /// @brief 登録済みのメニューコマンドを実行し、処理できた場合は true を返す。
    bool ExecuteMenuCommand(WORD id) const;

    /// @brief Engine のアンロード前にメニューのコールバックを破棄する。
    void ClearMenuCommands() noexcept { m_menuCommands.clear(); }

    HWND GetHandle() const { return m_handle; }

private:
    struct MenuCommand
    {
        void (*m_execute)(void*);
        void* m_context;
    };

    HWND m_handle = nullptr;
    std::wstring m_title;
    // 末端項目は nullptr として保持し、同じパスをサブメニューとして再登録できないようにする
    std::map<std::wstring, HMENU, std::less<>> m_menuItems;
    std::vector<MenuCommand> m_menuCommands;
};

#endif // _WINDOW_H_
