/*=============================================================================

 File   : AssetSystem.cpp
 Desc   : AssetSystem の初期化・名前解決・所有管理を定義する。

------------------------------------------------------------------------------

 Date   : 2026/10/15
 Author : Yokoyama Haruki

=============================================================================*/

#include "pch.h"
#include "Common/Include/StringUtils.h"
#include "Asset/AssetSystem.h"

#include "Graphics/GraphicsSystem.h"
#include "Log/Logger.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <numbers>
#include <string>
#include <type_traits>
#include <utility>

namespace Hestia
{
    namespace
    {
        bool IsPrimitiveMeshName(std::string_view name)
        {
            const std::string normalizedName = ToLowerAscii(name);
            return normalizedName == "cube" || normalizedName == "plane" ||
                normalizedName == "quad" || normalizedName == "sphere";
        }

        template<class T>
        constexpr AssetType GetAssetType()
        {
            if constexpr (std::is_same_v<T, Texture>)
                return AssetType::Texture;
            else
                return AssetType::MaterialShader;
        }

        std::unique_ptr<Mesh> CreateCubeMesh()
        {
            auto mesh = std::make_unique<Mesh>();
            constexpr float halfSize = 0.5f;
            constexpr std::array<std::array<std::array<float, 3>, 4>, 6> faces = {{
                {{{-halfSize, -halfSize, -halfSize}, {-halfSize, halfSize, -halfSize},
                  {halfSize, halfSize, -halfSize}, {halfSize, -halfSize, -halfSize}}},
                {{{-halfSize, -halfSize, halfSize}, {halfSize, -halfSize, halfSize},
                  {halfSize, halfSize, halfSize}, {-halfSize, halfSize, halfSize}}},
                {{{-halfSize, -halfSize, halfSize}, {-halfSize, halfSize, halfSize},
                  {-halfSize, halfSize, -halfSize}, {-halfSize, -halfSize, -halfSize}}},
                {{{halfSize, -halfSize, -halfSize}, {halfSize, halfSize, -halfSize},
                  {halfSize, halfSize, halfSize}, {halfSize, -halfSize, halfSize}}},
                {{{-halfSize, -halfSize, halfSize}, {-halfSize, -halfSize, -halfSize},
                  {halfSize, -halfSize, -halfSize}, {halfSize, -halfSize, halfSize}}},
                {{{-halfSize, halfSize, -halfSize}, {-halfSize, halfSize, halfSize},
                  {halfSize, halfSize, halfSize}, {halfSize, halfSize, -halfSize}}}
            }};
            constexpr std::array<std::array<float, 3>, 6> normals = {{
                {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f, 1.0f},
                {-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f},
                {0.0f, -1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}
            }};
            constexpr std::array<std::array<float, 3>, 6> tangents = {{
                {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f},
                {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f, 1.0f},
                {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}
            }};
            constexpr std::array<std::array<float, 2>, 4> uvs = {{
                {0.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f}
            }};

            // 面ごとに頂点を分け、法線と UV の継ぎ目を保つ
            for (std::size_t face = 0; face < faces.size(); ++face)
            {
                const uint32_t firstVertex = static_cast<uint32_t>(mesh->m_positions.size());
                for (std::size_t vertex = 0; vertex < 4; ++vertex)
                {
                    mesh->m_positions.push_back({ faces[face][vertex] });
                    mesh->m_normalTangents.push_back({ normals[face], {
                        tangents[face][0], tangents[face][1], tangents[face][2], 1.0f
                    } });
                    mesh->m_primaryTexCoords.push_back({ uvs[vertex] });
                }

                mesh->m_indices.insert(mesh->m_indices.end(), {
                    firstVertex, firstVertex + 1, firstVertex + 2,
                    firstVertex, firstVertex + 2, firstVertex + 3
                });
            }

            return mesh;
        }

