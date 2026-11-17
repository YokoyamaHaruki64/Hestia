/*=============================================================================

 File   : GameTime.h
 Desc   : GameRuntime に接続された時間取得・倍率設定を公開する。

------------------------------------------------------------------------------

 Date   : 2026/11/13
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _HESTIA_GAME_TIME_H_
#define _HESTIA_GAME_TIME_H_

#include <cassert>

#include "Engine/Include/API/TimeAPI.h"

namespace HestiaGame
{
    namespace Detail { class GameRuntime; }

    /// @brief Engine が所有する時間状態への Facade。
    /// @note GameRuntime の初期化から終了処理まで同じスレッドで使用する。
    class Time
    {
    public:
        /// @brief 通常更新のスケール適用済み経過時間（秒）を返す。
        static float DeltaTime() { return GetAPI().DeltaTime(); }

        /// @brief 通常更新の実経過時間（秒）を返す。
        static float UnscaledDeltaTime() { return GetAPI().UnscaledDeltaTime(); }

        /// @brief 通常更新で累積したスケール適用済み時間（秒）を返す。
        static float TotalTime() { return GetAPI().TotalTime(); }

        /// @brief 通常更新で累積した実時間（秒）を返す。
        static float UnscaledTotalTime() { return GetAPI().UnscaledTotalTime(); }

        /// @brief 固定更新のスケール適用済み刻み（秒）を返す。
        static float FixedDeltaTime() { return GetAPI().FixedDeltaTime(); }

        /// @brief 固定更新の実時間刻み（秒）を返す。
        static float UnscaledFixedDeltaTime() { return GetAPI().UnscaledFixedDeltaTime(); }

        /// @brief 現在の時間倍率を返す。
        static float TimeScale() { return GetAPI().TimeScale(); }

        /// @brief Engine に時間倍率の設定を依頼する。
        /// @param scale 有限の非負値。0 は停止。
        static void SetTimeScale(float scale) { GetAPI().SetTimeScale(scale); }

    private:
        friend class Detail::GameRuntime;

        static void Bind(const Hestia::TimeAPI* api) noexcept { s_api = api; }
        static void Unbind() noexcept { s_api = nullptr; }

        static const Hestia::TimeAPI& GetAPI()
        {
            assert(s_api != nullptr);
            return *s_api;
        }

        inline static const Hestia::TimeAPI* s_api = nullptr;
    };
}

#endif // _HESTIA_GAME_TIME_H_
