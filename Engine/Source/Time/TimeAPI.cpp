/*=============================================================================

 File   : TimeAPI.cpp
 Desc   : 時間 API の参照接続と Engine 側への倍率設定を実装する。

------------------------------------------------------------------------------

 Date   : 2026/10/07
 Author : Yokoyama Haruki

=============================================================================*/

#include "pch.h"
#include "Time/TimeAPI.h"
#include "Time/TimeSystem.h"

namespace Hestia
{
    void TimeAPI::Initialize(TimeSystem* system)
    {
        assert(system != nullptr);

        m_system = system;
        m_data = &system->GetData();
    }

    void TimeAPI::Finalize()
    {
        assert(m_system != nullptr);
        m_system = nullptr;
        m_data = nullptr;
    }

    void TimeAPI::SetTimeScale(float scale)
    {
        assert(m_system != nullptr);
        m_system->SetTimeScale(scale);
    }
}
