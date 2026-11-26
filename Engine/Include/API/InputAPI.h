/*=============================================================================

 File   : InputAPI.h
 Desc   : Game 向けの入力型と入力 API テーブルを定義する。

------------------------------------------------------------------------------

 Date   : 2026/11/26
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _INPUT_API_H_
#define _INPUT_API_H_

#include <cstdint>

#include "Common/Include/Math/Vector2.h"

namespace Hestia
{
    /// @brief Windows の仮想キーコードに対応する論理キー。
    enum class Key : std::uint8_t
    {
        Backspace = 0x08,
        Tab = 0x09,
        Enter = 0x0D,
        Shift = 0x10,
        Ctrl = 0x11,
        Alt = 0x12,
        Pause = 0x13,
        CapsLock = 0x14,
        Escape = 0x1B,
        Space = 0x20,
        PageUp = 0x21,
        PageDown = 0x22,
        End = 0x23,
        Home = 0x24,
        Left = 0x25,
        Up = 0x26,
        Right = 0x27,
        Down = 0x28,
        PrintScreen = 0x2C,
        Insert = 0x2D,
        Delete = 0x2E,

        Digit0 = 0x30,
        Digit1 = 0x31,
        Digit2 = 0x32,
        Digit3 = 0x33,
        Digit4 = 0x34,
        Digit5 = 0x35,
        Digit6 = 0x36,
        Digit7 = 0x37,
        Digit8 = 0x38,
        Digit9 = 0x39,

        A = 0x41,
        B = 0x42,
        C = 0x43,
        D = 0x44,
        E = 0x45,
        F = 0x46,
        G = 0x47,
        H = 0x48,
        I = 0x49,
        J = 0x4A,
        K = 0x4B,
        L = 0x4C,
        M = 0x4D,
        N = 0x4E,
        O = 0x4F,
        P = 0x50,
        Q = 0x51,
        R = 0x52,
        S = 0x53,
        T = 0x54,
        U = 0x55,
        V = 0x56,
        W = 0x57,
        X = 0x58,
        Y = 0x59,
        Z = 0x5A,

        LeftWindows = 0x5B,
        RightWindows = 0x5C,
        ContextMenu = 0x5D,

        Numpad0 = 0x60,
        Numpad1 = 0x61,
        Numpad2 = 0x62,
        Numpad3 = 0x63,
        Numpad4 = 0x64,
        Numpad5 = 0x65,
        Numpad6 = 0x66,
        Numpad7 = 0x67,
        Numpad8 = 0x68,
        Numpad9 = 0x69,
        NumpadMultiply = 0x6A,
        NumpadAdd = 0x6B,
        NumpadSeparator = 0x6C,
        NumpadSubtract = 0x6D,
        NumpadDecimal = 0x6E,
        NumpadDivide = 0x6F,

        F1 = 0x70,
        F2 = 0x71,
        F3 = 0x72,
        F4 = 0x73,
        F5 = 0x74,
        F6 = 0x75,
        F7 = 0x76,
        F8 = 0x77,
        F9 = 0x78,
        F10 = 0x79,
        F11 = 0x7A,
        F12 = 0x7B,
        F13 = 0x7C,
        F14 = 0x7D,
        F15 = 0x7E,
        F16 = 0x7F,
        F17 = 0x80,
        F18 = 0x81,
        F19 = 0x82,
        F20 = 0x83,
        F21 = 0x84,
        F22 = 0x85,
        F23 = 0x86,
        F24 = 0x87,

        NumLock = 0x90,
        ScrollLock = 0x91,
        LeftShift = 0xA0,
        RightShift = 0xA1,
        LeftCtrl = 0xA2,
        RightCtrl = 0xA3,
        LeftAlt = 0xA4,
        RightAlt = 0xA5,

        Oem1 = 0xBA,
        OemPlus = 0xBB,
        OemComma = 0xBC,
        OemMinus = 0xBD,
        OemPeriod = 0xBE,
        Oem2 = 0xBF,
        Oem3 = 0xC0,
        Oem4 = 0xDB,
        Oem5 = 0xDC,
        Oem6 = 0xDD,
        Oem7 = 0xDE,
        Oem8 = 0xDF,
        Oem102 = 0xE2,
    };

    /// @brief マウスの5つの入力ボタン。
    enum class MouseButton : std::uint8_t
    {
        Left,
        Right,
        Middle,
        X1,
        X2,

        MouseButtonCount,
    };

    /// @brief フォーカス中のカーソルの移動範囲。
    enum class CursorLockMode : std::uint8_t
    {
        None,
        Confined,
        Locked,
    };

    /// @brief GameRuntime に渡す入力照会・カーソル設定テーブル。
    struct InputAPI
    {
        bool (*KeyHold)(Key key) noexcept = nullptr;
        bool (*KeyDown)(Key key) noexcept = nullptr;
        bool (*KeyRelease)(Key key) noexcept = nullptr;
        bool (*MouseHold)(MouseButton button) noexcept = nullptr;
        bool (*MouseDown)(MouseButton button) noexcept = nullptr;
        bool (*MouseRelease)(MouseButton button) noexcept = nullptr;
        Vector2 (*MousePosition)() noexcept = nullptr;
        Vector2 (*DeltaPosition)() noexcept = nullptr;
        Vector2 (*DeltaScroll)() noexcept = nullptr;
        void (*SetCursorLockMode)(CursorLockMode mode) noexcept = nullptr;
        CursorLockMode (*GetCursorLockMode)() noexcept = nullptr;
        void (*SetCursorVisible)(bool visible) noexcept = nullptr;
        bool (*GetCursorVisible)() noexcept = nullptr;
    };
}

#endif // _INPUT_API_H_
