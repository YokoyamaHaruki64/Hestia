/*=============================================================================

 File   : TimeAPI.h
 Desc   : Game.dll に公開する時間取得・倍率設定 API を定義する。

------------------------------------------------------------------------------

 Date   : 2026/10/07
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _TIME_API_H_
#define _TIME_API_H_

#include <cassert>

#include "Common/Include/DllAPI.h"
#include "TimeData.h"

namespace Hestia
{
    class TimeSystem;

    /// @brief TimeSystem とその TimeData を非所有で参照する DLL 境界 API。
    /// @note Initialize 後から System の破棄前まで、System と同じスレッドで利用する。
    class HESTIA_ENGINE_API TimeAPI
    {
    public:
        /// @brief 参照先を設定する。
        /// @param system この API の利用期間を通して生存する TimeSystem。
        /// @pre system は nullptr ではない。
        void Initialize(TimeSystem* system);

        /// @brief 非所有参照を解除する。
        /// @pre 参照先 TimeSystem が初期化済みである。
        void Finalize();

        /// @brief 通常更新のスケール適用済み経過時間（秒）。
        /// @return 通常更新のスケール適用済み経過時間（秒）。
        /// @pre Initialize 済みで参照先の System が生存している。
        inline float DeltaTime() const
        {
            assert(m_data != nullptr);
            return m_data->m_deltaTime;
        }

        /// @brief 通常更新の実経過時間（秒）。
        /// @return 通常更新の実経過時間（秒）。
        /// @pre Initialize 済みで参照先の System が生存している。
        inline float UnscaledDeltaTime() const
        {
            assert(m_data != nullptr);
            return m_data->m_unscaledDeltaTime;
        }

        /// @brief 通常更新で累積したスケール適用済み時間（秒）。
        /// @return 通常更新で累積したスケール適用済み時間（秒）。
        /// @pre Initialize 済みで参照先の System が生存している。
        inline float TotalTime() const
        {
            assert(m_data != nullptr);
            return m_data->m_totalTime;
        }

        /// @brief 通常更新で累積した実時間（秒）。
        /// @return 通常更新で累積した実時間（秒）。
        /// @pre Initialize 済みで参照先の System が生存している。
        inline float UnscaledTotalTime() const
        {
            assert(m_data != nullptr);
            return m_data->m_unscaledTotalTime;
        }

        /// @brief 固定更新のスケール適用済み刻み（秒）。
        /// @return 固定更新のスケール適用済み刻み（秒）。
        /// @pre Initialize 済みで参照先の System が生存している。
        inline float FixedDeltaTime() const
        {
            assert(m_data != nullptr);
            return m_data->m_fixedDeltaTime;
        }

        /// @brief 固定更新の実時間刻み（秒）。
        /// @return 固定更新の実時間刻み（秒）。
        /// @pre Initialize 済みで参照先の System が生存している。
        inline float UnscaledFixedDeltaTime() const
        {
            assert(m_data != nullptr);
            return m_data->m_unscaledFixedDeltaTime;
        }

        /// @brief 現在の時間倍率。
        /// @return 現在の時間倍率。
        /// @pre Initialize 済みで参照先の System が生存している。
        inline float TimeScale() const
        {
            assert(m_data != nullptr);
            return m_data->m_timeScale;
        }

        /// @brief System に時間倍率の設定を委譲する。
        /// @param scale 有限の非負値。0 は停止。負数・NaN・無限大は拒否する。
        /// @pre Initialize 済みで参照先の System が生存している。
        void SetTimeScale(float scale);

    private:
        TimeSystem* m_system = nullptr;
        const TimeData* m_data = nullptr;
    };
}

#endif // _TIME_API_H_
