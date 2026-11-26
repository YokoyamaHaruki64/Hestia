/*=============================================================================

 File   : InputSystem.cpp
 Desc   : キーボード・マウスの通知とカーソル制御を処理する。

------------------------------------------------------------------------------

 Date   : 2026/11/26
 Author : Yokoyama Haruki

=============================================================================*/

#include "pch.h"
#include "Input/InputSystem.h"

namespace Hestia
{
    namespace
    {
        // Shift / Control / Alt の左右を区別せず、共通の修飾キーとして扱うための構造体
        struct ModifierKeys
        {
            UINT m_combined;
            UINT m_left;
            UINT m_right;
        };

        constexpr ModifierKeys MODIFIERS[] = {
            { VK_SHIFT, VK_LSHIFT, VK_RSHIFT },
            { VK_CONTROL, VK_LCONTROL, VK_RCONTROL },
            { VK_MENU, VK_LMENU, VK_RMENU },
        };

        // Raw Input の Generic Desktop / Mouse Usage
        constexpr USHORT MOUSE_USAGE_PAGE = 0x01;
        constexpr USHORT MOUSE_USAGE = 0x02;
    }

    InputSystem::KeyboardState InputSystem::s_keyboard{};
    InputSystem::MouseState InputSystem::s_mouse{};
    HWND InputSystem::s_window = nullptr;
    CursorLockMode InputSystem::s_cursorMode = CursorLockMode::None;
    bool InputSystem::s_cursorVisible = true;
    bool InputSystem::s_hasFocus = false;
    bool InputSystem::s_cursorClipped = false;

    bool InputSystem::Initialize(HWND window) noexcept
    {
        Reset();

        // Window なしで動く構成では Raw Input の登録を省く
        if (window == nullptr)
            return true;

        // 通常のマウスメッセージを残し、フォアグラウンド入力だけを登録する
        const RAWINPUTDEVICE device{ MOUSE_USAGE_PAGE, MOUSE_USAGE, 0, window };
        if (!RegisterRawInputDevices(&device, 1, sizeof(device)))
            return false;

        // Window は Engine の起動前にフォーカスされていることがある
        s_window = window;
        s_hasFocus = GetFocus() == window;
        return true;
    }

    void InputSystem::Finalize() noexcept
    {
        if (s_window != nullptr)
        {
            // HWND を手放す前に、カーソルと Raw Input を OS に戻す
            const bool wasHidden = IsCursorHidden();
            s_hasFocus = false;
            ReleaseCursorClip();
            if (wasHidden)
                UpdateCursorVisibility();

            const RAWINPUTDEVICE device{ MOUSE_USAGE_PAGE, MOUSE_USAGE, RIDEV_REMOVE, nullptr };
            RegisterRawInputDevices(&device, 1, sizeof(device));
            s_window = nullptr;
        }

        Reset();
        s_mouse = {};
        s_cursorMode = CursorLockMode::None;
        s_cursorVisible = true;
    }

    void InputSystem::Reset() noexcept
    {
        s_keyboard = {};
    }

    void InputSystem::KeyboardHook(UINT message, WPARAM wParam, LPARAM lParam) noexcept
    {
        // フォーカスを失うと解放通知が届かないことがあるため状態をリセットする
        if (message == WM_KILLFOCUS)
        {
            Reset();
            return;
        }
        const bool held = message == WM_KEYDOWN || message == WM_SYSKEYDOWN;
        if ((!held && message != WM_KEYUP && message != WM_SYSKEYUP) || wParam >= s_keyboard.m_held.size())
            return;

        // 修飾キーだけ左右を解決し、片側が押されている間は共通キーも保持する
        auto key = static_cast<UINT>(wParam);
        for (const auto& modifier : MODIFIERS)
        {
            if (key == modifier.m_combined)
            {
                const auto flags = HIWORD(lParam);
                const UINT scanCode = MAKEWORD(LOBYTE(flags), (flags & KF_EXTENDED) ? 0xE0 : 0);
                key = MapVirtualKeyW(scanCode, MAPVK_VSC_TO_VK_EX);
                if (key != modifier.m_left && key != modifier.m_right)
                    return;
            }
            if (key == modifier.m_left || key == modifier.m_right)
            {
                SetKey(key, held);
                SetKey(modifier.m_combined, s_keyboard.m_held[modifier.m_left] || s_keyboard.m_held[modifier.m_right]);
                return;
            }
        }
        SetKey(key, held);
    }

    void InputSystem::SetKey(std::size_t key, bool held) noexcept
    {
        // 実際の状態変化だけを記録し、同じフレームの押下と解放を両方残す
        if (s_keyboard.m_held[key] == held)
            return;

        s_keyboard.m_held[key] = held;
        (held ? s_keyboard.m_down : s_keyboard.m_released)[key] = true;
    }

