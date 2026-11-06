/*=============================================================================

 File   : AssetSystem.cpp
 Desc   : AssetSystem の初期化・名前解決・所有管理を定義する。

------------------------------------------------------------------------------

 Date   : 2026/10/15
 Author : Yokoyama Haruki

=============================================================================*/

#include "pch.h"
#include "Asset/AssetSystem.h"

#include "Graphics/GraphicsSystem.h"
#include "Log/Logger.h"

#include <type_traits>
#include <utility>

namespace Hestia
{
    namespace
    {
        template<class T>
        constexpr bool HasFileLoader =
            std::is_same_v<T, Mesh> ||
            std::is_same_v<T, Texture> ||
            std::is_same_v<T, MaterialShader>;

        template<class T>
        constexpr AssetType GetAssetType()
        {
            if constexpr (std::is_same_v<T, Mesh>)
                return AssetType::Mesh;
            else if constexpr (std::is_same_v<T, Texture>)
                return AssetType::Texture;
            else if constexpr (std::is_same_v<T, MaterialShader>)
                return AssetType::MaterialShader;
            else
                static_assert(HasFileLoader<T>, "Asset type does not have a file loader");
        }
    }

    AssetSystem* AssetSystem::s_instance = nullptr;

    AssetSystem::~AssetSystem()
    {
        Finalize();
    }

