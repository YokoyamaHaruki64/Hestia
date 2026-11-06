/*=============================================================================

 File   : AssetTests.cpp
 Desc   : Asset CPU 公開契約の妥当性、名前、所有権、寿命を検証する。

------------------------------------------------------------------------------

 Date   : 2026/10/15
 Author : Yokoyama Haruki

=============================================================================*/

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "Asset/AssetAPI.h"
#include "Asset/AssetDB.h"
#include "Asset/AssetStorage.h"
#include "Asset/AssetSystem.h"
#include "Graphics/GraphicsSystem.h"

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

    TEST(AssetAPI, UnsetApiFailsSafely)
    {
        Hestia::AssetAPI api;
        EXPECT_FALSE(api.Load<Hestia::Mesh>("AnyMesh").IsValid());
        EXPECT_EQ(api.Get<Hestia::Texture>({}), nullptr);
        EXPECT_FALSE(api.Register<Hestia::Mesh>(
            "AnyMesh", new Hestia::Mesh(MakeValidMesh())).IsValid());
    }
}