    bool InputSystem::MouseHook(HWND window, UINT message, WPARAM wParam, LPARAM lParam, LRESULT& result) noexcept
    {
        if (s_window == nullptr || window != s_window)
            return false;

        switch (message)
        {
        case WM_INPUT:
            // 読み取り後も DefWindowProc に渡し、OS に後処理をさせる
            if (s_hasFocus && GET_RAWINPUT_CODE_WPARAM(wParam) == RIM_INPUT)
                ReadRawMouse(reinterpret_cast<HRAWINPUT>(lParam));
            break;

        case WM_KILLFOCUS:
            // 設定値を残し、解放通知の欠落と別ウィンドウ側への制限を防ぐ
            s_hasFocus = false;
            s_mouse = {};
            ReleaseCursorClip();
            UpdateCursorVisibility();
            break;

        case WM_SETFOCUS:
            s_hasFocus = true;
            ApplyCursorClip();
            UpdateCursorVisibility();
            break;

        case WM_WINDOWPOSCHANGED:
            // 移動・リサイズでクライアント領域のスクリーン座標が変わる
            if (s_hasFocus && s_cursorMode != CursorLockMode::None)
            {
                ApplyCursorClip();
                UpdateCursorVisibility();
            }
            break;

        case WM_SETCURSOR:
            if (reinterpret_cast<HWND>(wParam) == window && LOWORD(lParam) == HTCLIENT && IsCursorHidden())
            {
                SetCursor(nullptr);
                result = TRUE;
                return true;
            }
            break;
        }
        return false;
    }

    void InputSystem::ReadRawMouse(HRAWINPUT handle) noexcept
    {
        RAWINPUT input{};
        UINT size = sizeof(input);
        const UINT read = GetRawInputData(handle, RID_INPUT, &input, &size, sizeof(RAWINPUTHEADER));
        if (read != sizeof(RAWINPUTHEADER) + sizeof(RAWMOUSE) || input.header.dwType != RIM_TYPEMOUSE)
            return;

        // 絶対座標のデバイスもボタンとホイールは共通に処理する
        const RAWMOUSE& mouse = input.data.mouse;
        if ((mouse.usFlags & MOUSE_MOVE_ABSOLUTE) == 0)
        {
            s_mouse.m_deltaPosition.x += static_cast<float>(mouse.lLastX);
            s_mouse.m_deltaPosition.y += static_cast<float>(mouse.lLastY);
        }

        const USHORT flags = mouse.usButtonFlags;
        const float scroll = static_cast<float>(static_cast<SHORT>(mouse.usButtonData)) / WHEEL_DELTA;
        if (flags & RI_MOUSE_HWHEEL)
            s_mouse.m_deltaScroll.x += scroll;
        if (flags & RI_MOUSE_WHEEL)
            s_mouse.m_deltaScroll.y += scroll;

        // MouseButton の並びに対応させ、短いクリックの両方の遷移を記録する
        constexpr USHORT buttonFlags[MOUSE_BUTTON_COUNT][2] = {
            { RI_MOUSE_LEFT_BUTTON_DOWN, RI_MOUSE_LEFT_BUTTON_UP },
            { RI_MOUSE_RIGHT_BUTTON_DOWN, RI_MOUSE_RIGHT_BUTTON_UP },
            { RI_MOUSE_MIDDLE_BUTTON_DOWN, RI_MOUSE_MIDDLE_BUTTON_UP },
            { RI_MOUSE_BUTTON_4_DOWN, RI_MOUSE_BUTTON_4_UP },
            { RI_MOUSE_BUTTON_5_DOWN, RI_MOUSE_BUTTON_5_UP },
        };
        for (std::size_t i = 0; i < MOUSE_BUTTON_COUNT; ++i)
        {
            if (flags & buttonFlags[i][0])
                SetMouseButton(i, true);
            if (flags & buttonFlags[i][1])
                SetMouseButton(i, false);
        }
    }

    void InputSystem::SetMouseButton(std::size_t button, bool held) noexcept
    {
        if (s_mouse.m_held[button] == held)
            return;

        s_mouse.m_held[button] = held;
        (held ? s_mouse.m_down : s_mouse.m_released)[button] = true;
    }

    void InputSystem::ApplyCursorClip() noexcept
    {
        if (!s_hasFocus || s_cursorMode == CursorLockMode::None)
        {
            ReleaseCursorClip();
            return;
        }

        // クライアント領域をスクリーン座標へ変換して OS の制限に渡す
        RECT client{};
        if (!GetClientRect(s_window, &client) || IsIconic(s_window) ||
            client.right <= client.left || client.bottom <= client.top)
        {
            ReleaseCursorClip();
            return;
        }

        // クライアント領域の中央にカーソルを閉じ込める
        if (s_cursorMode == CursorLockMode::Locked)
        {
            client.left += (client.right - client.left) / 2;
            client.top += (client.bottom - client.top) / 2;
            client.right = client.left + 1;
            client.bottom = client.top + 1;
        }

        POINT topLeft{ client.left, client.top };
        POINT bottomRight{ client.right, client.bottom };
        if (ClientToScreen(s_window, &topLeft) && ClientToScreen(s_window, &bottomRight))
        {
            const RECT bounds{ topLeft.x, topLeft.y, bottomRight.x, bottomRight.y };
            if (ClipCursor(&bounds))
            {
                s_cursorClipped = true;

                // Lockedではboundsが中央の1x1になっている
                if (s_cursorMode == CursorLockMode::Locked)
                    SetCursorPos(bounds.left, bounds.top);
                return;
            }
        }

        // 新しい範囲を適用できなければ、以前の位置に閉じ込めない
        ReleaseCursorClip();
    }