        std::unique_ptr<Mesh> CreatePlaneMesh()
        {
            auto mesh = std::make_unique<Mesh>();
            constexpr uint32_t divisions = 10;
            constexpr uint32_t verticesPerSide = divisions + 1;
            constexpr float spacing = 1.0f;
            constexpr float halfSize = divisions * spacing * 0.5f;

            // 1 m 間隔で頂点 Stream を作成
            for (uint32_t z = 0; z < verticesPerSide; ++z)
            {
                for (uint32_t x = 0; x < verticesPerSide; ++x)
                {
                    mesh->m_positions.push_back({ {
                        -halfSize + x * spacing, 0.0f, -halfSize + z * spacing
                    } });
                    mesh->m_normalTangents.push_back({
                        { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f }
                    });
                    mesh->m_primaryTexCoords.push_back({ {
                        static_cast<float>(x), static_cast<float>(z)
                    } });
                }
            }

            // 隣接頂点を二つの三角形で接続
            for (uint32_t z = 0; z < divisions; ++z)
            {
                for (uint32_t x = 0; x < divisions; ++x)
                {
                    const uint32_t topLeft = z * verticesPerSide + x;
                    const uint32_t bottomLeft = topLeft + verticesPerSide;
                    const uint32_t topRight = topLeft + 1;
                    const uint32_t bottomRight = bottomLeft + 1;
                    mesh->m_indices.insert(mesh->m_indices.end(), {
                        topLeft, bottomLeft, topRight,
                        topRight, bottomLeft, bottomRight
                    });
                }
            }

            return mesh;
        }

        std::unique_ptr<Mesh> CreateQuadMesh()
        {
            auto mesh = std::make_unique<Mesh>();
            mesh->m_positions = {
                { { -0.5f, 0.5f, 0.0f } }, { { 0.5f, 0.5f, 0.0f } },
                { { -0.5f, -0.5f, 0.0f } }, { { 0.5f, -0.5f, 0.0f } }
            };
            mesh->m_normalTangents.resize(4, {
                { 0.0f, 0.0f, -1.0f }, { 1.0f, 0.0f, 0.0f, 1.0f }
            });
            mesh->m_primaryTexCoords = {
                { { 0.0f, 0.0f } }, { { 1.0f, 0.0f } },
                { { 0.0f, 1.0f } }, { { 1.0f, 1.0f } }
            };
            mesh->m_indices = { 0, 1, 2, 2, 1, 3 };
            return mesh;
        }

        std::unique_ptr<Mesh> CreateSphereMesh()
        {
            auto mesh = std::make_unique<Mesh>();
            constexpr uint32_t longitudeSegments = 16;
            constexpr uint32_t latitudeSegments = 8;
            constexpr float pi = std::numbers::pi_v<float>;

            // 緯度・経度から位置、法線、接線、UV を作成
            for (uint32_t latitude = 0; latitude <= latitudeSegments; ++latitude)
            {
                const float v = static_cast<float>(latitude) / latitudeSegments;
                const float angle = -pi * 0.5f + pi * v;
                const float ringRadius = std::cos(angle);
                const float y = std::sin(angle);

                for (uint32_t longitude = 0; longitude <= longitudeSegments; ++longitude)
                {
                    const float u = static_cast<float>(longitude) / longitudeSegments;
                    const float theta = 2.0f * pi * u;
                    const std::array<float, 3> normal = {
                        ringRadius * std::cos(theta), y, ringRadius * std::sin(theta)
                    };
                    mesh->m_positions.push_back({ normal });
                    mesh->m_normalTangents.push_back({
                        normal, { -std::sin(theta), 0.0f, std::cos(theta), 1.0f }
                    });
                    mesh->m_primaryTexCoords.push_back({ { u, 1.0f - v } });
                }
            }

            const uint32_t verticesPerRing = longitudeSegments + 1;
            // 隣り合う緯度・経度の頂点を三角形で接続
            for (uint32_t latitude = 0; latitude < latitudeSegments; ++latitude)
            {
                for (uint32_t longitude = 0; longitude < longitudeSegments; ++longitude)
                {
                    const uint32_t lowerLeft = latitude * verticesPerRing + longitude;
                    const uint32_t upperLeft = lowerLeft + verticesPerRing;
                    const uint32_t lowerRight = lowerLeft + 1;
                    const uint32_t upperRight = upperLeft + 1;
                    mesh->m_indices.insert(mesh->m_indices.end(), {
                        lowerLeft, upperLeft, lowerRight,
                        lowerRight, upperLeft, upperRight
                    });
                }
            }

            return mesh;
        }

