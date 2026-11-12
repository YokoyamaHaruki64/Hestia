/*=============================================================================

 File   : AssetTests.cpp
 Desc   : Asset CPU 公開契約の妥当性、名前、所有権、寿命を検証する。

------------------------------------------------------------------------------

 Date   : 2026/10/15
 Author : Yokoyama Haruki

=============================================================================*/

#include "Common/Include/WindowsHeaders.h"

#include <gtest/gtest.h>
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

#include "Asset/AssetAPI.h"
#include "Asset/AssetDB.h"
#include "Asset/AssetLoader.h"
#include "Asset/AssetStorage.h"
#include "Asset/AssetSystem.h"
#include "Graphics/GraphicsSystem.h"
#include "Log/Logger.h"
#include "Log/LogStorage.h"

namespace
{
    Hestia::Mesh MakeValidMesh()
    {
        Hestia::Mesh mesh;
        mesh.m_positions.resize(3);
        mesh.m_indices = { 0, 1, 2 };
        return mesh;
    }

    struct TrackedAsset
    {
        explicit TrackedAsset(int* destroyedCount)
            : m_destroyedCount(destroyedCount)
        {
        }

        ~TrackedAsset()
        {
            ++*m_destroyedCount;
        }

        Hestia::AssetHandle<TrackedAsset> m_handle;
        int* m_destroyedCount;
    };

    TEST(AssetHandle, DefaultIsInvalidAndGenerationDoesNotDefineValidity)
    {
        Hestia::AssetHandle<Hestia::Mesh> handle;
        EXPECT_FALSE(handle.IsValid());

        handle.m_index = 0;
        EXPECT_TRUE(handle.IsValid());
        EXPECT_EQ(handle.m_generation, 0u);
    }

    TEST(TextureDesc, RequiresPositiveDimensionsAndOneMip)
    {
        Hestia::TextureDesc desc;
        EXPECT_FALSE(desc.IsValid());

        desc.m_width = 1;
        desc.m_height = 1;
        EXPECT_TRUE(desc.IsValid());

        desc.m_width = 0;
        EXPECT_FALSE(desc.IsValid());

        desc.m_width = 1;
        desc.m_mipLevels = 2;
        EXPECT_FALSE(desc.IsValid());

        desc.m_mipLevels = 1;
        desc.m_format = static_cast<Hestia::TextureFormat>(99);
        EXPECT_FALSE(desc.IsValid());
    }

    TEST(Mesh, RequiresPositionsAndIndices)
    {
        Hestia::Mesh mesh;
        EXPECT_FALSE(mesh.IsValid());

        mesh.m_positions.resize(3);
        EXPECT_FALSE(mesh.IsValid());

        mesh.m_indices = { 0, 1, 2 };
        EXPECT_TRUE(mesh.IsValid());
    }

    TEST(Mesh, OptionalStreamsMustMatchPositionsAndIndicesMustBeInRange)
    {
        Hestia::Mesh mesh = MakeValidMesh();
        EXPECT_TRUE(mesh.IsValid());

        mesh.m_normalTangents.resize(2);
        EXPECT_FALSE(mesh.IsValid());

        mesh.m_normalTangents.resize(3);
        EXPECT_TRUE(mesh.IsValid());

        mesh.m_primaryTexCoords.resize(2);
        EXPECT_FALSE(mesh.IsValid());
        mesh.m_primaryTexCoords.resize(3);
        EXPECT_TRUE(mesh.IsValid());

        mesh.m_colors.resize(4);
        EXPECT_FALSE(mesh.IsValid());
        mesh.m_colors.resize(3);
        EXPECT_TRUE(mesh.IsValid());

        mesh.m_indices.back() = 3;
        EXPECT_FALSE(mesh.IsValid());
    }

    TEST(AssetDB, NormalizesAsciiNamesAndSupportsBothInitialTypes)
    {
        Hestia::AssetDB db;
        Hestia::AssetDB::EntryMap entries;
        entries.emplace("MeshOne", Hestia::AssetInfo{
            Hestia::AssetType::Mesh, "Meshes/one.mesh" });
        entries.emplace("TextureOne", Hestia::AssetInfo{
            Hestia::AssetType::Texture, "Textures/one.png" });

        ASSERT_TRUE(db.Initialize(std::move(entries)));

        const Hestia::AssetInfo* meshInfo = db.Find("mEsHoNe");
        ASSERT_NE(meshInfo, nullptr);
        EXPECT_EQ(meshInfo->m_type, Hestia::AssetType::Mesh);
        EXPECT_EQ(meshInfo->m_path, "Meshes/one.mesh");

        const Hestia::AssetInfo* textureInfo = db.Find("textureONE");
        ASSERT_NE(textureInfo, nullptr);
        EXPECT_EQ(textureInfo->m_type, Hestia::AssetType::Texture);
        EXPECT_TRUE(db.Contains("TextureOne"));
    }

