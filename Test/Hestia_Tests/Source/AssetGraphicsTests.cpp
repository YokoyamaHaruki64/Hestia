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
#include "Asset/Material.h"
#include "Asset/MaterialShaderParser.h"
#include "Graphics/DX12Backend.h"
#include "Graphics/DXMaterialShaderCompiler.h"
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

        Hestia::LegacyMaterial texturedMaterial = Hestia::LegacyMaterial::Default();
        texturedMaterial.textures.push_back(textureHandle);

        Matrix4x4_SIMD planeTransform = Matrix4x4_SIMD::Identity();
        planeTransform.Scale(64.0f, 64.0f, 1.0f);
        planeTransform.Translate(0.0f, 0.0f, 1.0f);

        const Hestia::RenderData texturedDraw{
            planeTransform,
            texturedMaterial,
            meshHandle,
            L"../Build/Shader/VS_Sprite.cso",
            L"../Build/Shader/PS_Sprite.cso"
        };
        ASSERT_TRUE(Hestia::GraphicsSystem::Submit(texturedDraw));
        m_graphics.Render();

        Hestia::LegacyMaterial untexturedMaterial = Hestia::LegacyMaterial::Default();
        const Hestia::RenderData untexturedDraw{
            planeTransform,
            untexturedMaterial,
            meshHandle,
            L"../Build/Shader/VS_Sprite.cso",
            L"../Build/Shader/PS_Sprite.cso"
        };
        ASSERT_TRUE(Hestia::GraphicsSystem::Submit(untexturedDraw));
        m_graphics.Render();

        Hestia::LegacyMaterial missingTextureMaterial = Hestia::LegacyMaterial::Default();
        Hestia::AssetHandle<Hestia::Texture> missingTexture;
        missingTexture.m_index = 0xfffffffeu;
        missingTexture.m_generation = 1;
        missingTextureMaterial.textures.push_back(missingTexture);
        const Hestia::RenderData missingTextureDraw{
            planeTransform,
            missingTextureMaterial,
            meshHandle,
            L"../Build/Shader/VS_Sprite.cso",
            L"../Build/Shader/PS_Sprite.cso"
        };
        ASSERT_TRUE(Hestia::GraphicsSystem::Submit(missingTextureDraw));
        m_graphics.Render();

        Hestia::RenderData invalidMeshDraw{};
        EXPECT_FALSE(Hestia::GraphicsSystem::Submit(invalidMeshDraw));
    }

    TEST(DX12BackendIntegration, DrawsForwardSubMeshWithDefaultNormalTangentStream)
    {
        BackendTestEnvironment environment;
        ASSERT_TRUE(environment.IsValid());
        Hestia::TimeSystem time;
        TimeSystemGuard timeGuard(time);
        auto backend = std::make_unique<Hestia::DX12Backend>();
        ASSERT_TRUE(backend->Initialize(environment.GetWindow(), 1600, 900));

        // NORMAL だけを要求する Pass をコンパイルし、Material を通常の Asset として登録
        Hestia::GraphicsSystem graphics;
        Hestia::AssetSystem assets;
        ASSERT_TRUE(assets.Initialize(&graphics));
        Hestia::ParsedMaterialShader parsed;
        std::string error;
        ASSERT_TRUE(Hestia::ParseMaterialShader(R"(
MaterialShader "Test/ForwardSubMesh"
{
    RenderType = Forward;
    Cull = None;
    Properties
    {
        _Tint ("Tint", Color) = (1, 1, 1, 1)
        _BaseMap ("Base Map", 2D) = "white"
    }
    Pass "Forward"
    {
        HLSL_BEGIN
        #pragma vertex VSMain
        #pragma pixel PSMain
        SamplerState TestSampler : register(s0);
        float4 VSMain(float3 position : POSITION, float3 normal : NORMAL) : SV_Position
        {
            // 既定の normal.z = -1 なら可視。0 の場合は遠クリップ面の外になる
            return float4(position.xy, normal.z + 1.5f, 1);
        }
        float4 PSMain() : SV_Target0
        {
            return _Tint * _BaseMap.SampleLevel(TestSampler, float2(0, 0), 0);
        }
        HLSL_END
    }
}
)", parsed, error)) << error;
        Hestia::DXMaterialShaderCompiler::Result compiled;
        ASSERT_TRUE(Hestia::DXMaterialShaderCompiler::Compile(
            std::filesystem::absolute("ForwardSubMesh.matsh"), parsed, compiled, error)) << error;
        auto shader = Hestia::MaterialShader::Create(std::move(parsed.m_name),
            std::move(compiled.m_properties), compiled.m_constantBufferSize, std::move(compiled.m_passes));
        ASSERT_NE(shader, nullptr);
        const auto shaderHandle = Hestia::AssetSystem::Register("TestForwardShader", shader.release());
        ASSERT_TRUE(shaderHandle.IsValid());
        auto material = std::make_unique<Hestia::Material>();
        material->SetShader(Hestia::AssetSystem::Get(shaderHandle));
        const auto materialHandle = Hestia::AssetSystem::Register("TestForwardMaterial", material.release());
        ASSERT_TRUE(materialHandle.IsValid());

        // NormalTangent は作らず、Slot 1 の SubMesh だけに Material を提出
        auto mesh = MakePlane();
        mesh.m_handle = { 0, 1 };
        mesh.m_indices = { 0, 0, 0, 2, 1, 3 }; // Slot 0 の範囲を描いてもピクセルは出ない
        mesh.m_subMeshes = { { 0, 3, 0 }, { 3, 3, 1 } };
        ASSERT_TRUE(backend->UploadMesh(mesh.m_handle, mesh));
        std::vector<Hestia::RenderData> draws = {
            { .mesh = mesh.m_handle, .materials = { {}, materialHandle } }
        };

        // GPU の統計を読み戻し、PSO の作成だけでなく実際の描画を確認
        auto* device = backend->GetDevice();
        const D3D12_QUERY_HEAP_DESC queryDesc{ D3D12_QUERY_HEAP_TYPE_PIPELINE_STATISTICS, 1, 0 };
        ComPtr<ID3D12QueryHeap> query;
        ASSERT_TRUE(SUCCEEDED(device->CreateQueryHeap(&queryDesc, IID_PPV_ARGS(query.GetAddressOf()))));
        D3D12_HEAP_PROPERTIES heap{};
        heap.Type = D3D12_HEAP_TYPE_READBACK;
        heap.CreationNodeMask = heap.VisibleNodeMask = 1;
        D3D12_RESOURCE_DESC buffer{};
        buffer.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        buffer.Width = sizeof(D3D12_QUERY_DATA_PIPELINE_STATISTICS);
        buffer.Height = buffer.DepthOrArraySize = buffer.MipLevels = buffer.SampleDesc.Count = 1;
        buffer.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        ComPtr<ID3D12Resource> readback;
        ASSERT_TRUE(SUCCEEDED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &buffer,
            D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(readback.GetAddressOf()))));

        ASSERT_TRUE(backend->BeginDraw(draws, Matrix4x4::Identity(), Matrix4x4::Identity()));
        auto* commandList = backend->GetCommandList();
        commandList->BeginQuery(query.Get(), D3D12_QUERY_TYPE_PIPELINE_STATISTICS, 0);
        backend->Draw();
        commandList->EndQuery(query.Get(), D3D12_QUERY_TYPE_PIPELINE_STATISTICS, 0);
        commandList->ResolveQueryData(query.Get(), D3D12_QUERY_TYPE_PIPELINE_STATISTICS, 0, 1, readback.Get(), 0);
        backend->EndDraw();
        backend->Finalize();

        const D3D12_RANGE readRange{ 0, sizeof(D3D12_QUERY_DATA_PIPELINE_STATISTICS) };
        void* data = nullptr;
        ASSERT_TRUE(SUCCEEDED(readback->Map(0, &readRange, &data)));
        const auto& statistics = *static_cast<const D3D12_QUERY_DATA_PIPELINE_STATISTICS*>(data);
        EXPECT_EQ(statistics.IAPrimitives, 1u);
        EXPECT_GT(statistics.PSInvocations, 0u);
        const D3D12_RANGE writtenRange{ 0, 0 };
        readback->Unmap(0, &writtenRange);
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

        const auto draw = [&](const Hestia::LegacyMaterial& material, Hestia::AssetHandle<Hestia::Mesh> meshHandle)
        {
            Matrix4x4_SIMD worldMatrix = Matrix4x4_SIMD::Identity();
            worldMatrix.Scale(64.0f, 64.0f, 1.0f);
            worldMatrix.Translate(0.0f, 0.0f, 1.0f);

            std::vector<Hestia::RenderData> renderData = {
                {
                    worldMatrix,
                    material,
                    meshHandle,
                    L"../Build/Shader/VS_Sprite.cso",
                    L"../Build/Shader/PS_Sprite.cso"
                }
            };

            if (!backend->BeginDraw(renderData, Matrix4x4::Identity(), Matrix4x4::Identity()))
                return false;

            backend->Draw();
            backend->EndDraw();
            return true;
        };

        Hestia::LegacyMaterial texturedMaterial = Hestia::LegacyMaterial::Default();
        texturedMaterial.textures.push_back(textureHandle);
        ASSERT_TRUE(draw(texturedMaterial, smallMeshHandle));

        Hestia::LegacyMaterial unsetTextureMaterial = Hestia::LegacyMaterial::Default();
        ASSERT_TRUE(draw(unsetTextureMaterial, smallMeshHandle));

        Hestia::LegacyMaterial invalidTextureMaterial = Hestia::LegacyMaterial::Default();
        invalidTextureMaterial.textures.push_back({});
        ASSERT_TRUE(draw(invalidTextureMaterial, smallMeshHandle));

        auto staleTextureHandle = textureHandle;
        ++staleTextureHandle.m_generation;
        Hestia::LegacyMaterial staleTextureMaterial = Hestia::LegacyMaterial::Default();
        staleTextureMaterial.textures.push_back(staleTextureHandle);
        ASSERT_TRUE(draw(staleTextureMaterial, smallMeshHandle));

        auto missingTextureHandle = textureHandle;
        missingTextureHandle.m_index = 1;
        Hestia::LegacyMaterial missingTextureMaterial = Hestia::LegacyMaterial::Default();
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