        std::unique_ptr<Mesh> CreatePrimitiveMesh(std::string_view name)
        {
            // Asset 名と同じ規則で ASCII 英字を小文字化
            const std::string normalizedName = ToLowerAscii(name);

            // 予約名に対応する Mesh を生成
            if (normalizedName == "cube")
                return CreateCubeMesh();
            if (normalizedName == "plane")
                return CreatePlaneMesh();
            if (normalizedName == "quad")
                return CreateQuadMesh();
            if (normalizedName == "sphere")
                return CreateSphereMesh();
            return {};
        }
    }

    AssetSystem* AssetSystem::s_instance = nullptr;

    AssetSystem::~AssetSystem()
    {
        Finalize();
    }

    bool AssetSystem::Initialize(GraphicsSystem* graphics, const std::filesystem::path& assetsRoot)
    {
        if (graphics == nullptr || s_instance != nullptr)
            return false;

        AssetDB assetDB;
        if (!assetDB.Initialize(assetsRoot))
            return false;

        // Loader を初期化
        if (!GetLoader<Texture>().Initialize(graphics))
            return false;

        m_assetDB = std::move(assetDB);
        m_graphics = graphics;
        s_instance = this;
        return true;
    }

    void AssetSystem::Finalize()
    {
        if (s_instance != this)
            return;

        // 借用元の Material を先に破棄して Shader の逆索引を解除
        GetStorage<Material>() = {};

        // CPU Asset と検索情報を解放
        m_storages = {};
        m_assetDB = {};
        m_loaders = {};
        m_graphics = nullptr;
        s_instance = nullptr;
    }

    template<class T>
    AssetHandle<T> AssetSystem::Load(std::string_view path)
    {
        auto* system = s_instance;
        if (system == nullptr)
            return {};

        // 手動登録と組み込みMeshは従来の名前で取得する
        auto& storage = system->GetStorage<T>();
        if (const auto cached = storage.Find(path); cached.IsValid())
            return cached;

        if constexpr (std::is_same_v<T, Mesh>)
        {
            if (auto primitive = CreatePrimitiveMesh(path))
                return system->RegisterNamed<Mesh>(path, std::move(primitive));
        }

        const auto* info = system->m_assetDB.FindByPath(path);
        if (info == nullptr)
        {
            Logger::Error("Asset path is not registered: " + std::string(path));
            return {};
        }

        return system->LoadFile<T>({ info->m_guid, 0 }, info);
    }

    template<class T>
    AssetHandle<T> AssetSystem::Load(const AssetRef& ref)
    {
        auto* system = s_instance;
        return system != nullptr ? system->LoadFile<T>(ref, nullptr) : AssetHandle<T>{};
    }

