/*=============================================================================

 File   : TimeData.h
 Desc   : TimeSystem が所有し、DLL 境界から参照する時間値を定義する。

------------------------------------------------------------------------------

 Date   : 2026/10/07
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _TIME_DATA_H_
#define _TIME_DATA_H_

namespace Hestia
{
    /// @brief TimeSystem が所有する時間値。TimeAPI はこのデータを非所有で参照する。
    struct TimeData
    {
        float m_deltaTime = 0.0f;
        float m_unscaledDeltaTime = 0.0f;

        float m_totalTime = 0.0f;
        float m_unscaledTotalTime = 0.0f;

        float m_fixedDeltaTime = 0.0f;
        float m_unscaledFixedDeltaTime = 0.0f;

        float m_timeScale = 1.0f;
    };
}

#endif // _TIME_DATA_H_

