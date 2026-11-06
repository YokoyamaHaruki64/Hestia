/*=============================================================================

 File   : ApplicationAPI.cpp
 Desc   : ApplicationAPI の実装。

------------------------------------------------------------------------------

 Date   : 2026/10/06
 Author : Yokoyama Haruki

=============================================================================*/

#include "pch.h"

#include "ApplicationAPI.h"
#include "Application.h"


Hestia::ApplicationAPI Application::CreateApplicationAPI()
{
    Hestia::ApplicationAPI api;

    api.m_context = this;

    // 関数ポインタを設定

    api.m_setTargetFPS = [](void* context, double fps)
        {
            static_cast<Application*>(context)->SetTargetFPS(fps);
        };

    api.m_getTargetFPS = [](void* context)
        {
            return static_cast<Application*>(context)->GetTargetFPS();
        };

    api.m_setUnlimitedFrameRate = [](void* context, bool unlimited)
        {
            static_cast<Application*>(context)->SetUnlimitedFrameRate(unlimited);
        };

    api.m_isUnlimitedFrameRate = [](void* context)
        {
            return static_cast<Application*>(context)->IsUnlimitedFrameRate();
        };

    api.m_setFixedDeltaTime = [](void* context, double deltaTime)
        {
            static_cast<Application*>(context)->SetFixedDeltaTime(deltaTime);
        };

    api.m_getFixedDeltaTime = [](void* context)
        {
            return static_cast<Application*>(context)->GetFixedDeltaTime();
        };

    api.m_requestQuit = [](void* context)
        {
            static_cast<Application*>(context)->RequestQuit();
        };

    api.m_getWindowHandle = [](void* context)
        {
            return static_cast<Application*>(context)->GetWindowHandle();
        };

    return api;
}