    template<class T>
    AssetHandle<T> AssetSystem::LoadFile(const AssetRef& ref, const AssetInfo* info)
    {
        auto& storage = GetStorage<T>();
        if (const auto cached = storage.Find(ref); cached.IsValid())
            return cached;

        // PathとGUIDを共通の登録経路へ渡し、モデルのAssetをまとめて生成する
        if (info == nullptr)
            info = m_assetDB.FindByGuid(ref.m_guid);
        if (info == nullptr)
        {
            Logger::Error("Asset GUID is not registered: " + ref.m_guid.ToString());
            return {};
        }

        if constexpr (std::is_same_v<T, Mesh> || std::is_same_v<T, Material> || std::is_same_v<T, Prefab>)
        {
            if constexpr (std::is_same_v<T, Prefab>)
            {
                if (info->m_type == AssetType::Prefab && ref.m_localId == 0)
                    return GetLoader<Prefab>().LoadYaml(*info, m_assetDB);
            }

            constexpr bool isSubAsset = !std::is_same_v<T, Prefab>;
            if (info->m_type == AssetType::Model && isSubAsset == (ref.m_localId != 0))
            {
                const AssetRef primary{ ref.m_guid, 0 };
                if (!GetStorage<Prefab>().Find(primary).IsValid() &&
                    !GetLoader<Prefab>().Load(*info, m_assetDB).IsValid())
                    return {};

                if (const auto loaded = storage.Find(ref); loaded.IsValid())
                    return loaded;

                Logger::Error(std::format("Missing SubAsset {} in '{}'", ref.m_localId, info->m_path));
                return {};
            }
        }
        else if constexpr (std::is_same_v<T, Texture> || std::is_same_v<T, MaterialShader>)
        {
            if (ref.m_localId == 0 && info->m_type == GetAssetType<T>())
                return GetLoader<T>().Load(ref, m_assetDB.SourcePath(*info));
        }

        Logger::Error("Asset type or local ID does not match the request: " + info->m_path);
        return {};
    }

    template<class T>
    T* AssetSystem::Get(AssetHandle<T> handle)
    {
        return s_instance != nullptr ? s_instance->GetStorage<T>().Get(handle) : nullptr;
    }

    template<class T>
    AssetRef AssetSystem::GetReference(AssetHandle<T> handle)
    {
        return s_instance != nullptr ? s_instance->GetStorage<T>().GetReference(handle) : AssetRef{};
    }

    template<class T>
    AssetHandle<T> AssetSystem::Register(std::string_view name, T* asset)
    {
        std::unique_ptr<T> owner(asset);
        if (IsPrimitiveMeshName(name))
        {
            Logger::Error("Primitive mesh name is reserved");
            return {};
        }

        return s_instance != nullptr ? s_instance->RegisterNamed<T>(name, std::move(owner)) : AssetHandle<T>{};
    }

    template<class T>
    AssetHandle<T> AssetSystem::RegisterNamed(std::string_view name, std::unique_ptr<T> asset)
    {
        if (name.empty() || !asset)
        {
            Logger::Error("Asset registration requires a non-empty name and Asset");
            return {};
        }

        if constexpr (std::is_same_v<T, Texture>)
        {
            Logger::Error("Texture registration requires decoded loader pixels");
            return {};
        }
        else if constexpr (std::is_same_v<T, Mesh>)
        {
            if (!asset->IsValid())
            {
                Logger::Error("Mesh registration rejected invalid CPU data");
                return {};
            }
        }
        else if constexpr (std::is_same_v<T, Material>)
        {
            if (asset->GetShader() == nullptr)
            {
                Logger::Error("Material registration requires a shader");
                return {};
            }
        }

        // 名前登録の衝突を型別Cacheとファイルの相対パスに対して確認する
        const bool alreadyCached = std::apply([name](const auto&... storage)
        {
            return (storage.Contains(name) || ...);
        }, m_storages);
        if (alreadyCached || m_assetDB.FindByPath(name) != nullptr)
        {
            Logger::Error("Asset name already exists");
            return {};
        }

        auto& storage = GetStorage<T>();
        T& registered = *asset;
        const auto handle = storage.Register(name, std::move(asset));
        if (!handle.IsValid())
        {
            Logger::Error("Asset storage rejected registration");
            return {};
        }

        if constexpr (std::is_same_v<T, Mesh>)
            OnRegistered(handle, registered);
        return handle;
    }

