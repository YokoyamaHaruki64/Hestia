/*=============================================================================

 File   : InputSystem.h
 Desc   : ウィンドウの入力状態とカーソル設定を管理する。

------------------------------------------------------------------------------

 Date   : 2026/11/26
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _INPUT_SYSTEM_H_
#define _INPUT_SYSTEM_H_

#include <bitset>
#include <cstddef>

#include "API/InputAPI.h"
#include "Common/Include/WindowsHeaders.h"

namespace Hestia
{
    /// @brief App の更新スレッドでキーボード・マウス入力を保持する。
    class InputSystem
    {
    public:
        /// @brief マウスの Raw Input を登録する。
        /// @param window Application が所有する Window の HWND。Window がなければ nullptr
        /// @return 初期化成功: true / Raw Input 登録失敗: false
        static bool Initialize(HWND window) noexcept;
        /// @brief カーソル制限と Raw Input 登録を解除する。
        static void Finalize() noexcept;
        /// @brief キーの押下状態と遷移をすべてクリアする。
        static void Reset() noexcept;
        /// @brief ウィンドウメッセージから押下状態と遷移を反映する。
        static void KeyboardHook(UINT message, WPARAM wParam, LPARAM lParam) noexcept;
        /// @brief マウス通知とカーソル設定を処理する。
        /// @param result 処理済みのとき WindowProc が返す値
        /// @return 既定処理を省略する: true / 既定処理へ渡す: false
        static bool MouseHook(HWND window, UINT message, WPARAM wParam, LPARAM lParam, LRESULT& result) noexcept;
        /// @brief 現フレームの押下・解放の遷移とマウスの移動・スクロール量をクリアする。
        static void EndFrame() noexcept;
        /// @brief GameRuntime へ渡す入力 API テーブルを取得する。
        static const InputAPI& GetAPI() noexcept;

        static bool KeyHold(Key key) noexcept;
        static bool KeyDown(Key key) noexcept;
        static bool KeyRelease(Key key) noexcept;

    private:
        InputSystem() = delete;
        static void SetKey(std::size_t key, bool held) noexcept;
        static void ReadRawMouse(HRAWINPUT handle) noexcept;
        static void SetMouseButton(std::size_t button, bool held) noexcept;
        static void ApplyCursorClip() noexcept;
        static void ReleaseCursorClip() noexcept;
        static void UpdateCursorVisibility() noexcept;
        static bool IsCursorHidden() noexcept;

        static bool MouseHold(MouseButton button) noexcept;
        static bool MouseDown(MouseButton button) noexcept;
        static bool MouseRelease(MouseButton button) noexcept;
        static Vector2 MousePosition() noexcept;
        static Vector2 DeltaPosition() noexcept;
        static Vector2 DeltaScroll() noexcept;
        static void SetCursorLockMode(CursorLockMode mode) noexcept;
        static CursorLockMode GetCursorLockMode() noexcept;
        static void SetCursorVisible(bool visible) noexcept;
        static bool GetCursorVisible() noexcept;

        struct KeyboardState
        {
            std::bitset<256> m_held;
            std::bitset<256> m_down;
            std::bitset<256> m_released;
        };

        static constexpr std::size_t MOUSE_BUTTON_COUNT = static_cast<std::size_t>(MouseButton::MouseButtonCount);

        struct MouseState
        {
            std::bitset<MOUSE_BUTTON_COUNT> m_held;
            std::bitset<MOUSE_BUTTON_COUNT> m_down;
            std::bitset<MOUSE_BUTTON_COUNT> m_released;
            Vector2 m_deltaPosition;
            Vector2 m_deltaScroll;
        };

        static KeyboardState s_keyboard;
        static MouseState s_mouse;
        static HWND s_window;
        static CursorLockMode s_cursorMode;
        static bool s_cursorVisible;
        static bool s_hasFocus;
        static bool s_cursorClipped;
    };
}

#endif // _INPUT_SYSTEM_H_