    TEST(AssetDB, RejectsEmptyAndAsciiCaseCollidingNames)
    {
        Hestia::AssetDB emptyNameDb;
        Hestia::AssetDB::EntryMap emptyNameEntries;
        emptyNameEntries.emplace("", Hestia::AssetInfo{
            Hestia::AssetType::Mesh, "Meshes/one.mesh" });
        EXPECT_FALSE(emptyNameDb.Initialize(std::move(emptyNameEntries)));

        Hestia::AssetDB collisionDb;
        Hestia::AssetDB::EntryMap collisionEntries;
        collisionEntries.emplace("TextureOne", Hestia::AssetInfo{
            Hestia::AssetType::Texture, "Textures/one.png" });
        collisionEntries.emplace("textureone", Hestia::AssetInfo{
            Hestia::AssetType::Texture, "Textures/two.png" });
        EXPECT_FALSE(collisionDb.Initialize(std::move(collisionEntries)));
    }

    TEST(AssetStorage, ReturnsStableBorrowedAddressAndChecksGeneration)
    {
        Hestia::AssetStorage<Hestia::Mesh> storage;
        auto firstAsset = std::make_unique<Hestia::Mesh>(MakeValidMesh());
        Hestia::Mesh* expectedAddress = firstAsset.get();

        const auto first = storage.Register("Primary", std::move(firstAsset));
        ASSERT_TRUE(first.IsValid());
        ASSERT_EQ(storage.Get(first), expectedAddress);
        EXPECT_EQ(first.m_index, 0u);
        EXPECT_EQ(first.m_generation, 1u);

        for (uint32_t index = 0; index < 64; ++index)
        {
            auto asset = std::make_unique<Hestia::Mesh>(MakeValidMesh());
            ASSERT_TRUE(storage.Register("Mesh" + std::to_string(index), std::move(asset)).IsValid());
        }

        EXPECT_EQ(storage.Get(first), expectedAddress);

        auto stale = first;
        ++stale.m_generation;
        EXPECT_EQ(storage.Get(stale), nullptr);
    }

    TEST(AssetStorage, FindsNamesWithoutAsciiCaseAndRejectsInvalidRegistration)
    {
        Hestia::AssetStorage<Hestia::Mesh> storage;
        const auto handle = storage.Register(
            "StaticMesh", std::make_unique<Hestia::Mesh>(MakeValidMesh()));

        ASSERT_TRUE(handle.IsValid());
        const auto found = storage.Find("sTaTiCmEsH");
        EXPECT_EQ(found.m_index, handle.m_index);
        EXPECT_EQ(found.m_generation, handle.m_generation);
        EXPECT_FALSE(storage.Find("Missing").IsValid());
        EXPECT_FALSE(storage.Register("", std::make_unique<Hestia::Mesh>(MakeValidMesh())).IsValid());
        EXPECT_FALSE(storage.Register("NoAsset", nullptr).IsValid());
        EXPECT_FALSE(storage.Contains(""));
        EXPECT_FALSE(storage.Contains("NoAsset"));
    }

    TEST(AssetStorage, ConsumesRejectedAssetsAndOwnsAcceptedAssetsUntilDestruction)
    {
        int destroyedCount = 0;
        {
            Hestia::AssetStorage<TrackedAsset> storage;
            const auto accepted = storage.Register(
                "Tracked", std::make_unique<TrackedAsset>(&destroyedCount));
            ASSERT_TRUE(accepted.IsValid());
            EXPECT_EQ(destroyedCount, 0);

            EXPECT_FALSE(storage.Register(
                "tRaCkEd", std::make_unique<TrackedAsset>(&destroyedCount)).IsValid());
            EXPECT_EQ(destroyedCount, 1);

            EXPECT_FALSE(storage.Register(
                "", std::make_unique<TrackedAsset>(&destroyedCount)).IsValid());
            EXPECT_EQ(destroyedCount, 2);

            EXPECT_FALSE(storage.Register("Null", nullptr).IsValid());
            EXPECT_EQ(destroyedCount, 2);
            EXPECT_NE(storage.Get(accepted), nullptr);
        }

        EXPECT_EQ(destroyedCount, 3);
    }

