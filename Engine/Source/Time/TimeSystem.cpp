/*=============================================================================

 File   : TimeSystem.cpp
 Desc   : 時間値の更新と static 参照先の登録・解除を実装する。

------------------------------------------------------------------------------

 Date   : 2026/10/07
 Author : Yokoyama Haruki

=============================================================================*/

#include "pch.h"
#include "Time/TimeSystem.h"

namespace Hestia
{
    TimeSystem* TimeSystem::s_instance = nullptr;

    void TimeSystem::Initialize()
    {
        assert(s_instance == nullptr);
        m_data = TimeData{};
        s_instance = this;
    }

    void TimeSystem::Finalize()
    {
        assert(s_instance == this);
        s_instance = nullptr;
    }

    void TimeSystem::SetTimeScale(float scale)
    {
        if (!std::isfinite(scale) || scale < 0.0f)
            return;

        m_data.m_timeScale = scale;
    }

    void TimeSystem::Update(float deltaTime)
    {
        assert(std::isfinite(deltaTime) && deltaTime >= 0.0f);

        m_data.m_unscaledDeltaTime = deltaTime;
        m_data.m_deltaTime = deltaTime * m_data.m_timeScale;

        m_data.m_unscaledTotalTime += m_data.m_unscaledDeltaTime;
        m_data.m_totalTime += m_data.m_deltaTime;
    }

    void TimeSystem::UpdateFixed(float fixedDeltaTime)
    {
        assert(std::isfinite(fixedDeltaTime) && fixedDeltaTime >= 0.0f);

        m_data.m_unscaledFixedDeltaTime = fixedDeltaTime;
        m_data.m_fixedDeltaTime = fixedDeltaTime * m_data.m_timeScale;
    }
}
