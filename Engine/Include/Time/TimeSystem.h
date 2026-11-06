/*=============================================================================

 File   : TimeSystem.h
 Desc   : Engine 内部の時間状態と通常・固定更新を定義する。

------------------------------------------------------------------------------

 Date   : 2026/10/07
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _TIME_SYSTEM_H_
#define _TIME_SYSTEM_H_

#include <cassert>

#include "TimeData.h"

namespace Hestia
{
    /// @brief 同時に1個だけ存在する Engine 内部の時計。
    /// @note 同じスレッドで生成・更新・参照・破棄する。static Getter は生存期間内だけ呼び出す。
    class TimeSystem
    {
    public:
        TimeSystem() = default;

        TimeSystem(const TimeSystem&) = delete;
        TimeSystem& operator=(const TimeSystem&) = delete;
        TimeSystem(TimeSystem&&) = delete;
        TimeSystem& operator=(TimeSystem&&) = delete;

        /// @brief 時間値を初期化し、static Getter の参照先に登録する。
        /// @pre 他の TimeSystem が初期化されていない。
        void Initialize();

        /// @brief static Getter の参照先を解除する。
        /// @pre この時計を参照する TimeAPI の利用が終了している。
        void Finalize();

        /// @brief 通常更新のスケール適用済み経過時間（秒）。
        /// @return 通常更新のスケール適用済み経過時間（秒）。
        /// @pre TimeSystem が存在する。
        inline static float DeltaTime()
        {
            assert(s_instance != nullptr);
            return s_instance->m_data.m_deltaTime;
        }

        /// @brief 通常更新の実経過時間（秒）。
        /// @return 通常更新の実経過時間（秒）。
        /// @pre TimeSystem が存在する。
        inline static float UnscaledDeltaTime()
        {
            assert(s_instance != nullptr);
            return s_instance->m_data.m_unscaledDeltaTime;
        }

        /// @brief 通常更新で累積したスケール適用済み時間（秒）。
        /// @return 通常更新で累積したスケール適用済み時間（秒）。
        /// @pre TimeSystem が存在する。
        inline static float TotalTime()
        {
            assert(s_instance != nullptr);
            return s_instance->m_data.m_totalTime;
        }

        /// @brief 通常更新で累積した実時間（秒）。
        /// @return 通常更新で累積した実時間（秒）。
        /// @pre TimeSystem が存在する。
        inline static float UnscaledTotalTime()
        {
            assert(s_instance != nullptr);
            return s_instance->m_data.m_unscaledTotalTime;
        }

        /// @brief 固定更新のスケール適用済み刻み（秒）。
        /// @return 固定更新のスケール適用済み刻み（秒）。
        /// @pre TimeSystem が存在する。
        inline static float FixedDeltaTime()
        {
            assert(s_instance != nullptr);
            return s_instance->m_data.m_fixedDeltaTime;
        }

        /// @brief 固定更新の実時間刻み（秒）。
        /// @return 固定更新の実時間刻み（秒）。
        /// @pre TimeSystem が存在する。
        inline static float UnscaledFixedDeltaTime()
        {
            assert(s_instance != nullptr);
            return s_instance->m_data.m_unscaledFixedDeltaTime;
        }

        /// @brief 現在の時間倍率。
        /// @return 現在の時間倍率。
        /// @pre TimeSystem が存在する。
        inline static float TimeScale()
        {
            assert(s_instance != nullptr);
            return s_instance->m_data.m_timeScale;
        }

        /// @brief 時間倍率を設定する。更新済みの時間値には遡って適用しない。
        /// @param scale 有限の非負値。0 はスケール適用時間を停止する。
        /// @note 負数・NaN・無限大は拒否し、現在の倍率を維持する。
        void SetTimeScale(float scale);

        /// @brief 通常更新の経過時間を設定し、累積時間を加算する。
        /// @param deltaTime 有限の非負な実経過時間（秒）。
        void Update(float deltaTime);

        /// @brief 固定更新の刻みを設定する。累積時間には加算しない。
        /// @param fixedDeltaTime 有限の非負な実時間刻み（秒）。
        void UpdateFixed(float fixedDeltaTime);

        /// @brief API が参照する共有時間値を取得する。
        /// @return この時計の生存期間内で有効な読み取り専用の非所有参照。
        inline const TimeData& GetData() const
        {
            return m_data;
        }

    private:
        TimeData m_data;

        static TimeSystem* s_instance;
    };
}

#endif // _TIME_SYSTEM_H_
