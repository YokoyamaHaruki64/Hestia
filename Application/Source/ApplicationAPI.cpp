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

    return api;
}

void Hestia::ApplicationAPI::SetTargetFPS(double fps) const
{
    if (m_setTargetFPS)
    {
        m_setTargetFPS(m_context, fps);
    }
}
double Hestia::ApplicationAPI::GetTargetFPS() const
{
    if (m_getTargetFPS)
    {
        return m_getTargetFPS(m_context);
    }

    return 0.0;
}

void Hestia::ApplicationAPI::SetUnlimitedFrameRate(bool unlimited) const
{
    if (m_setUnlimitedFrameRate)
    {
        m_setUnlimitedFrameRate(m_context, unlimited);
    }
}
bool Hestia::ApplicationAPI::IsUnlimitedFrameRate() const
{
    if (m_isUnlimitedFrameRate)
    {
        return m_isUnlimitedFrameRate(m_context);
    }
    return false;
}

void Hestia::ApplicationAPI::SetFixedDeltaTime(double deltaTime) const
{
    if (m_setFixedDeltaTime)
    {
        m_setFixedDeltaTime(m_context, deltaTime);
    }
}
double Hestia::ApplicationAPI::GetFixedDeltaTime() const
{
    if (m_getFixedDeltaTime)
    {
        return m_getFixedDeltaTime(m_context);
    }
    return 0.0;
}

void Hestia::ApplicationAPI::RequestQuit() const
{
    if (m_requestQuit)
    {
        m_requestQuit(m_context);
    }
}
