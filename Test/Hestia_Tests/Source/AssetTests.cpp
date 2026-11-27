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
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "Asset/AssetDB.h"
#include "Asset/AssetLoader.h"
#include "Asset/AssetStorage.h"
#include "Asset/AssetSystem.h"
#include "ECS/Component.h"
#include "ECS/ECSSystem.h"
#include "Graphics/GraphicsSystem.h"
#include "Graphics/MeshRenderer.h"
#include "Log/Logger.h"
#include "Log/LogStorage.h"
#include "Game/Include/GameAssets.h"
#include "Serialize/EntityYamlReader.h"
#include "Transform/TransformData.h"
#include "Transform/TransformProcess.h"

namespace
{
    void WriteAssetFile(const std::filesystem::path& path, std::string_view source)
    {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        file << source;
        file.close();
        ASSERT_TRUE(file.good());
    }

    class MaterialAssetTest : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            const auto* test = ::testing::UnitTest::GetInstance()->current_test_info();
            m_directory = std::filesystem::temp_directory_path() /
                ("HestiaMaterial-" + std::to_string(GetCurrentProcessId()) + "-" + test->name());
            ASSERT_TRUE(std::filesystem::create_directory(m_directory));
            ASSERT_NO_FATAL_FAILURE(WriteAssetFile(m_directory / "Shader.matsh.meta",
                "type: MaterialShader\nguid: b4c019ce-181f-46c4-bf5f-c6c2cdc634a2\n"));
            ASSERT_NO_FATAL_FAILURE(WriteAssetFile(m_directory / "Shader.matsh", R"(
MaterialShader "Tests/Material"
{
    RenderType = Forward;
    RenderPhase = Opaque;
    Properties
    {
        _Color ("Color", Color) = (1, 1, 1, 1)
        _Roughness ("Roughness", Float) = 0.5
        _Matrix ("Matrix", Matrix3x3) = (1, 0, 0, 0, 1, 0, 0, 0, 1)
        _Albedo ("Albedo", Texture2D) = "white"
    }
    Pass "Forward"
    {
        HLSL_BEGIN
        #pragma vertex VSMain
        #pragma pixel PSMain
        float4 VSMain(float3 position : POSITION) : SV_POSITION { return float4(position, 1); }
        float4 PSMain() : SV_TARGET { return _Color; }
        HLSL_END
    }
}
)"));
        }

        void TearDown() override
        {
            m_assets.Finalize();
            std::filesystem::remove_all(m_directory);
        }

        static constexpr std::string_view MATERIAL_HEADER =
            "version: 1\nshader: { guid: b4c019ce-181f-46c4-bf5f-c6c2cdc634a2, localId: 0 }\n";

