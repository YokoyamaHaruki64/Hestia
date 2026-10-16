/*=============================================================================

 File   : AssetGraphicsTests.cpp
 Desc   : Asset と Graphics の DX12 統合経路を検証する。

------------------------------------------------------------------------------

 Date   : 2026/10/15
 Author : Yokoyama Haruki

=============================================================================*/

#include "Common/Include/WindowsHeaders.h"

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <d3d12sdklayers.h>
#include <objbase.h>
#include <string>
#include <vector>

#include "Asset/AssetAPI.h"
#include "Asset/AssetSystem.h"
#include "Graphics/DX12Backend.h"
#include "Graphics/GraphicsSystem.h"
#include "Time/TimeSystem.h"

namespace
{
    std::filesystem::path FindRepositoryRoot()
    {
        std::array<wchar_t, 32768> modulePath{};
        const DWORD pathLength = GetModuleFileNameW(
            nullptr, modulePath.data(), static_cast<DWORD>(modulePath.size()));
        if (pathLength == 0 || pathLength == modulePath.size())
            return {};

        std::filesystem::path candidate =
            std::filesystem::path(modulePath.data()).parent_path();
        while (!candidate.empty())
        {
            if (std::filesystem::exists(candidate / L"Engine" / L"Engine.vcxproj") &&
                std::filesystem::exists(candidate / L"Assets" / L"Textures" / L"texture_test_2048_rgba.png"))
                return candidate;

            const auto parent = candidate.parent_path();
            if (parent == candidate)
                break;
            candidate = parent;
        }

        return {};
    }

    Hestia::Mesh MakePlane()
    {
        Hestia::Mesh mesh;
        mesh.m_positions = {
            { { -0.5f,  0.5f, 0.0f } },
            { {  0.5f,  0.5f, 0.0f } },
            { { -0.5f, -0.5f, 0.0f } },
            { {  0.5f, -0.5f, 0.0f } },
        };
        mesh.m_indices = { 0, 1, 2, 2, 1, 3 };
        return mesh;
    }

    LRESULT CALLBACK HiddenWindowProcedure(
        HWND window, UINT message, WPARAM wParam, LPARAM lParam)
    {
        return DefWindowProcW(window, message, wParam, lParam);
    }

    class BackendTestEnvironment
    {
    public:
        BackendTestEnvironment()
            : m_originalWorkingDirectory(std::filesystem::current_path()),
              m_instance(GetModuleHandleW(nullptr))
        {
            const auto repositoryRoot = FindRepositoryRoot();
            if (repositoryRoot.empty())
                return;

            std::filesystem::current_path(repositoryRoot / L"Engine");

            WNDCLASSEXW windowClass{};
            windowClass.cbSize = sizeof(windowClass);
            windowClass.lpfnWndProc = HiddenWindowProcedure;
            windowClass.hInstance = m_instance;
            windowClass.lpszClassName = L"HestiaDX12BackendTestWindow";
            m_registeredClass = RegisterClassExW(&windowClass) != 0;
            if (!m_registeredClass && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
                return;

            m_window = CreateWindowExW(
                0,
                windowClass.lpszClassName,
                L"",
                WS_OVERLAPPEDWINDOW,
                0,
                0,
                1600,
                900,
                nullptr,
                nullptr,
                m_instance,
                nullptr);
        }

        ~BackendTestEnvironment()
        {
            if (m_window != nullptr)
                DestroyWindow(m_window);
            if (m_registeredClass)
                UnregisterClassW(L"HestiaDX12BackendTestWindow", m_instance);
            std::filesystem::current_path(m_originalWorkingDirectory);
        }

        bool IsValid() const
        {
            return m_window != nullptr;
        }

        HWND GetWindow() const
        {
            return m_window;
        }

    private:
        std::filesystem::path m_originalWorkingDirectory;
        HINSTANCE m_instance = nullptr;
        HWND m_window = nullptr;
        bool m_registeredClass = false;
    };