    TEST(AssetSystem, AllowsOneInactiveGraphicsAndRetainsMeshAfterUploadFailure)
    {
        Hestia::GraphicsSystem graphics;
        Hestia::AssetSystem system;
        ASSERT_TRUE(system.Initialize(&graphics));

        EXPECT_FALSE(Hestia::AssetSystem::Register<Hestia::Mesh>(
            "texturetest", new Hestia::Mesh(MakeValidMesh())).IsValid());
        EXPECT_FALSE(Hestia::AssetSystem::Register<Hestia::Texture>(
            "ManualTexture", new Hestia::Texture()).IsValid());
        EXPECT_FALSE(Hestia::AssetSystem::Register<Hestia::Mesh>(
            "InvalidThenValid", new Hestia::Mesh()).IsValid());
        EXPECT_TRUE(Hestia::AssetSystem::Register<Hestia::Mesh>(
            "InvalidThenValid", new Hestia::Mesh(MakeValidMesh())).IsValid());

        Hestia::AssetHandle<Hestia::Mesh> handle =
            Hestia::AssetSystem::Register<Hestia::Mesh>("StaticMesh", new Hestia::Mesh(MakeValidMesh()));
        ASSERT_TRUE(handle.IsValid());
        ASSERT_NE(Hestia::AssetSystem::Get<Hestia::Mesh>(handle), nullptr);

        const auto cached = Hestia::AssetSystem::Load<Hestia::Mesh>("sTaTiCmEsH");
        EXPECT_EQ(cached.m_index, handle.m_index);
        EXPECT_EQ(cached.m_generation, handle.m_generation);

        auto invalidIndex = handle;
        invalidIndex.m_index = UINT32_MAX;
        EXPECT_EQ(Hestia::AssetSystem::Get<Hestia::Mesh>(invalidIndex), nullptr);
        EXPECT_EQ(Hestia::AssetSystem::Get<Hestia::Mesh>({ 1024, 1 }), nullptr);

        auto stale = handle;
        ++stale.m_generation;
        EXPECT_EQ(Hestia::AssetSystem::Get<Hestia::Mesh>(stale), nullptr);

        system.Finalize();
        EXPECT_EQ(Hestia::AssetSystem::Get<Hestia::Mesh>(handle), nullptr);
    }

    TEST(AssetSystem, RejectsNullGraphicsAndSecondActiveInstance)
    {
        Hestia::AssetSystem first;
        EXPECT_FALSE(first.Initialize(nullptr));

        Hestia::GraphicsSystem graphics;
        ASSERT_TRUE(first.Initialize(&graphics));

        Hestia::AssetSystem second;
        EXPECT_FALSE(second.Initialize(&graphics));

        first.Finalize();
        EXPECT_TRUE(second.Initialize(&graphics));
        second.Finalize();
    }

    class MaterialShaderLoaderTest : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            // AssetDB の相対パスを既存のテストランナーと同じ起点で解決
            m_originalDirectory = std::filesystem::current_path();
            auto repository = m_originalDirectory;
            while (!std::filesystem::exists(repository / "Engine" / "Engine.vcxproj"))
            {
                const auto parent = repository.parent_path();
                ASSERT_NE(parent, repository) << "Hestia repository root was not found";
                repository = parent;
            }
            std::filesystem::current_path(repository / "Test");

