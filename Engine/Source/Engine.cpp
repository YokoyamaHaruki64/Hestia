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
    static Mesh mesh2D;
    static DXMesh dxMesh2D;
    bool Engine::Initialize(const ApplicationAPI& applicationAPI)
    {
        if (!m_log.Initialize())
            return false;

        m_applicationAPI = applicationAPI;

        m_time.Initialize();
        if (!m_graphics.Initialize(m_applicationAPI.GetWindowHandle(), 1600, 900))
            return false;

        m_logAPI.Initialize(&m_log);
        m_timeAPI.Initialize(&m_time);

        // デバッグ用の平面メッシュを作成する
        mesh2D.vertices = {
            { Vector3(-0.5f,  0.5f, 0.0f), Vector2(0.0f, 0.0f), Vector3(0, 0, -1), Vector3(1, 0, 0), Color::White() },
            { Vector3(0.5f,  0.5f, 0.0f), Vector2(1.0f, 0.0f), Vector3(0, 0, -1), Vector3(1, 0, 0), Color::White() },
            { Vector3(-0.5f, -0.5f, 0.0f), Vector2(0.0f, 1.0f), Vector3(0, 0, -1), Vector3(1, 0, 0), Color::White() },
            { Vector3(0.5f, -0.5f, 0.0f), Vector2(1.0f, 1.0f), Vector3(0, 0, -1), Vector3(1, 0, 0), Color::White() },
        };

        mesh2D.indices = { 0, 1, 2, 2, 1, 3 };
        GraphicsSystem::CreateMesh(mesh2D, dxMesh2D);
        return true;
    }

    void Engine::Finalize()
    {
        m_timeAPI.Finalize();
        m_time.Finalize();
        m_graphics.Finalize();
        m_applicationAPI = ApplicationAPI{};

        // 他の System の終了処理が記録できるよう、ログは最後に終了する。
        m_logAPI.Finalize();
        m_log.Finalize();
    }

    void Engine::ProcessMessage(HWND, UINT, WPARAM, LPARAM)
    {
    }

    void Engine::FixedUpdate(float deltaTime)
    {
        m_time.UpdateFixed(deltaTime);
    }

    void Engine::FrameExecute(float deltaTime)
    {
        m_time.Update(deltaTime);

        Logger::Log("FrameExecute: DeltaTime= " + std::to_string(TimeSystem::DeltaTime()));

        Material mat = Material::Default();
        mat.textures.push_back(GraphicsSystem::LoadTexture(L"../Assets/Textures/texture_test_2048_rgba.png"));
        Matrix4x4_SIMD worldMatrix = Matrix4x4_SIMD::Identity();
        worldMatrix.Scale(800.0f, 800.0f, 1.0f);
        worldMatrix.RotateZ(0.0f);
        worldMatrix.Translate(0.0f, 0.0f, 1.0f);

        for (int i = 0; i < 10000; ++i)
        {
            GraphicsSystem::Submit(RenderData{
                worldMatrix,
                mat,
                dxMesh2D,
                L"../Shaders/Build/VS_Sprite.cso",
                L"../Shaders/Build/PS_Sprite.cso"
            });
        }

        m_graphics.Render();
    }
}
