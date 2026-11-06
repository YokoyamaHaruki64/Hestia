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
    Engine::~Engine()
    {
        Finalize();
    }

    bool Engine::Initialize(const ApplicationAPI& applicationAPI)
    {
        if (!m_log.Initialize())
            return false;

        m_logInitialized = true;
        m_applicationAPI = applicationAPI;

        m_time.Initialize();
        m_timeInitialized = true;

        if (!m_graphics.Initialize(m_applicationAPI.GetWindowHandle(), 1600, 900))
        {
            Finalize();
            return false;
        }
        m_graphicsInitialized = true;

        if (!m_assets.Initialize(&m_graphics))
        {
            Finalize();
            return false;
        }
        m_assetsInitialized = true;

        m_logAPI.Initialize(&m_log);
        m_timeAPI.Initialize(&m_time);
        m_timeAPIInitialized = true;
        m_assetAPI.Initialize(&m_assets);
        m_assetAPIInitialized = true;

        if (!m_ecs.Initialize())
        {
            Finalize();
            return false;
        }
        m_ecsInitialized = true;

        // デバッグ用平面を位置・UV・色の Stream で登録
        auto mesh = std::make_unique<Mesh>();
        mesh->m_positions = {
            { { -0.5f,  0.5f, 0.0f } },
            { {  0.5f,  0.5f, 0.0f } },
            { { -0.5f, -0.5f, 0.0f } },
            { {  0.5f, -0.5f, 0.0f } },
        };
        mesh->m_primaryTexCoords = {
            { { 0.0f, 0.0f } },
            { { 1.0f, 0.0f } },
            { { 0.0f, 1.0f } },
            { { 1.0f, 1.0f } },
        };
        mesh->m_colors = {
            { { 1.0f, 1.0f, 1.0f, 1.0f } },
            { { 1.0f, 1.0f, 1.0f, 1.0f } },
            { { 1.0f, 1.0f, 1.0f, 1.0f } },
            { { 1.0f, 1.0f, 1.0f, 1.0f } },
        };
        mesh->m_indices = { 0, 1, 2, 2, 1, 3 };

        m_debugMesh = m_assetAPI.Register<Mesh>("DebugPlane", mesh.release());
        if (!m_debugMesh.IsValid())
        {
            Finalize();
            return false;
        }

        // Texture は Engine 初期化時に一度だけ解決
        m_debugMaterial = Material::Default();
        const auto textureHandle = m_assetAPI.Load<Texture>("TextureTest");
        if (!textureHandle.IsValid())
        {
            Finalize();
            return false;
        }
        m_debugMaterial.textures.push_back(textureHandle);
        return true;
    }

    void Engine::Finalize()
    {
        if (m_ecsInitialized)
        {
            m_ecs.Finalize();
            m_ecsInitialized = false;
        }

        if (m_assetAPIInitialized)
        {
            m_assetAPI.Finalize();
            m_assetAPIInitialized = false;
        }

        if (m_assetsInitialized)
        {
            m_assets.Finalize();
            m_assetsInitialized = false;
        }

        if (m_graphicsInitialized)
        {
            m_graphics.Finalize();
            m_graphicsInitialized = false;
        }

        if (m_timeAPIInitialized)
        {
            m_timeAPI.Finalize();
            m_timeAPIInitialized = false;
        }

        if (m_timeInitialized)
        {
            m_time.Finalize();
            m_timeInitialized = false;
        }

        m_debugMaterial = Material{};
        m_debugMesh = {};
        m_applicationAPI = ApplicationAPI{};

        if (m_logInitialized)
        {
            // 他の System の終了処理が記録できるよう、ログは最後に終了する
            m_logAPI.Finalize();
            m_log.Finalize();
            m_logInitialized = false;
        }
    }

    void Engine::ProcessMessage(HWND, UINT, WPARAM, LPARAM)
    {
    }

    void Engine::FixedUpdate(float deltaTime)
    {
        if (m_timeInitialized)
        {
            m_time.UpdateFixed(deltaTime);
            if (m_ecsInitialized)
                ECSSystem::Run(ECSProcessPhase::FixedUpdate);
        }
    }

    void Engine::FrameExecute(float deltaTime)
    {
        if (!m_graphicsInitialized || !m_timeInitialized || !m_debugMesh.IsValid())
            return;

        m_time.Update(deltaTime);

        if (m_ecsInitialized)
        {
            // phase順にProcessを実行し、各phase内は登録順を維持する
            ECSSystem::Run(ECSProcessPhase::FrameBegin);
            ECSSystem::Run(ECSProcessPhase::Update);
            ECSSystem::Run(ECSProcessPhase::LateUpdate);
            ECSSystem::Run(ECSProcessPhase::PreRender);
        }
         Logger::Log("FrameExecute: DeltaTime= " + std::to_string(TimeSystem::DeltaTime()));

        Matrix4x4_SIMD worldMatrix = Matrix4x4_SIMD::Identity();
        worldMatrix.Scale(800.0f, 800.0f, 1.0f);
        worldMatrix.RotateZ(0.0f);
        worldMatrix.Translate(0.0f, 0.0f, 1.0f);

        for (int i = 0; i < 10000; ++i)
        {
            GraphicsSystem::Submit(RenderData{
                worldMatrix,
                m_debugMaterial,
                m_debugMesh,
                L"../Shaders/Build/VS_Sprite.cso",
                L"../Shaders/Build/PS_Sprite.cso"
            });
        }

        m_graphics.Render();

        // 描画が終わるまでEntity実体を維持し、破棄予約をFrame末尾で確定する
        if (m_ecsInitialized)
            ECSSystem::EndFrame();
    }
}