    bool AssetSystem::Initialize(GraphicsSystem* graphics)
    {
        if (graphics == nullptr || s_instance != nullptr)
            return false;

        // 初期 Asset の登録情報を一時 Map に用意
        AssetDB::EntryMap entries;
        entries.emplace("TextureTest", AssetInfo{
            AssetType::Texture,
            "../Assets/Textures/texture_test_2048_rgba.png" });
        entries.emplace("Unlit", AssetInfo{
            AssetType::MaterialShader,
            "../Assets/Shaders/MatShaders/Unlit.matsh" });

        AssetDB assetDB;
        if (!assetDB.Initialize(std::move(entries)))
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
    AssetHandle<T> AssetSystem::Load(std::string_view name)
    {
        if (s_instance == nullptr)
            return {};

        return s_instance->LoadImpl<T>(name);
    }

    template<class T>
    T* AssetSystem::Get(AssetHandle<T> handle)
    {
        if (s_instance == nullptr)
            return nullptr;

        return s_instance->GetStorage<T>().Get(handle);
    }

    template<class T>
    AssetHandle<T> AssetSystem::Register(std::string_view name, T* asset)
    {
        std::unique_ptr<T> owner(asset);
        if (s_instance == nullptr)
            return {};

        return s_instance->RegisterImpl<T>(name, std::move(owner), false);
    }

    template<class T>
    AssetHandle<T> AssetSystem::LoadImpl(std::string_view name)
    {
        if (name.empty())
        {
            Logger::Error("Asset name is empty");
            return {};
        }

        AssetStorage<T>& storage = GetStorage<T>();
        const AssetHandle<T> cached = storage.Find(name);
        if (cached.IsValid())
            return cached;

        if constexpr (!HasFileLoader<T>)
        {
            Logger::Error("Material is not registered in the CPU cache");
            return {};
        }
        else
        {
            // ファイルロード対象は DB の型とパスを解決
            const AssetInfo* info = m_assetDB.Find(name);
            if (info == nullptr)
            {
                Logger::Error("Asset name is not registered in AssetDB");
                return {};
            }

            if (info->m_type != GetAssetType<T>())
            {
                Logger::Error("AssetDB type does not match the requested Asset type");
                return {};
            }

            return GetLoader<T>().Load(name, info->m_path);
        }
    }

    template<class T>
    AssetHandle<T> AssetSystem::RegisterImpl(
        std::string_view name, std::unique_ptr<T> asset, bool databaseOrigin)
    {
        if (name.empty() || !asset)
        {
            Logger::Error("Asset registration requires a non-empty name and Asset");
            return {};
        }

        const AssetInfo* info = m_assetDB.Find(name);
        if (databaseOrigin)
        {
            bool matchesEntry = false;
            if constexpr (HasFileLoader<T>)
                matchesEntry = info != nullptr && info->m_type == GetAssetType<T>();

            if (!matchesEntry)
            {
                Logger::Error("Loader registration does not match an AssetDB entry");
                return {};
            }
        }
        else if (info != nullptr)
        {
            Logger::Error("Manual Asset name conflicts with AssetDB");
            return {};
        }

        if constexpr (std::is_same_v<T, Texture>)
        {
            if (!databaseOrigin)
            {
                Logger::Error("Texture registration requires decoded loader pixels");
                return {};
            }
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

        // 型をまたいで Asset 名を一意に保つ
        const bool alreadyCached = std::apply([name](const auto&... storage)
        {
            return (storage.Contains(name) || ...);
        }, m_storages);

        if (alreadyCached)
        {
            Logger::Error("Asset name is already cached");
            return {};
        }

        AssetStorage<T>& storage = GetStorage<T>();
        const AssetHandle<T> handle = storage.Register(name, std::move(asset));
        if (!handle.IsValid())
        {
            Logger::Error("Asset storage rejected registration");
            return {};
        }

        if constexpr (std::is_same_v<T, Mesh>)
        {
            Mesh* registeredMesh = storage.Get(handle);
            if (registeredMesh != nullptr)
                OnRegistered(handle, *registeredMesh);
        }

        return handle;
    }

    template<class T>
    AssetHandle<T> AssetSystem::RegisterFromLoader(
        std::string_view name, std::unique_ptr<T> asset)
    {
        if (s_instance == nullptr)
            return {};

        return s_instance->RegisterImpl<T>(name, std::move(asset), true);
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

    // 明示的なテンプレートインスタンス化

    // Load
    template AssetHandle<Mesh> AssetSystem::Load<Mesh>(std::string_view);
    template AssetHandle<Texture> AssetSystem::Load<Texture>(std::string_view);
    template AssetHandle<MaterialShader> AssetSystem::Load<MaterialShader>(std::string_view);
    template AssetHandle<Material> AssetSystem::Load<Material>(std::string_view);

    // Get
    template Mesh* AssetSystem::Get<Mesh>(AssetHandle<Mesh>);
    template Texture* AssetSystem::Get<Texture>(AssetHandle<Texture>);
    template MaterialShader* AssetSystem::Get<MaterialShader>(AssetHandle<MaterialShader>);
    template Material* AssetSystem::Get<Material>(AssetHandle<Material>);

    // Register
    template AssetHandle<Mesh> AssetSystem::Register<Mesh>(std::string_view, Mesh*);
    template AssetHandle<Texture> AssetSystem::Register<Texture>(std::string_view, Texture*);
    template AssetHandle<MaterialShader> AssetSystem::Register<MaterialShader>(
        std::string_view, MaterialShader*);
    template AssetHandle<Material> AssetSystem::Register<Material>(std::string_view, Material*);

    template AssetHandle<Mesh> AssetSystem::LoadImpl<Mesh>(std::string_view);
    template AssetHandle<Texture> AssetSystem::LoadImpl<Texture>(std::string_view);
    template AssetHandle<Mesh> AssetSystem::RegisterImpl(
        std::string_view, std::unique_ptr<Mesh>, bool);
    template AssetHandle<Texture> AssetSystem::RegisterImpl(
        std::string_view, std::unique_ptr<Texture>, bool);
    template AssetHandle<Mesh> AssetSystem::RegisterFromLoader(
        std::string_view, std::unique_ptr<Mesh>);
    template AssetHandle<Texture> AssetSystem::RegisterFromLoader(
        std::string_view, std::unique_ptr<Texture>);
    template AssetHandle<MaterialShader> AssetSystem::RegisterFromLoader(
        std::string_view, std::unique_ptr<MaterialShader>);
    template AssetLoader<Mesh>& AssetSystem::GetLoader<Mesh>();
    template AssetLoader<Texture>& AssetSystem::GetLoader<Texture>();
    template AssetStorage<Mesh>& AssetSystem::GetStorage<Mesh>();
    template AssetStorage<Texture>& AssetSystem::GetStorage<Texture>();
}
