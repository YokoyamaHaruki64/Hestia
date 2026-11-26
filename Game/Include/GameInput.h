/*=============================================================================

 File   : GameInput.h
 Desc   : Game Script に入力照会とカーソル設定を公開する。

------------------------------------------------------------------------------

 Date   : 2026/11/26
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _HESTIA_GAME_INPUT_H_
#define _HESTIA_GAME_INPUT_H_

#include "Engine/Include/API/InputAPI.h"

namespace HestiaGame
{
    namespace Detail { class GameRuntime; }
    using Hestia::CursorLockMode;
    using Hestia::Key;
    using Hestia::MouseButton;

    /// @brief GameRuntime と同じスレッドで使う入力 Facade。
    /// @note Engine API との接続前と切断後は入力を false またはゼロで返す。
    class Input
    {
    public:
        /// @brief 指定キーが現在押されている場合は true。
        static bool KeyHold(Key key) noexcept { return s_api && s_api->KeyHold(key); }
        /// @brief このフレーム中に指定キーを押した場合は true。
        static bool KeyDown(Key key) noexcept { return s_api && s_api->KeyDown(key); }
        /// @brief このフレーム中に指定キーを離した場合は true。
        static bool KeyRelease(Key key) noexcept { return s_api && s_api->KeyRelease(key); }

        /// @brief 指定ボタンが現在押されている場合は true。
        static bool MouseHold(MouseButton button) noexcept { return s_api && s_api->MouseHold(button); }
        /// @brief このフレーム中に指定ボタンを押した場合は true。
        static bool MouseDown(MouseButton button) noexcept { return s_api && s_api->MouseDown(button); }
        /// @brief このフレーム中に指定ボタンを離した場合は true。
        static bool MouseRelease(MouseButton button) noexcept { return s_api && s_api->MouseRelease(button); }

        /// @brief クライアント左上を原点とするカーソル座標をピクセルで返す。
        static Vector2 MousePosition() noexcept { return s_api ? s_api->MousePosition() : Vector2{}; }
        /// @brief フレーム内の相対移動量を Raw Input のカウントで返す。右・下が正。
        static Vector2 DeltaPosition() noexcept { return s_api ? s_api->DeltaPosition() : Vector2{}; }
        /// @brief フレーム内のスクロール量を返す。水平右・垂直奥が正で1目盛りが1.0。
        static Vector2 DeltaScroll() noexcept { return s_api ? s_api->DeltaScroll() : Vector2{}; }

        /// @brief カーソルの移動範囲を設定する。Locked 中は非表示。
        static void SetCursorLockMode(CursorLockMode mode) noexcept
        {
            if (s_api)
                s_api->SetCursorLockMode(mode);
        }
        /// @brief 指定されたモードを返す。接続前と切断後は None。
        static CursorLockMode GetCursorLockMode() noexcept
        {
            return s_api ? s_api->GetCursorLockMode() : CursorLockMode::None;
        }

        /// @brief カーソルを表示するか設定する。Locked 中の設定は解除後に反映する。
        static void SetCursorVisible(bool visible) noexcept
        {
            if (s_api)
                s_api->SetCursorVisible(visible);
        }
        /// @brief 表示の設定値を返す。初期値と未接続時は true。
        static bool GetCursorVisible() noexcept { return !s_api || s_api->GetCursorVisible(); }

    private:
        friend class Detail::GameRuntime;

        static void Bind(const Hestia::InputAPI* api) noexcept { s_api = api; }
        static void Unbind() noexcept { s_api = nullptr; }

        inline static const Hestia::InputAPI* s_api = nullptr;
    };
}

#endif // _HESTIA_GAME_INPUT_H_
