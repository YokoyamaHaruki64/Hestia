/*=============================================================================

 File   : Engine.cpp
 Desc   : Engine の初期化、終了、通知・更新処理を実装する。

------------------------------------------------------------------------------

 Date   : 2026/10/06
 Author : Yokoyama Haruki

=============================================================================*/

#include "pch.h"
#include "Engine.h"

namespace Hestia
{
    bool Engine::Initialize(const ApplicationAPI& applicationAPI)
    {
        m_applicationAPI = applicationAPI;
        return true;
    }

    void Engine::Finalize()
    {
        m_applicationAPI = ApplicationAPI{};
    }

    void Engine::ProcessMessage(HWND, UINT, WPARAM, LPARAM)
    {
    }

    void Engine::FixedUpdate(float)
    {
    }

    void Engine::FrameExecute(float)
    {
    }
}