    template<class T>
    AssetHandle<T> AssetSystem::RegisterFromLoader(const AssetRef& ref, std::unique_ptr<T> asset)
    {
        // Loader側でファイルと変換結果の検証が済んだAssetを登録する
        T& registered = *asset;
        const auto handle = s_instance->GetStorage<T>().Register(ref, std::move(asset));
        if constexpr (std::is_same_v<T, Mesh>)
        {
            if (handle.IsValid())
                s_instance->OnRegistered(handle, registered);
        }
        return handle;
    }

    template<class T>
    AssetLoader<T>& AssetSystem::GetLoader()
    {
        return std::get<AssetLoader<T>>(m_loaders);
    }

    template<class T>
    AssetStorage<T>& AssetSystem::GetStorage()
    {
        return std::get<AssetStorage<T>>(m_storages);
    }

    void AssetSystem::OnRegistered(AssetHandle<Mesh> handle, Mesh& mesh)
    {
        if (m_graphics == nullptr || !m_graphics->UploadMesh(handle, mesh))
            Logger::Error("Mesh was cached on CPU but GPU upload failed");
    }

    // .cppで定義したAsset APIを各型で使用できるようにする
    template AssetHandle<Mesh> AssetSystem::Load<Mesh>(std::string_view);
    template AssetHandle<Texture> AssetSystem::Load<Texture>(std::string_view);
    template AssetHandle<MaterialShader> AssetSystem::Load<MaterialShader>(std::string_view);
    template AssetHandle<Material> AssetSystem::Load<Material>(std::string_view);
    template AssetHandle<Prefab> AssetSystem::Load<Prefab>(std::string_view);

    template AssetHandle<Mesh> AssetSystem::Load<Mesh>(const AssetRef&);
    template AssetHandle<Texture> AssetSystem::Load<Texture>(const AssetRef&);
    template AssetHandle<MaterialShader> AssetSystem::Load<MaterialShader>(const AssetRef&);
    template AssetHandle<Material> AssetSystem::Load<Material>(const AssetRef&);
    template AssetHandle<Prefab> AssetSystem::Load<Prefab>(const AssetRef&);

    template Mesh* AssetSystem::Get<Mesh>(AssetHandle<Mesh>);
    template Texture* AssetSystem::Get<Texture>(AssetHandle<Texture>);
    template MaterialShader* AssetSystem::Get<MaterialShader>(AssetHandle<MaterialShader>);
    template Material* AssetSystem::Get<Material>(AssetHandle<Material>);
    template Prefab* AssetSystem::Get<Prefab>(AssetHandle<Prefab>);

    template AssetRef AssetSystem::GetReference<Mesh>(AssetHandle<Mesh>);
    template AssetRef AssetSystem::GetReference<Texture>(AssetHandle<Texture>);
    template AssetRef AssetSystem::GetReference<MaterialShader>(AssetHandle<MaterialShader>);
    template AssetRef AssetSystem::GetReference<Material>(AssetHandle<Material>);
    template AssetRef AssetSystem::GetReference<Prefab>(AssetHandle<Prefab>);

    template AssetHandle<Mesh> AssetSystem::Register<Mesh>(std::string_view, Mesh*);
    template AssetHandle<Texture> AssetSystem::Register<Texture>(std::string_view, Texture*);
    template AssetHandle<MaterialShader> AssetSystem::Register<MaterialShader>(std::string_view, MaterialShader*);
    template AssetHandle<Material> AssetSystem::Register<Material>(std::string_view, Material*);

    template AssetHandle<Mesh> AssetSystem::RegisterFromLoader(const AssetRef&, std::unique_ptr<Mesh>);
    template AssetHandle<Texture> AssetSystem::RegisterFromLoader(const AssetRef&, std::unique_ptr<Texture>);
    template AssetHandle<MaterialShader> AssetSystem::RegisterFromLoader(
        const AssetRef&, std::unique_ptr<MaterialShader>);
    template AssetHandle<Material> AssetSystem::RegisterFromLoader(const AssetRef&, std::unique_ptr<Material>);
    template AssetHandle<Prefab> AssetSystem::RegisterFromLoader(const AssetRef&, std::unique_ptr<Prefab>);
}