    class TimeSystemGuard
    {
    public:
        explicit TimeSystemGuard(Hestia::TimeSystem& time)
            : m_time(time)
        {
            m_time.Initialize();
            m_active = true;
        }

        ~TimeSystemGuard()
        {
            Finalize();
        }

        void Finalize()
        {
            if (!m_active)
                return;

            m_time.Finalize();
            m_active = false;
        }

    private:
        Hestia::TimeSystem& m_time;
        bool m_active = false;
    };

    class AssetGraphicsFixture : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            const HRESULT comResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
            m_comInitialized = SUCCEEDED(comResult);
            ASSERT_TRUE(m_comInitialized || comResult == RPC_E_CHANGED_MODE);

            m_originalWorkingDirectory = std::filesystem::current_path();
            const auto repositoryRoot = FindRepositoryRoot();
            ASSERT_FALSE(repositoryRoot.empty());
            std::filesystem::current_path(repositoryRoot / L"Engine");
            m_time.Initialize();
            m_timeInitialized = true;

            m_instance = GetModuleHandleW(nullptr);
            WNDCLASSEXW windowClass{};
            windowClass.cbSize = sizeof(windowClass);
            windowClass.lpfnWndProc = HiddenWindowProcedure;
            windowClass.hInstance = m_instance;
            windowClass.lpszClassName = L"HestiaAssetGraphicsTestWindow";
            if (RegisterClassExW(&windowClass) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
                FAIL() << "Failed to register hidden test window class";

            m_window = CreateWindowExW(
                0,
                windowClass.lpszClassName,
                L"",
                WS_OVERLAPPEDWINDOW,
                0,
                0,
                1600,
                900,
                nullptr,
                nullptr,
                m_instance,
                nullptr);
            ASSERT_NE(m_window, nullptr);

            ASSERT_TRUE(m_graphics.Initialize(m_window, 1600, 900));
            m_graphicsInitialized = true;

            ASSERT_TRUE(m_assets.Initialize(&m_graphics));
            m_assetsInitialized = true;
            m_assetAPI.Initialize(&m_assets);
            m_assetAPIInitialized = true;
        }

        void TearDown() override
        {
            if (m_assetAPIInitialized)
                m_assetAPI.Finalize();
            if (m_assetsInitialized)
                m_assets.Finalize();
            if (m_graphicsInitialized)
                m_graphics.Finalize();
            if (m_timeInitialized)
                m_time.Finalize();
            if (m_window != nullptr)
                DestroyWindow(m_window);
            if (m_instance != nullptr)
                UnregisterClassW(L"HestiaAssetGraphicsTestWindow", m_instance);
            if (!m_originalWorkingDirectory.empty())
                std::filesystem::current_path(m_originalWorkingDirectory);
            if (m_comInitialized)
                CoUninitialize();
        }

        Hestia::GraphicsSystem m_graphics;
        Hestia::AssetSystem m_assets;
        Hestia::AssetAPI m_assetAPI;
        Hestia::TimeSystem m_time;
        std::filesystem::path m_originalWorkingDirectory;
        HINSTANCE m_instance = nullptr;
        HWND m_window = nullptr;
        bool m_graphicsInitialized = false;
        bool m_assetsInitialized = false;
        bool m_assetAPIInitialized = false;
        bool m_timeInitialized = false;
        bool m_comInitialized = false;
    };