    void InputSystem::ReleaseCursorClip() noexcept
    {
        if (s_cursorClipped && ClipCursor(nullptr))
            s_cursorClipped = false;
    }

    void InputSystem::UpdateCursorVisibility() noexcept
    {
        // OS からの次の WM_SETCURSOR を待たず、カーソル直下のクライアント領域だけ更新する
        POINT cursor{};
        RECT client{};
        if (!GetCursorPos(&cursor) ||
            WindowFromPoint(cursor) != s_window ||
            !ScreenToClient(s_window, &cursor) ||
            !GetClientRect(s_window, &client) ||
            !PtInRect(&client, cursor))
            return;

        const auto classCursor = reinterpret_cast<HCURSOR>(GetClassLongPtrW(s_window, GCLP_HCURSOR));
        SetCursor(IsCursorHidden() ? nullptr : classCursor);
    }

    bool InputSystem::IsCursorHidden() noexcept
    {
        return s_hasFocus && (!s_cursorVisible || s_cursorMode == CursorLockMode::Locked);
    }

    void InputSystem::EndFrame() noexcept
    {
        // 押下状態を次のフレームへ持ち越し、遷移と差分だけを消す
        s_keyboard.m_down.reset();
        s_keyboard.m_released.reset();
        s_mouse.m_down.reset();
        s_mouse.m_released.reset();
        s_mouse.m_deltaPosition = {};
        s_mouse.m_deltaScroll = {};
    }

    const InputAPI& InputSystem::GetAPI() noexcept
    {
        static constexpr InputAPI api{
            &KeyHold, &KeyDown, &KeyRelease,
            &MouseHold, &MouseDown, &MouseRelease,
            &MousePosition, &DeltaPosition, &DeltaScroll,
            &SetCursorLockMode, &GetCursorLockMode, &SetCursorVisible, &GetCursorVisible,
        };
        return api;
    }

    bool InputSystem::KeyHold(Key key) noexcept
    {
        return s_keyboard.m_held[static_cast<std::size_t>(key)];
    }

    bool InputSystem::KeyDown(Key key) noexcept
    {
        return s_keyboard.m_down[static_cast<std::size_t>(key)];
    }

    bool InputSystem::KeyRelease(Key key) noexcept
    {
        return s_keyboard.m_released[static_cast<std::size_t>(key)];
    }

    bool InputSystem::MouseHold(MouseButton button) noexcept
    {
        const auto index = static_cast<std::size_t>(button);
        return index < MOUSE_BUTTON_COUNT && s_mouse.m_held[index];
    }

    bool InputSystem::MouseDown(MouseButton button) noexcept
    {
        const auto index = static_cast<std::size_t>(button);
        return index < MOUSE_BUTTON_COUNT && s_mouse.m_down[index];
    }

    bool InputSystem::MouseRelease(MouseButton button) noexcept
    {
        const auto index = static_cast<std::size_t>(button);
        return index < MOUSE_BUTTON_COUNT && s_mouse.m_released[index];
    }

    Vector2 InputSystem::MousePosition() noexcept
    {
        POINT cursor{};
        if (s_window == nullptr || !GetCursorPos(&cursor) || !ScreenToClient(s_window, &cursor))
            return {};

        return { static_cast<float>(cursor.x), static_cast<float>(cursor.y) };
    }

    Vector2 InputSystem::DeltaPosition() noexcept
    {
        return s_mouse.m_deltaPosition;
    }

    Vector2 InputSystem::DeltaScroll() noexcept
    {
        return s_mouse.m_deltaScroll;
    }

    void InputSystem::SetCursorLockMode(CursorLockMode mode) noexcept
    {
        if (mode > CursorLockMode::Locked || s_cursorMode == mode)
            return;

        // フォーカスを失っている間は設定値を記録し、復帰時に適用する
        s_cursorMode = mode;
        if (s_hasFocus)
        {
            ApplyCursorClip();
            UpdateCursorVisibility();
        }
    }

    CursorLockMode InputSystem::GetCursorLockMode() noexcept
    {
        return s_cursorMode;
    }

    void InputSystem::SetCursorVisible(bool visible) noexcept
    {
        s_cursorVisible = visible;
        if (s_hasFocus && s_cursorMode != CursorLockMode::Locked)
            UpdateCursorVisibility();
    }

    bool InputSystem::GetCursorVisible() noexcept
    {
        return s_cursorVisible;
    }
}
