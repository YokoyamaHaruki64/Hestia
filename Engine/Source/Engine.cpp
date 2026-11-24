/*=============================================================================

 File   : Engine.cpp
 Desc   : Engine の初期化、終了、通知・更新処理を実装する。

------------------------------------------------------------------------------

 Date   : 2026/10/06
 Author : Yokoyama Haruki

=============================================================================*/

#include "pch.h"
#include "Engine.h"

#include <filesystem>

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

        // Game の初期化と Facade の接続は、参照先の System が揃ってから行う
        if (!LoadGame())
        {
            Finalize();
            return false;
        }
        return true;
    }

    bool Engine::LoadGame()
    {
#if defined(HESTIA_TEST) && HESTIA_TEST
        const GameRuntimeAPI* runtimeAPI = GetGameRuntimeAPI();
#else
        // 作業ディレクトリから独立して、App と同じ場所の DLL を開く
        std::wstring executablePath(MAX_PATH, L'\0');
        DWORD pathLength = 0;
        while(true)
        {
            pathLength = GetModuleFileNameW(nullptr, executablePath.data(),
                static_cast<DWORD>(executablePath.size()));
            if (pathLength == 0)
            {
                Logger::Error("Game.dll: could not resolve the executable path");
                return false;
            }
            if (pathLength < executablePath.size())
                break;
            executablePath.resize(executablePath.size() * 2);
        }
        executablePath.resize(pathLength);
        const auto gamePath = std::filesystem::path(executablePath).parent_path() / L"Game.dll";
        m_gameModule = LoadLibraryW(gamePath.c_str());
        if (m_gameModule == nullptr)
        {
            const DWORD error = GetLastError();
            Logger::Error("Game.dll: load failed (Win32 " + std::to_string(error) + ")");
            return false;
        }

        // 入口とテーブルを確認してから、Runtime へ Engine 側の参照を渡す
        const auto getRuntimeAPI = reinterpret_cast<decltype(&GetGameRuntimeAPI)>(
            GetProcAddress(m_gameModule, "GetGameRuntimeAPI"));
        if (getRuntimeAPI == nullptr)
        {
            Logger::Error("Game.dll: GetGameRuntimeAPI export is missing");
            return false;
        }
        const GameRuntimeAPI* runtimeAPI = getRuntimeAPI();
#endif
        if (runtimeAPI == nullptr ||
            runtimeAPI->Create == nullptr ||
            runtimeAPI->Destroy == nullptr ||
            runtimeAPI->FixedUpdate == nullptr ||
            runtimeAPI->Update == nullptr ||
            runtimeAPI->LateUpdate == nullptr ||
            runtimeAPI->EndFrame == nullptr)
        {
            Logger::Error("Game.dll: invalid GameRuntimeAPI table");
            return false;
        }

        m_entityAPI = ECSSystem::CreateEntityAPI();
        m_gameEngineAPI = { &m_timeAPI, &m_entityAPI, &m_ecs.GetProcessAPI(), &m_logAPI, &m_assetAPI };
        m_gameRuntime = runtimeAPI->Create(&m_gameEngineAPI);
        if (m_gameRuntime == nullptr)
        {
            Logger::Error("Game.dll: GameRuntime creation failed");
            return false;
        }

        // Active 設定の通知先を含む Runtime の初期化が済んでから、更新用 API を保持する
        m_gameRuntimeAPI = runtimeAPI;
        Logger::Log("GameRuntime initialized");
        return true;
    }

    void Engine::UnloadGame()
    {
        // Game の終了処理からは引き続き Engine API を使えるようにする
        if (m_gameRuntime != nullptr)
        {
            m_gameRuntimeAPI->Destroy(m_gameRuntime);
            m_gameRuntime = nullptr;
            Logger::Log("GameRuntime finalized");
        }

        // DLL 内の関数テーブルへの参照をアンロード前に解除する
        m_gameRuntimeAPI = nullptr;
        m_gameEngineAPI = {};
        m_entityAPI = {};
        if (m_gameModule != nullptr)
        {
            FreeLibrary(m_gameModule);
            m_gameModule = nullptr;
        }
    }

    bool Engine::CreateDebugScene()
    {
        // 全立方体で同じインスタンス描画用Materialを共有
        auto* shader = AssetSystem::Get(AssetSystem::Load<MaterialShader>("Shaders/MatShaders/Unlit_Inst.matsh"));
        if (!shader || !shader->IsInstancing())
            return false;

        auto material = std::make_unique<Material>();
        material->SetShader(shader);
        const auto* debugMaterial = AssetSystem::Get(AssetSystem::Register<Material>("DebugCubeMaterial", material.release()));
        if (!debugMaterial)
            return false;

        // 斜め上から透視投影し、手前と奥で立方体の見かけの大きさに差を出す
        GraphicsSystem::SetView(Matrix4x4_SIMD::LookAtLH(
            { 440.0f, 275.0f, -820.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }));
        GraphicsSystem::SetProj(Matrix4x4_SIMD::PerspectiveFovLH(
            0.8f, 1600.0f / 900.0f, 10.0f, 1600.0f));

        // 100×25×40の格子に並べ、回転後の対角線もセル間隔より小さくする
        constexpr int columns = 100;
        constexpr int rows = 25;
        constexpr int layers = 40;
        constexpr int cubeCount = columns * rows * layers;
        constexpr float spacing = 10.0f;
        constexpr float cubeSize = 5.5f;
        const Vector4 colors[] = {
            { 1.0f, 0.25f, 0.30f, 1.0f }, { 0.25f, 0.85f, 0.40f, 1.0f },
            { 0.30f, 0.60f, 1.0f, 1.0f }, { 1.0f, 0.75f, 0.25f, 1.0f },
            { 0.75f, 0.40f, 1.0f, 1.0f }, { 0.20f, 0.85f, 0.85f, 1.0f },
        };
        const auto rotation = Quaternion::EulerToQuat({ 0.5f, 0.65f, 0.0f });
        for (int index = 0; index < cubeCount; ++index)
        {
            const auto entity = ECSSystem::CreateEntity();
            if (!entity.IsValid())
                return false;
            ECSSystem::AddProcess<&MeshRendererProcess::Run>(entity);
            auto* renderer = ECSSystem::GetComponent<MeshRenderer>(entity);
            if (!renderer)
                return false;

            // 格子の位置を求め、奥行きのある色分けを設定
            const int column = index % columns;
            const int row = index / columns % rows;
            const int layer = index / (columns * rows);
            renderer->m_mesh = AssetSystem::Get(AssetSystem::Load<Mesh>("Cube"));
            renderer->m_materials = { debugMaterial };
            renderer->m_instanceColor = colors[(column / 5 + row / 5 + layer / 5) % std::size(colors)];

            // 格子の中心を原点に揃えてTransformへ登録
            const float x = (column - (columns - 1) * 0.5f) * spacing;
            const float y = (row - (rows - 1) * 0.5f) * spacing;
            const float z = (layer - (layers - 1) * 0.5f) * spacing;
            HestiaGame::Transform transform(entity);
            if (!transform.SetLocalScale({ cubeSize, cubeSize, cubeSize }) ||
                !transform.SetLocalRotation(rotation) || !transform.SetLocalPosition({ x, y, z }))
                return false;
        }
        return true;
    }

    void Engine::Finalize()
    {
        // Facade の参照先を解放する前に Game を終了する
        UnloadGame();

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
            if (m_gameRuntime != nullptr)
                m_gameRuntimeAPI->FixedUpdate(m_gameRuntime);
        }
    }

    void Engine::FrameExecute(float deltaTime)
    {
        if (!m_graphicsInitialized || !m_timeInitialized)
            return;

        m_time.Update(deltaTime);

        auto start = std::chrono::high_resolution_clock::now();
        if (m_ecsInitialized)
        {
            // phase順にProcessを実行し、各phase内は登録順を維持する
            ECSSystem::Run(ECSProcessPhase::FrameBegin);
            ECSSystem::Run(ECSProcessPhase::Update);
            if (m_gameRuntime != nullptr)
                m_gameRuntimeAPI->Update(m_gameRuntime);
            ECSSystem::Run(ECSProcessPhase::LateUpdate);
            if (m_gameRuntime != nullptr)
                m_gameRuntimeAPI->LateUpdate(m_gameRuntime);
            ECSSystem::Run(ECSProcessPhase::PreRender);
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        Logger::Log("FrameExecute: DeltaTime= " + std::to_string(TimeSystem::DeltaTime()));
        OutputDebugString((L"FPS = " + std::to_wstring(static_cast<int>(1.0f / TimeSystem::DeltaTime())) + L"\n").c_str());
        OutputDebugString((L"Process Time = " + std::to_wstring(duration.count()) + L" us\n").c_str());
        // PreRenderで収集した描画提出を記録
        start = std::chrono::high_resolution_clock::now();
        m_graphics.Render();
        end = std::chrono::high_resolution_clock::now();
        duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start
        );
        OutputDebugString((L"Render Time = " + std::to_wstring(duration.count()) + L" us\n").c_str());
        // Script の終了処理が済んでから、破棄予約された Entity の実体を解放する
        if (m_gameRuntime != nullptr)
            m_gameRuntimeAPI->EndFrame(m_gameRuntime);
        if (m_ecsInitialized)
            ECSSystem::EndFrame();
    }
}