    TEST_F(AssetGraphicsFixture, LoadsAndDrawsMeshWithTextureAndFallbackBindings)
    {
        RecordProperty("target", "AssetAPI texture load, mesh registration, GraphicsSystem submit and fallback draws");

        const auto textureHandle = m_assetAPI.Load<Hestia::Texture>("TextureTest");
        ASSERT_TRUE(textureHandle.IsValid());
        ASSERT_NE(m_assetAPI.Get(textureHandle), nullptr);

        auto plane = std::make_unique<Hestia::Mesh>(MakePlane());
        const auto meshHandle = m_assetAPI.Register<Hestia::Mesh>("GraphicsTestPlane", plane.release());
        ASSERT_TRUE(meshHandle.IsValid());

        Hestia::Material texturedMaterial = Hestia::Material::Default();
        texturedMaterial.textures.push_back(textureHandle);

        Matrix4x4_SIMD planeTransform = Matrix4x4_SIMD::Identity();
        planeTransform.Scale(64.0f, 64.0f, 1.0f);
        planeTransform.Translate(0.0f, 0.0f, 1.0f);

        const Hestia::RenderData texturedDraw{
            planeTransform,
            texturedMaterial,
            meshHandle,
            L"../Shaders/Build/VS_Sprite.cso",
            L"../Shaders/Build/PS_Sprite.cso"
        };
        ASSERT_TRUE(Hestia::GraphicsSystem::Submit(texturedDraw));
        m_graphics.Render();

        Hestia::Material untexturedMaterial = Hestia::Material::Default();
        const Hestia::RenderData untexturedDraw{
            planeTransform,
            untexturedMaterial,
            meshHandle,
            L"../Shaders/Build/VS_Sprite.cso",
            L"../Shaders/Build/PS_Sprite.cso"
        };
        ASSERT_TRUE(Hestia::GraphicsSystem::Submit(untexturedDraw));
        m_graphics.Render();

        Hestia::Material missingTextureMaterial = Hestia::Material::Default();
        Hestia::AssetHandle<Hestia::Texture> missingTexture;
        missingTexture.m_index = 0xfffffffeu;
        missingTexture.m_generation = 1;
        missingTextureMaterial.textures.push_back(missingTexture);
        const Hestia::RenderData missingTextureDraw{
            planeTransform,
            missingTextureMaterial,
            meshHandle,
            L"../Shaders/Build/VS_Sprite.cso",
            L"../Shaders/Build/PS_Sprite.cso"
        };
        ASSERT_TRUE(Hestia::GraphicsSystem::Submit(missingTextureDraw));
        m_graphics.Render();

        Hestia::RenderData invalidMeshDraw{};
        EXPECT_FALSE(Hestia::GraphicsSystem::Submit(invalidMeshDraw));
    }

