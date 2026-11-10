/*=============================================================================

 File   : Engine.cpp
 Desc   : Engine の初期化、終了、通知・更新処理を実装する。

------------------------------------------------------------------------------

 Date   : 2026/10/06
 Author : Yokoyama Haruki

=============================================================================*/

#include "pch.h"
#include "Engine.h"

#include "Asset/Material.h"
#include "Game/Transform.h"
#include "Graphics/MeshRenderer.h"

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

        // 通常のECS描画経路を使うデバッグシーンを作成
        if (!CreateDebugScene())
        {
            Finalize();
            return false;
        }
        return true;
    }

    bool Engine::CreateDebugScene()
    {
        // 全インスタンスが共有する、8頂点と36インデックスの立方体を登録
        auto mesh = std::make_unique<Mesh>();
        mesh->m_positions = {
            { { -0.5f, -0.5f, -0.5f } }, { {  0.5f, -0.5f, -0.5f } },
            { {  0.5f,  0.5f, -0.5f } }, { { -0.5f,  0.5f, -0.5f } },
            { { -0.5f, -0.5f,  0.5f } }, { {  0.5f, -0.5f,  0.5f } },
            { {  0.5f,  0.5f,  0.5f } }, { { -0.5f,  0.5f,  0.5f } },
        };
        mesh->m_indices = {
            0, 2, 1, 0, 3, 2,   4, 5, 6, 4, 6, 7,
            0, 4, 7, 0, 7, 3,   1, 2, 6, 1, 6, 5,
            0, 1, 5, 0, 5, 4,   3, 7, 6, 3, 6, 2,
        };

        const auto* debugMesh = m_assetAPI.Get(m_assetAPI.Register<Mesh>("DebugCube", mesh.release()));
        if (!debugMesh)
            return false;

        // 全立方体で同じインスタンス描画用Materialを共有
        auto* shader = AssetSystem::Get(AssetSystem::Load<MaterialShader>("Unlit_Inst"));
        if (!shader || !shader->IsInstancing())
            return false;

        auto material = std::make_unique<Material>();
        material->SetShader(shader);
        const auto* debugMaterial = AssetSystem::Get(AssetSystem::Register<Material>("DebugCubeMaterial", material.release()));
        if (!debugMaterial)
            return false;

        // 位置と色だけを変え、同じDrawから各インスタンスを識別できるようにする
        const Vector4 colors[] = {
            { 1.0f, 0.25f, 0.30f, 1.0f }, { 0.25f, 0.85f, 0.40f, 1.0f },
            { 0.30f, 0.60f, 1.0f, 1.0f }, { 1.0f, 0.75f, 0.25f, 1.0f },
            { 0.75f, 0.40f, 1.0f, 1.0f }, { 0.20f, 0.85f, 0.85f, 1.0f },
        };
        const auto rotation = Quaternion::EulerToQuat({ 0.5f, 0.65f, 0.0f });
        for (int index = 0; index < static_cast<int>(std::size(colors)); ++index)
        {
            const auto entity = ECSSystem::CreateEntity();
            if (!entity.IsValid())
                return false;
            ECSSystem::AddProcess<&MeshRendererProcess::Run>(entity);
            auto* renderer = ECSSystem::GetComponent<MeshRenderer>(entity);
            if (!renderer)
                return false;

            // 共有Assetと個体ごとのデータを登録
            renderer->m_mesh = debugMesh;
            renderer->m_materials = { debugMaterial };
            renderer->m_instanceColor = colors[index];
            HestiaGame::Transform transform(entity);
            const float x = static_cast<float>(index % 3 - 1) * 120.0f;
            const float y = index < 3 ? 55.0f : -55.0f;
            if (!transform.SetLocalScale({ 56.0f, 56.0f, 56.0f }) ||
                !transform.SetLocalRotation(rotation) || !transform.SetLocalPosition({ x, y, 50.0f }))
                return false;
        }
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

        // Graphicsが保持する描画提出の借用参照をAssetの解放前に解除
        if (m_graphicsInitialized)
        {
            m_graphics.Finalize();
            m_graphicsInitialized = false;
        }

        if (m_assetsInitialized)
        {
            m_assets.Finalize();
            m_assetsInitialized = false;
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
        if (!m_graphicsInitialized || !m_timeInitialized)
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

        // PreRenderで収集した描画提出を記録
        m_graphics.Render();

        // 描画が終わるまでEntity実体を維持し、破棄予約をFrame末尾で確定する
        if (m_ecsInitialized)
            ECSSystem::EndFrame();
    }
}