        std::filesystem::path m_directory;
        Hestia::GraphicsSystem m_graphics;
        Hestia::AssetSystem m_assets;
    };

    TEST_F(MaterialAssetTest, LoadsOverridesAndRejectsInvalidProperties)
    {
        using namespace Hestia;
        ASSERT_NO_FATAL_FAILURE(WriteAssetFile(m_directory / "Valid.material", std::string(MATERIAL_HEADER) +
            "properties:\n  _Color: [0.25, 0.5, 0.75, 1]\n  _Albedo: null\n"));
        ASSERT_NO_FATAL_FAILURE(WriteAssetFile(m_directory / "Invalid.material", std::string(MATERIAL_HEADER) +
            "properties:\n  _Roughness: 0.25\n  _Roughness: 0.75\n"));
        ASSERT_NO_FATAL_FAILURE(WriteAssetFile(m_directory / "MissingTexture.material", std::string(MATERIAL_HEADER) +
            "properties:\n  _Albedo: { guid: 8810ccc3-339e-4408-9dc2-57be100dad4a, localId: 0 }\n"));
        ASSERT_TRUE(m_assets.Initialize(&m_graphics, m_directory));

        // Shaderから継承した既定値と、ファイル側の上書きを同じMaterialから取得する
        const auto handle = AssetSystem::Load<Material>("Valid.material");
        const auto* material = AssetSystem::Get(handle);
        ASSERT_NE(material, nullptr);
        EXPECT_FLOAT_EQ(material->GetColor("_Color").r, 0.25f);
        EXPECT_FLOAT_EQ(material->GetFloat("_Roughness"), 0.5f);
        EXPECT_FALSE(material->GetTexture("_Albedo").IsValid());
        EXPECT_EQ(AssetSystem::Get(AssetSystem::Load<Material>(AssetSystem::GetReference(handle))), material);

        EXPECT_FALSE(AssetSystem::Load<Material>("Invalid.material").IsValid());
        EXPECT_FALSE(AssetSystem::Load<Material>("MissingTexture.material").IsValid());
    }

    TEST_F(MaterialAssetTest, SavesOverridesToAnAvailablePathAndKeepsTheGuidOnOverwrite)
    {
        using namespace Hestia;
        ASSERT_TRUE(m_assets.Initialize(&m_graphics, m_directory));
        auto* shader = AssetSystem::Get(AssetSystem::Load<MaterialShader>("Shader.matsh"));
        ASSERT_NE(shader, nullptr);
        auto material = std::make_unique<Material>();
        material->SetShader(shader);
        ASSERT_TRUE(material->SetColor("_Color", Color{ 0.25f, 0.75f, 0.5f, 1.0f }));
        const Matrix3x3 matrix{ 1, 0, 0, 5, 1, 0, 0, 7, 1 };
        ASSERT_TRUE(material->SetMatrix3x3("_Matrix", matrix));
        const auto runtime = AssetSystem::Register<Material>("paint", material.release());
        ASSERT_TRUE(runtime.IsValid());

        // 同名のメタデータを保護し、空き名で登録されたMaterialをすぐにロードする
        ASSERT_NO_FATAL_FAILURE(WriteAssetFile(m_directory / "paint.material.meta", "existing metadata"));
        const auto path = AssetSystem::Save(runtime);
        ASSERT_TRUE(path);
        EXPECT_EQ(*path, "paint_1.material");
        EXPECT_FALSE(std::filesystem::exists(m_directory / "paint.material"));
        std::filesystem::remove(m_directory / "paint.material.meta");
        const auto loaded = AssetSystem::Load<Material>(*path);
        const auto* restored = AssetSystem::Get(loaded);
        ASSERT_NE(restored, nullptr);
        EXPECT_FLOAT_EQ(restored->GetColor("_Color").g, 0.75f);
        EXPECT_FLOAT_EQ(restored->GetMatrix3x3("_Matrix").m[1][0], 5.0f);
        EXPECT_FLOAT_EQ(restored->GetMatrix3x3("_Matrix").m[2][1], 7.0f);
        EXPECT_FLOAT_EQ(restored->GetFloat("_Roughness"), 0.5f);
        EXPECT_FALSE(AssetSystem::Save(runtime, *path));

        // 既定値をYAMLへ固定せず、上書き後も同じ参照で再起動時に復元できる
        std::ifstream file(m_directory / *path, std::ios::binary);
        const std::string yaml((std::istreambuf_iterator<char>(file)), {});
        file.close();
        EXPECT_EQ(yaml.find("_Roughness"), std::string::npos);
        const auto ref = AssetSystem::GetReference(loaded);
        ASSERT_TRUE(ref.m_guid);
        ASSERT_TRUE(AssetSystem::Get(loaded)->SetFloat("_Roughness", 0.75f));
        EXPECT_EQ(AssetSystem::Save(loaded), path);
        m_assets.Finalize();
        ASSERT_TRUE(m_assets.Initialize(&m_graphics, m_directory));
        const auto reopened = AssetSystem::Load<Material>(*path);
        const auto* updated = AssetSystem::Get(reopened);
        ASSERT_NE(updated, nullptr);
        EXPECT_EQ(AssetSystem::GetReference(reopened), ref);
        EXPECT_FLOAT_EQ(updated->GetFloat("_Roughness"), 0.75f);
    }

    TEST(Prefab, SavesLoadsAndInstantiatesIndependentHierarchies)
    {
        using namespace Hestia;
        const auto directory = std::filesystem::temp_directory_path() /
            ("HestiaPrefab-" + std::to_string(GetCurrentProcessId()));
        std::filesystem::create_directory(directory);
        ASSERT_NO_FATAL_FAILURE(WriteAssetFile(directory / "Triangle.obj",
            "o Triangle\nv 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n"));

        GraphicsSystem graphics;
        AssetSystem assets;
        ECSSystem ecs;
        ASSERT_TRUE(ecs.Initialize());
        ASSERT_TRUE(assets.Initialize(&graphics, directory));
        const auto model = AssetSystem::Load<Prefab>("Triangle.obj");
        const auto root = ECSSystem::Instantiate(model);
        ASSERT_TRUE(root.IsValid());
        const auto& children = ECSSystem::GetComponent<TransformHierarchy>(root)->m_children;
        ASSERT_EQ(children.size(), 1u);
        const auto child = children.front();
        const auto* renderer = ECSSystem::GetComponent<MeshRenderer>(child);
        ASSERT_TRUE(renderer && renderer->m_mesh);
        const auto meshRef = AssetSystem::GetReference(renderer->m_mesh->m_handle);
        ASSERT_TRUE(static_cast<bool>(meshRef.m_guid));
        ECSSystem::GetComponent<Name>(child)->m_value = "Saved Triangle";
        ASSERT_TRUE(TransformProcess::SetLocalTRS(*ECSSystem::GetComponent<Transform>(child),
            *ECSSystem::GetComponent<TransformHierarchy>(child), Vector3(4, 5, 6),
            Quaternion::Identity(), Vector3::One()));

        const auto snapshot = ECSSystem::SaveEntity(root);
        ASSERT_TRUE(snapshot);
        const auto saved = AssetSystem::Save(*snapshot, "Saved");
        ASSERT_TRUE(saved);
        EXPECT_EQ(*saved, "Saved.prefab");
        EXPECT_TRUE(AssetSystem::Load<Prefab>(*saved).IsValid());
        ECSSystem::DestroyEntity(root);
        ECSSystem::EndFrame();
        assets.Finalize();
        ASSERT_TRUE(assets.Initialize(&graphics, directory));

        const auto handle = AssetSystem::Load<Prefab>("Saved.prefab");
        const auto* prefab = AssetSystem::Get(handle);
        ASSERT_NE(prefab, nullptr);
        EXPECT_EQ(prefab->m_name, "Saved");
        const auto first = ECSSystem::Instantiate(*prefab);
        const auto second = ECSSystem::Instantiate(handle, Vector3(1, 2, 3),
            Quaternion(0, 1, 0, 0), Vector3(2, 2, 2));
        ASSERT_TRUE(first.IsValid() && second.IsValid());
        EXPECT_NE(first, second);
        const auto& firstChildren = ECSSystem::GetComponent<TransformHierarchy>(first)->m_children;
        const auto& secondChildren = ECSSystem::GetComponent<TransformHierarchy>(second)->m_children;
        ASSERT_EQ(firstChildren.size(), 1u);
        ASSERT_EQ(secondChildren.size(), 1u);
        EXPECT_NE(firstChildren.front(), secondChildren.front());
        for (const auto instance : { first, second })
        {
            const auto restored = ECSSystem::GetComponent<TransformHierarchy>(instance)->m_children.front();
            EXPECT_EQ(ECSSystem::GetComponent<TransformHierarchy>(restored)->m_parent, instance);
            EXPECT_EQ(ECSSystem::GetComponent<Name>(restored)->m_value, "Saved Triangle");
            EXPECT_FLOAT_EQ(ECSSystem::GetComponent<Transform>(restored)->m_localPosition.x, 4.0f);
            const auto* restoredRenderer = ECSSystem::GetComponent<MeshRenderer>(restored);
            ASSERT_TRUE(restoredRenderer && restoredRenderer->m_mesh);
            EXPECT_EQ(AssetSystem::GetReference(restoredRenderer->m_mesh->m_handle), meshRef);
        }
        EXPECT_FLOAT_EQ(ECSSystem::GetComponent<Transform>(first)->m_localPosition.x, 0.0f);
        const auto* overrideTransform = ECSSystem::GetComponent<Transform>(second);
        EXPECT_FLOAT_EQ(overrideTransform->m_localPosition.x, 1.0f);
        EXPECT_FLOAT_EQ(overrideTransform->m_localRotation.y, 1.0f);
        EXPECT_FLOAT_EQ(overrideTransform->m_localScale.x, 2.0f);

        std::filesystem::remove(directory / "Saved.prefab.meta");
        std::filesystem::remove(directory / "Saved.prefab");
        std::filesystem::remove(directory / "Triangle.obj.meta");
        std::filesystem::remove(directory / "Triangle.obj");
        std::filesystem::remove(directory);
    }

    TEST(AssetDB, FindsSceneAndNestedScriptWithStableGuids)
    {
        const auto root = std::filesystem::temp_directory_path() /
            ("HestiaAssetDB-SceneScript-" + std::to_string(GetCurrentProcessId()));
        std::filesystem::create_directories(root / "Actors");
        ASSERT_NO_FATAL_FAILURE(WriteAssetFile(root / "Main.scene", ""));
        ASSERT_NO_FATAL_FAILURE(WriteAssetFile(root / "Actors" / "Spin.cpp", ""));

        Hestia::AssetDB db;
        ASSERT_TRUE(db.Initialize(root));
        const auto* scene = db.FindByPath("Main.scene");
        const auto* script = db.FindByPath("Actors/Spin.cpp");
        EXPECT_NE(scene, nullptr);
        EXPECT_NE(script, nullptr);
        if (scene && script)
        {
            const auto sceneGuid = scene->m_guid;
            const auto scriptGuid = script->m_guid;
            EXPECT_NE(sceneGuid, scriptGuid);
            ASSERT_TRUE(db.Initialize(root));
            EXPECT_EQ(db.FindByGuid(sceneGuid), db.FindByPath("Main.scene"));
            EXPECT_EQ(db.FindByGuid(scriptGuid), db.FindByPath("Actors/Spin.cpp"));
        }
        EXPECT_TRUE(std::filesystem::exists(root / "Main.scene.meta"));
        EXPECT_TRUE(std::filesystem::exists(root / "Actors" / "Spin.cpp.meta"));
        std::filesystem::remove_all(root);
    }

    TEST(AssetDB, RejectsUnknownMetadataVersionBeforeCreatingMissingMetadata)
    {
        const auto root = std::filesystem::temp_directory_path() /
            ("HestiaAssetDB-Version-" + std::to_string(GetCurrentProcessId()));
        std::filesystem::create_directories(root);
        ASSERT_NO_FATAL_FAILURE(WriteAssetFile(root / "A.png", ""));
        ASSERT_NO_FATAL_FAILURE(WriteAssetFile(root / "Z.png", ""));
        ASSERT_NO_FATAL_FAILURE(WriteAssetFile(root / "Z.png.meta",
            "version: 2\ntype: Texture\nguid: 94b2374d-0a87-420d-9e11-16dcacbde2fd\n"));

        Hestia::AssetDB db;
        EXPECT_FALSE(db.Initialize(root));
        EXPECT_FALSE(std::filesystem::exists(root / "A.png.meta"));
        std::filesystem::remove_all(root);
    }

    TEST(SceneSerialization, RoundTripsMultipleRootsActiveStateAndChildScript)
    {
        using namespace Hestia;
        const auto directory = std::filesystem::temp_directory_path() /
            ("HestiaScene-" + std::to_string(GetCurrentProcessId()));
        std::filesystem::create_directories(directory);
        GraphicsSystem graphics;
        AssetSystem assets;
        ASSERT_TRUE(assets.Initialize(&graphics, directory));
        ECSSystem ecs;
        ASSERT_TRUE(ecs.Initialize());
        const auto parent = ECSSystem::CreateEntity();
        const auto child = ECSSystem::CreateEntity();
        const auto other = ECSSystem::CreateEntity();
        ECSSystem::GetComponent<TransformHierarchy>(parent)->m_children.push_back(child);
        ECSSystem::GetComponent<TransformHierarchy>(child)->m_parent = parent;
        ASSERT_TRUE(ECSSystem::SetActive(child, false));
        ECSSystem::GetComponent<Name>(other)->m_value = "Second root";
        ECSSystem::AddProcess<&MeshRendererProcess::Run>(child);
        auto* renderer = ECSSystem::GetComponent<MeshRenderer>(child);
        ASSERT_NE(renderer, nullptr);
        renderer->m_mesh = AssetSystem::Get(AssetSystem::Load<Mesh>("Cube"));
        ASSERT_NE(renderer->m_mesh, nullptr);
        const AssetGuid guid{ { 8, 1 } };
        const ScriptGuidsByEntity scriptGuids{ { child, { guid } } };
        auto first = ECSSystem::SaveEntity(parent, scriptGuids);
        auto second = ECSSystem::SaveEntity(other);
        ASSERT_TRUE(first && second);
        const std::array roots{ std::move(*first), std::move(*second) };

        const auto path = directory / "Saved.scene";
        ASSERT_TRUE(AssetSystem::Save(roots, "Saved.scene"));
        ASSERT_NE(assets.GetDatabase().FindByPath("Saved.scene"), nullptr);

        // キャッシュを初期化し直し、Sceneの参照だけから組み込みMeshを生成できるようにする
        ASSERT_TRUE(ECSSystem::ClearSceneEntities(nullptr, nullptr));
        assets.Finalize();
        ASSERT_TRUE(assets.Initialize(&graphics, directory));
        auto loaded = EntityYamlReader::LoadScene(path);
        ASSERT_TRUE(loaded);
        ASSERT_EQ(loaded->size(), 2u);
        EXPECT_TRUE(loaded->at(0).m_active);
        ASSERT_EQ(loaded->at(0).m_children.size(), 1u);
        const auto& savedChild = loaded->at(0).m_children.front();
        EXPECT_FALSE(savedChild.m_active);
        ASSERT_EQ(savedChild.m_scripts.size(), 1u);
        EXPECT_EQ(savedChild.m_scripts.front(), (AssetRef{ guid, 0 }));
        EXPECT_FALSE(savedChild.m_source.IsValid());
        EXPECT_TRUE(loaded->at(1).m_children.empty());

        std::vector<SceneScriptAttachment> restoredScripts;
        ASSERT_TRUE(ECSSystem::InstantiateScene(*loaded, restoredScripts));
        ASSERT_EQ(restoredScripts.size(), 1u);
        const auto* restoredRenderer = ECSSystem::GetComponent<MeshRenderer>(restoredScripts.front().m_entity);
        ASSERT_TRUE(restoredRenderer && restoredRenderer->m_mesh);
        const auto* restoredMesh = restoredRenderer->m_mesh;
        const AssetRef cubeRef{ AssetGuid::Parse("e9bba8da-8941-4ec8-9b7a-90ff8eee3708"), 0 };
        EXPECT_EQ(AssetSystem::GetReference(restoredMesh->m_handle), cubeRef);
        EXPECT_EQ(AssetSystem::Get(AssetSystem::Load<Mesh>("Cube")), restoredMesh);
        std::filesystem::remove_all(directory);
    }

    TEST(SceneSerialization, RejectsDisconnectedParentCycle)
    {
        using namespace Hestia;
        const auto path = std::filesystem::temp_directory_path() /
            ("HestiaScene-Cycle-" + std::to_string(GetCurrentProcessId()) + ".scene");
        ASSERT_NO_FATAL_FAILURE(WriteAssetFile(path,
            "version: 1\nentities:\n"
            "  - { id: 1, parent: 2, active: true, components: {} }\n"
            "  - { id: 2, parent: 1, active: true, components: {} }\n"));
        ECSSystem ecs;
        ASSERT_TRUE(ecs.Initialize());
        EXPECT_FALSE(EntityYamlReader::LoadScene(path));
        std::filesystem::remove(path);
    }

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

    TEST(AssetDB, GeneratesMetadataAndFindsTheSameAssetByPathAndGuid)
    {
        const auto root = std::filesystem::temp_directory_path()
            / ("HestiaAssetDB-Generate-" + std::to_string(GetCurrentProcessId()));
        std::filesystem::create_directory(root);
        ASSERT_NO_FATAL_FAILURE(WriteAssetFile(root / "One.PNG", ""));

        Hestia::AssetDB db;
        ASSERT_TRUE(db.Initialize(root));
        EXPECT_TRUE(std::filesystem::exists(root / "One.PNG.meta"));
        const auto* texture = db.FindByPath("one.png");
        ASSERT_NE(texture, nullptr);
        EXPECT_EQ(texture->m_type, Hestia::AssetType::Texture);
        EXPECT_EQ(texture->m_path, "One.PNG");
        const auto guid = texture->m_guid;
        EXPECT_EQ(db.FindByGuid(guid), texture);

        ASSERT_TRUE(db.Initialize(root));
        EXPECT_EQ(db.FindByPath("One.PNG"), db.FindByGuid(guid));
        EXPECT_NE(db.FindByGuid(guid), nullptr);
        EXPECT_EQ(db.FindByPath("missing.png"), nullptr);

        std::filesystem::remove(root / "One.PNG.meta");
        std::filesystem::remove(root / "One.PNG");
        std::filesystem::remove(root);
    }

    TEST(AssetDB, RejectsInvalidMetadataAndDuplicateGuids)
    {
        const auto root = std::filesystem::temp_directory_path()
            / ("HestiaAssetDB-Invalid-" + std::to_string(GetCurrentProcessId()));
        std::filesystem::create_directory(root);
        constexpr std::string_view validMeta =
            "type: Texture\nguid: 2db48ec9-7a6f-49a2-8c79-73639d0a8572\n";
        ASSERT_NO_FATAL_FAILURE(WriteAssetFile(root / "One.png", ""));
        ASSERT_NO_FATAL_FAILURE(WriteAssetFile(root / "One.png.meta", validMeta));

        Hestia::AssetDB db;
        ASSERT_TRUE(db.Initialize(root));
        const auto* existing = db.FindByPath("One.png");
        ASSERT_NE(existing, nullptr);

        ASSERT_NO_FATAL_FAILURE(WriteAssetFile(root / "Two.png", ""));
        ASSERT_NO_FATAL_FAILURE(WriteAssetFile(root / "Two.png.meta", validMeta));
        EXPECT_FALSE(db.Initialize(root));
        EXPECT_EQ(db.FindByPath("One.png"), existing);

        ASSERT_NO_FATAL_FAILURE(WriteAssetFile(root / "Two.png.meta", "type: [\n"));
        EXPECT_FALSE(db.Initialize(root));
        EXPECT_EQ(db.FindByPath("Two.png"), nullptr);

        std::filesystem::remove(root / "Two.png.meta");
        std::filesystem::remove(root / "Two.png");
        std::filesystem::remove(root / "One.png.meta");
        std::filesystem::remove(root / "One.png");
        std::filesystem::remove(root);
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
            "Shaders/MatShaders/Unlit.matsh", new Hestia::Mesh(MakeValidMesh())).IsValid());
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

    TEST(AssetSystem, GeneratesAndCachesBuiltinMeshesOnFirstLoad)
    {
        Hestia::GraphicsSystem graphics;
        Hestia::AssetSystem system;
        ASSERT_TRUE(system.Initialize(&graphics));

        EXPECT_FALSE(Hestia::AssetSystem::Register<Hestia::Mesh>(
            "cUbE", new Hestia::Mesh(MakeValidMesh())).IsValid());

        const auto cube = Hestia::AssetSystem::Load<Hestia::Mesh>("cUbE");
        ASSERT_TRUE(cube.IsValid());
        EXPECT_EQ(Hestia::AssetSystem::Load<Hestia::Mesh>("Cube").m_index, cube.m_index);
        const auto* cubeMesh = Hestia::AssetSystem::Get(cube);
        ASSERT_NE(cubeMesh, nullptr);
        EXPECT_EQ(cubeMesh->m_positions.size(), 24u);
        EXPECT_EQ(cubeMesh->m_indices.size(), 36u);

        const auto plane = Hestia::AssetSystem::Load<Hestia::Mesh>("Plane");
        const auto* planeMesh = Hestia::AssetSystem::Get(plane);
        ASSERT_NE(planeMesh, nullptr);
        EXPECT_EQ(planeMesh->m_positions.size(), 121u);
        EXPECT_EQ(planeMesh->m_indices.size(), 600u);
        EXPECT_FLOAT_EQ(planeMesh->m_positions[1].m_position[0] -
            planeMesh->m_positions[0].m_position[0], 1.0f);

        const auto quad = Hestia::AssetSystem::Load<Hestia::Mesh>("Quad");
        const auto* quadMesh = Hestia::AssetSystem::Get(quad);
        ASSERT_NE(quadMesh, nullptr);
        EXPECT_EQ(quadMesh->m_positions.size(), 4u);
        EXPECT_EQ(quadMesh->m_indices.size(), 6u);

        const auto sphere = Hestia::AssetSystem::Load<Hestia::Mesh>("Sphere");
        const auto* sphereMesh = Hestia::AssetSystem::Get(sphere);
        ASSERT_NE(sphereMesh, nullptr);
        EXPECT_EQ(sphereMesh->m_positions.size(), 153u);
        EXPECT_EQ(sphereMesh->m_indices.size(), 768u);
        for (const auto& vertex : sphereMesh->m_positions)
        {
            const auto& position = vertex.m_position;
            const float radius = std::sqrt(
                position[0] * position[0] + position[1] * position[1] + position[2] * position[2]);
            EXPECT_NEAR(radius, 1.0f, 0.00001f);
        }

        // 名前と永続参照で同じHandleを引き、組み込み間でGUIDが重複しないことを確認する
        std::vector<Hestia::AssetGuid> guids;
        for (const auto handle : { cube, plane, quad, sphere })
        {
            const auto ref = Hestia::AssetSystem::GetReference(handle);
            ASSERT_TRUE(ref.m_guid);
            EXPECT_EQ(ref.m_localId, 0u);
            EXPECT_EQ(std::ranges::find(guids, ref.m_guid), guids.end());
            guids.push_back(ref.m_guid);
            const auto reloaded = Hestia::AssetSystem::Load<Hestia::Mesh>(ref);
            EXPECT_EQ(reloaded.m_index, handle.m_index);
            EXPECT_EQ(reloaded.m_generation, handle.m_generation);
            EXPECT_FALSE(Hestia::AssetSystem::Load<Hestia::Mesh>(Hestia::AssetRef{ ref.m_guid, 1 }).IsValid());
        }
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

            // ディスクに用意したShaderをLoaderへ直接渡す
            EXPECT_FALSE(Hestia::AssetLoader<Hestia::MaterialShader>{}.Load({ Hestia::AssetGuid::Parse("18f898be-8721-4ee2-a30e-7386a2bb9c2b"), 0 }, sourcePath).IsValid());

            // 意図した失敗経路を確認してから、通常のAssetパスから正常なファイルをロード
            std::vector<Hestia::LogEntry> entries;
            m_logStorage.TransferPending(16);
            m_logStorage.Drain(entries, 16);
            EXPECT_TRUE(std::any_of(entries.begin(), entries.end(), [diagnostic](const auto& entry)
            {
                return entry.m_level == Hestia::LogLevel::Error
                    && entry.m_message.find(diagnostic) != std::string::npos;
            })) << diagnostic;

            const auto valid = Hestia::AssetSystem::Load<Hestia::MaterialShader>("Shaders/MatShaders/Unlit.matsh");
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
        const auto handle = Hestia::AssetSystem::Load<Hestia::MaterialShader>("Shaders/MatShaders/Unlit.matsh");
        ASSERT_TRUE(handle.IsValid());
        const auto* shader = Hestia::AssetSystem::Get(handle);
        ASSERT_NE(shader, nullptr);
        EXPECT_EQ(shader->GetName(), "Hestia/Unlit");
        EXPECT_EQ(shader->GetHandle().m_index, handle.m_index);
        EXPECT_EQ(shader->GetHandle().m_generation, handle.m_generation);

        const auto cached = Hestia::AssetSystem::Load<Hestia::MaterialShader>("shaders/matshaders/uNLIT.matsh");
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
        const auto handle = Hestia::AssetLoader<Hestia::MaterialShader>{}.Load({ Hestia::AssetGuid::Parse("18f898be-8721-4ee2-a30e-7386a2bb9c2b"), 0 }, sourcePath);
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

    TEST(AssetAPI, UnboundGameFacadeFailsSafely)
    {
        EXPECT_FALSE(HestiaGame::Assets::Load<Hestia::Mesh>("AnyMesh").IsValid());
        EXPECT_EQ(HestiaGame::Assets::Get<Hestia::Texture>({}), nullptr);
        EXPECT_FALSE(HestiaGame::Assets::Register<Hestia::Mesh>(
            "AnyMesh", new Hestia::Mesh(MakeValidMesh())).IsValid());
    }
}