            // GPU を起動せず、ロード時の診断を同期的に確認
            ASSERT_TRUE(m_logStorage.Initialize(16));
            m_logStorage.Open();
            ASSERT_TRUE(m_logger.Initialize(&m_logStorage));
            ASSERT_TRUE(m_assets.Initialize(&m_graphics));
        }

        void TearDown() override
        {
            m_assets.Finalize();
            m_logger.Finalize();
            m_logStorage.Close();
            m_logStorage.Finalize();

            if (!m_tempDirectory.empty())
            {
                std::filesystem::remove(m_tempDirectory / "Shader.matsh");
                std::filesystem::remove(m_tempDirectory);
            }
            std::filesystem::current_path(m_originalDirectory);
        }

        void WriteShaderSource(std::string_view source, std::string& sourcePath)
        {
            const auto directory = std::filesystem::temp_directory_path()
                / ("HestiaMaterialShaderTests-" + std::to_string(GetCurrentProcessId()));
            ASSERT_TRUE(std::filesystem::create_directory(directory));
            m_tempDirectory = directory;
            const auto path = m_tempDirectory / "Shader.matsh";
            std::ofstream file(path, std::ios::binary);
            file << source;
            file.close();
            ASSERT_TRUE(file.good());

            const auto utf8Path = path.u8string();
            sourcePath.assign(utf8Path.begin(), utf8Path.end());
        }

        void ExpectRejectedSource(std::string_view source, std::string_view diagnostic)
        {
            std::string sourcePath;
            ASSERT_NO_FATAL_FAILURE(WriteShaderSource(source, sourcePath));

            // AssetDB と一致する名前を使い、登録時の名前検証で失敗するだけのケースを除く
            EXPECT_FALSE(Hestia::AssetLoader<Hestia::MaterialShader>{}.Load("Unlit", sourcePath).IsValid());

            // 意図した失敗経路を確認してから、同じ名前で正常なファイルをロード
            std::vector<Hestia::LogEntry> entries;
            m_logStorage.TransferPending(16);
            m_logStorage.Drain(entries, 16);
            EXPECT_TRUE(std::any_of(entries.begin(), entries.end(), [diagnostic](const auto& entry)
            {
                return entry.m_level == Hestia::LogLevel::Error
                    && entry.m_message.find(diagnostic) != std::string::npos;
            })) << diagnostic;

            const auto valid = Hestia::AssetSystem::Load<Hestia::MaterialShader>("Unlit");
            const auto* shader = Hestia::AssetSystem::Get(valid);
            ASSERT_NE(shader, nullptr);
            EXPECT_EQ(shader->GetName(), "Hestia/Unlit");
        }

        Hestia::GraphicsSystem m_graphics;
        Hestia::AssetSystem m_assets;
        Hestia::LogStorage m_logStorage;
        Hestia::Logger m_logger;
        std::filesystem::path m_originalDirectory;
        std::filesystem::path m_tempDirectory;
    };

    TEST_F(MaterialShaderLoaderTest, LoadsForwardPassAndMergesReflectionAcrossStages)
    {
        // AssetSystem の通常経路でコンパイルし、別表記のロードで同じ Asset を取得
        const auto handle = Hestia::AssetSystem::Load<Hestia::MaterialShader>("Unlit");
        ASSERT_TRUE(handle.IsValid());
        const auto* shader = Hestia::AssetSystem::Get(handle);
        ASSERT_NE(shader, nullptr);
        EXPECT_EQ(shader->GetName(), "Hestia/Unlit");
        EXPECT_EQ(shader->GetHandle().m_index, handle.m_index);
        EXPECT_EQ(shader->GetHandle().m_generation, handle.m_generation);

        const auto cached = Hestia::AssetSystem::Load<Hestia::MaterialShader>("uNLIT");
        EXPECT_EQ(cached.m_index, handle.m_index);
        EXPECT_EQ(cached.m_generation, handle.m_generation);
        EXPECT_EQ(Hestia::AssetSystem::Get(cached), shader);

        // Forward の両 Stage の出力と頂点入力を確認
        const auto passes = shader->GetPasses();
        ASSERT_FALSE(passes.empty());
        const auto& pass = passes.front();
        EXPECT_EQ(pass.m_info.m_name, "Unlit");
        EXPECT_EQ(pass.m_info.m_renderType, Hestia::RenderType::Forward);
        EXPECT_FALSE(pass.m_pipeline.m_vertexShader.empty());
        EXPECT_FALSE(pass.m_pipeline.m_pixelShader.empty());

        const auto& streams = pass.m_pipeline.m_requiredVertexStreams;
        EXPECT_EQ(streams.size(), 2u);
        EXPECT_EQ(std::ranges::count(streams, Hestia::VertexStream::Position), 1);
        EXPECT_EQ(std::ranges::count(streams, Hestia::VertexStream::PrimaryTexCoord), 1);

        // PS と VS で使う値の配置、未使用の値、SRV スロットを一つのスキーマから取得
        const auto* tint = shader->FindProperty("_Tint");
        ASSERT_NE(tint, nullptr);
        const auto* color = std::get_if<Color>(&tint->m_defaultValue);
        ASSERT_NE(color, nullptr);
        EXPECT_FLOAT_EQ(color->r, 1.0f);
        EXPECT_FLOAT_EQ(color->g, 1.0f);
        EXPECT_FLOAT_EQ(color->b, 1.0f);
        EXPECT_FLOAT_EQ(color->a, 1.0f);
        EXPECT_EQ(tint->m_offset, 0u);
        EXPECT_EQ(tint->m_size, 16u);

        const auto* scale = shader->FindProperty("_Scale");
        ASSERT_NE(scale, nullptr);
        const auto* scaleDefault = std::get_if<float>(&scale->m_defaultValue);
        ASSERT_NE(scaleDefault, nullptr);
        EXPECT_FLOAT_EQ(*scaleDefault, 1.0f);
        EXPECT_EQ(scale->m_offset, 16u);
        EXPECT_EQ(scale->m_size, 4u);
        EXPECT_GE(shader->GetConstantBufferSize(), 20u);

        const auto* unused = shader->FindProperty("_Unused");
        ASSERT_NE(unused, nullptr);
        const auto* unusedDefault = std::get_if<float>(&unused->m_defaultValue);
        ASSERT_NE(unusedDefault, nullptr);
        EXPECT_FLOAT_EQ(*unusedDefault, 2.0f);
        EXPECT_EQ(unused->m_offset, UINT32_MAX);

        const auto* baseMap = shader->FindProperty("_BaseMap");
        ASSERT_NE(baseMap, nullptr);
        EXPECT_TRUE(std::holds_alternative<Hestia::AssetHandle<Hestia::Texture>>(baseMap->m_defaultValue));
        EXPECT_EQ(baseMap->m_resourceSlot, 0u);
    }

    TEST_F(MaterialShaderLoaderTest, RequiresOneNormalTangentStreamForEitherOrBothInputs)
    {
        // 同じ Stream を共有する入力を、片方ずつと両方で宣言する
        std::string sourcePath;
        ASSERT_NO_FATAL_FAILURE(WriteShaderSource(R"(
MaterialShader "Test/NormalTangent"
{
    RenderType = Forward;

    SHARED_HLSL_BEGIN
    float4 VSNormal(float3 normal : NORMAL) : SV_Position { return float4(normal, 1); }
    float4 VSTangent(float4 tangent : TANGENT) : SV_Position { return tangent; }
    float4 VSBoth(float3 normal : NORMAL, float4 tangent : TANGENT) : SV_Position
    {
        return float4(normal, 0) + tangent;
    }
    float4 PSMain() : SV_Target0 { return float4(1, 1, 1, 1); }
    SHARED_HLSL_END

    Pass "Normal"
    {
        HLSL_BEGIN
        #pragma vertex VSNormal
        #pragma pixel PSMain
        HLSL_END
    }
    Pass "Tangent"
    {
        HLSL_BEGIN
        #pragma vertex VSTangent
        #pragma pixel PSMain
        HLSL_END
    }
    Pass "Both"
    {
        HLSL_BEGIN
        #pragma vertex VSBoth
        #pragma pixel PSMain
        HLSL_END
    }
}
)", sourcePath));

        // どの Pass もバッファの要求は NormalTangent 1件
        const auto handle = Hestia::AssetLoader<Hestia::MaterialShader>{}.Load("Unlit", sourcePath);
        ASSERT_TRUE(handle.IsValid());
        const auto* shader = Hestia::AssetSystem::Get(handle);
        ASSERT_NE(shader, nullptr);
        ASSERT_EQ(shader->GetPasses().size(), 3u);
        for (const auto& pass : shader->GetPasses())
        {
            SCOPED_TRACE(pass.m_info.m_name);
            EXPECT_EQ(pass.m_pipeline.m_requiredVertexStreams,
                std::vector<Hestia::VertexStream>{ Hestia::VertexStream::NormalTangent });
        }
    }

    TEST_F(MaterialShaderLoaderTest, RejectsUnsupportedGBufferBeforeRegistration)
    {
        ExpectRejectedSource(R"(
MaterialShader "Test/Unsupported"
{
    Pass
    {
        RenderType = GBuffer;
        ShadingModel = None;
        HLSL_BEGIN
        #pragma vertex VSMain
        #pragma surface SurfaceMain
        float4 VSMain(float3 position : POSITION) : SV_Position { return float4(position, 1); }
        float4 SurfaceMain() : SV_Target { return float4(1, 1, 1, 1); }
        HLSL_END
    }
}
)", "GBuffer Pass compilation is not implemented");
    }

    TEST_F(MaterialShaderLoaderTest, RejectsHlslCompilationFailureBeforeRegistration)
    {
        ExpectRejectedSource(R"(
MaterialShader "Test/Broken"
{
    Pass
    {
        RenderType = Forward;
        HLSL_BEGIN
        #pragma vertex VSMain
        #pragma pixel PSMain
        float4 VSMain(float3 position : POSITION) : SV_Position { return float4(position, 1); }
        float4 PSMain() : SV_Target { return MissingShaderValue; }
        HLSL_END
    }
}
)", "MissingShaderValue");
    }

    TEST(AssetAPI, UnsetApiFailsSafely)
    {
        Hestia::AssetAPI api;
        EXPECT_FALSE(api.Load<Hestia::Mesh>("AnyMesh").IsValid());
        EXPECT_EQ(api.Get<Hestia::Texture>({}), nullptr);
        EXPECT_FALSE(api.Register<Hestia::Mesh>(
            "AnyMesh", new Hestia::Mesh(MakeValidMesh())).IsValid());
    }
}