    TEST(DX12BackendIntegration, DrawsFallbackBindingsGrowsStreamsAndReportsNoDeviceErrors)
    {
        RecordProperty("target", "DX12Backend upload, fallback texture binding, stream growth and debug-layer diagnostics");

        BackendTestEnvironment environment;
        ASSERT_TRUE(environment.IsValid());

        Hestia::TimeSystem time;
        TimeSystemGuard timeGuard(time);

        auto backend = std::make_unique<Hestia::DX12Backend>();
        ASSERT_TRUE(backend->Initialize(environment.GetWindow(), 1600, 900));

        ComPtr<ID3D12Device> device;
        device = backend->GetDevice();
        ComPtr<ID3D12InfoQueue> infoQueue;
        const bool hasInfoQueue = device && SUCCEEDED(device.As(&infoQueue));
        RecordProperty("debug_layer", hasInfoQueue ? "available" : "unavailable");

        const Hestia::AssetHandle<Hestia::Mesh> smallMeshHandle{ 0, 1 };
        Hestia::Mesh smallMesh;
        smallMesh.m_handle = smallMeshHandle;
        smallMesh.m_positions = {
            { { -0.5f,  0.5f, 0.0f } },
            { {  0.5f,  0.5f, 0.0f } },
            { { -0.5f, -0.5f, 0.0f } },
            { {  0.5f, -0.5f, 0.0f } },
        };
        smallMesh.m_indices = { 0, 1, 2, 2, 1, 3 };
        Hestia::Mesh invalidMesh;
        invalidMesh.m_handle = smallMeshHandle;
        EXPECT_FALSE(backend->UploadMesh(smallMeshHandle, invalidMesh));
        ASSERT_TRUE(backend->UploadMesh(smallMeshHandle, smallMesh));

        const Hestia::AssetHandle<Hestia::Texture> textureHandle{ 0, 1 };
        const Hestia::TextureDesc textureDesc{ 1, 1, 1, Hestia::TextureFormat::RGBA8Unorm };
        const std::array<uint8_t, 4> pixel = { 255, 255, 255, 255 };
        ASSERT_TRUE(backend->UploadTexture(textureHandle, textureDesc, pixel.data(), pixel.size(), pixel.size()));
        EXPECT_FALSE(backend->UploadTexture({}, textureDesc, pixel.data(), pixel.size(), pixel.size()));

        const auto draw = [&](const Hestia::Material& material, Hestia::AssetHandle<Hestia::Mesh> meshHandle)
        {
            Matrix4x4_SIMD worldMatrix = Matrix4x4_SIMD::Identity();
            worldMatrix.Scale(64.0f, 64.0f, 1.0f);
            worldMatrix.Translate(0.0f, 0.0f, 1.0f);

            std::vector<Hestia::RenderData> renderData = {
                {
                    worldMatrix,
                    material,
                    meshHandle,
                    L"../Shaders/Build/VS_Sprite.cso",
                    L"../Shaders/Build/PS_Sprite.cso"
                }
            };

            if (!backend->BeginDraw(renderData, Matrix4x4::Identity(), Matrix4x4::Identity()))
                return false;

            backend->Draw();
            backend->EndDraw();
            return true;
        };

        Hestia::Material texturedMaterial = Hestia::Material::Default();
        texturedMaterial.textures.push_back(textureHandle);
        ASSERT_TRUE(draw(texturedMaterial, smallMeshHandle));

        Hestia::Material unsetTextureMaterial = Hestia::Material::Default();
        ASSERT_TRUE(draw(unsetTextureMaterial, smallMeshHandle));

        Hestia::Material invalidTextureMaterial = Hestia::Material::Default();
        invalidTextureMaterial.textures.push_back({});
        ASSERT_TRUE(draw(invalidTextureMaterial, smallMeshHandle));

        auto staleTextureHandle = textureHandle;
        ++staleTextureHandle.m_generation;
        Hestia::Material staleTextureMaterial = Hestia::Material::Default();
        staleTextureMaterial.textures.push_back(staleTextureHandle);
        ASSERT_TRUE(draw(staleTextureMaterial, smallMeshHandle));

        auto missingTextureHandle = textureHandle;
        missingTextureHandle.m_index = 1;
        Hestia::Material missingTextureMaterial = Hestia::Material::Default();
        missingTextureMaterial.textures.push_back(missingTextureHandle);
        ASSERT_TRUE(draw(missingTextureMaterial, smallMeshHandle));

        Hestia::AssetHandle<Hestia::Mesh> largeMeshHandle{ 1, 1 };
        Hestia::Mesh largeMesh;
        largeMesh.m_handle = largeMeshHandle;
        largeMesh.m_positions.resize(256);
        largeMesh.m_indices = { 0, 1, 2 };
        ASSERT_TRUE(backend->UploadMesh(largeMeshHandle, largeMesh));
        ASSERT_TRUE(draw(unsetTextureMaterial, largeMeshHandle));

        backend->Finalize();
        backend.reset();
        timeGuard.Finalize();

        if (hasInfoQueue)
        {
            std::vector<std::string> errors;
            const UINT64 messageCount = infoQueue->GetNumStoredMessagesAllowedByRetrievalFilter();
            for (UINT64 index = 0; index < messageCount; ++index)
            {
                SIZE_T messageSize = 0;
                if (FAILED(infoQueue->GetMessage(index, nullptr, &messageSize)))
                    continue;

                std::vector<uint8_t> storage(messageSize);
                auto* message = reinterpret_cast<D3D12_MESSAGE*>(storage.data());
                if (FAILED(infoQueue->GetMessage(index, message, &messageSize)))
                    continue;
                if (message->Severity == D3D12_MESSAGE_SEVERITY_ERROR ||
                    message->Severity == D3D12_MESSAGE_SEVERITY_CORRUPTION)
                    errors.emplace_back(message->pDescription);
            }

            EXPECT_TRUE(errors.empty()) << (errors.empty() ? std::string{} : errors.front());
        }

    }
}
