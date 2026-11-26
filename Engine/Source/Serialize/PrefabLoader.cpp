/*=============================================================================

 File   : PrefabLoader.cpp
 Desc   : Prefab YAMLをEntityのスナップショットへ変換する。

------------------------------------------------------------------------------

 Date   : 2026/11/20
 Author : Yokoyama Haruki

=============================================================================*/

#include "pch.h"
#include "Common/Include/StringUtils.h"
#include "Asset/AssetSystem.h"

#include <algorithm>
#include <charconv>

#include "ECS/ECSSystem.h"
#include "Serialize/EntityYamlReader.h"
#include "Log/Logger.h"
#include "Serialize/YamlFile.h"

namespace Hestia
{
    namespace
    {
        std::string_view View(ryml::csubstr text)
        {
            return { text.str, text.len };
        }

        template<class T>
        bool ReadNumber(std::string_view text, T& value)
        {
            if (text.empty())
                return false;
            const auto end = text.data() + text.size();
            const auto result = std::from_chars(text.data(), end, value);
            return result.ec == std::errc{} && result.ptr == end;
        }

        template<class T>
        bool ReadNumber(ryml::ConstNodeRef node, T& value)
        {
            if (!node.readable() || !node.has_val())
                return false;

            return ReadNumber(View(node.val()), value);
        }

        bool ReadSceneFields(ryml::ConstNodeRef row, SerializedEntity& entity)
        {
            const auto active = row.find_child("active");
            if (!active.readable())
                return false;

            const auto value = ReadYamlValue(active);
            const auto* flag = value ? std::get_if<bool>(&value->m_value) : nullptr;
            if (!flag)
                return false;
            entity.m_active = *flag;

            // Script参照は型の主Assetだけを許可し、同じ型を二度付けない
            const auto scripts = row.find_child("scripts");
            if (!scripts.readable())
                return true;
            if (!scripts.is_seq())
                return false;

            for (const auto node : scripts.children())
            {
                const auto script = ReadYamlValue(node);
                const auto* ref = script ? std::get_if<AssetRef>(&script->m_value) : nullptr;
                if (!ref || !ref->m_guid || ref->m_localId != 0 ||
                    std::ranges::find(entity.m_scripts, *ref) != entity.m_scripts.end())
                    return false;

                entity.m_scripts.push_back(*ref);
            }
            return true;
        }
    }

    static std::optional<std::vector<SerializedEntity>> ReadEntities(const std::filesystem::path& path, std::string* prefabName)
    {
        const auto fail = [&](std::string_view message) -> std::optional<std::vector<SerializedEntity>>
        {
            Logger::Error("Could not load Entity YAML '" + PathToUtf8(path) + "': " + std::string(message));
            return {};
        };

        const auto* registry = ECSSystem::GetRegistry();
        if (!registry)
            return fail("ComponentRegistry is unavailable");

        std::string error;
        const auto tree = YamlReader::Read(path, error);
        if (!tree)
            return fail(error);
        const auto root = tree->crootref();

        if (!root.is_map())
            return fail("expected a YAML map");

        const auto entities = root.find_child("entities");
        if (!entities.readable() || !entities.is_seq())
            return fail("expected entities");

        if (prefabName)
        {
            const auto name = root.find_child("prefabName");
            if (!name.readable() || !name.has_val() || name.val_is_null() || name.val().empty())
                return fail("expected prefabName");

            *prefabName = View(name.val());
        }
        else
        {
            uint32_t version = 0;
            if (!ReadNumber(root.find_child("version"), version) || version != 1)
                return fail("unsupported Scene version");
        }

        // ファイル内IDで各行を索引化し、値をRegistryの宣言に照合する
        std::unordered_map<uint64_t, SerializedEntity> byId;
        std::unordered_map<uint64_t, std::vector<uint64_t>> children;
        std::vector<uint64_t> rootIds;
        for (const auto row : entities.children())
        {
            uint64_t id = 0;
            if (!row.is_map() || !ReadNumber(row.find_child("id"), id) || id == 0)
                return fail("expected a positive Entity id");
            const auto [entry, inserted] = byId.try_emplace(id);
            if (!inserted)
                return fail("duplicate Entity id");

            const auto parentNode = row.find_child("parent");
            if (parentNode.readable())
            {
                uint64_t parent = 0;
                if (!ReadNumber(parentNode, parent) || parent == 0)
                    return fail("expected a positive parent id");
                children[parent].push_back(id);
            }
            else
                rootIds.push_back(id);

            if (!prefabName && !ReadSceneFields(row, entry->second))
                return fail("invalid active value or Script reference");

            const auto components = row.find_child("components");
            if (!components.readable() || !components.is_map())
                return fail("expected a Component map");
            auto& destination = entry->second.m_components;
            for (const auto component : components.children())
            {
                const auto typeName = View(component.key());
                const auto* schema = registry->FindInfo(typeName);
                if (!schema || !component.is_map() ||
                    std::ranges::find(destination, typeName, &SerializedComponent::m_typeName) != destination.end())
                    return fail("invalid or duplicate Component: " + std::string(typeName));

                auto& fields = destination.emplace_back(schema->name).m_fields;
                for (const auto fieldNode : component.children())
                {
                    const auto key = View(fieldNode.key());
                    const auto field = std::ranges::find(schema->serializeFields, key, &SerializeFieldDescriptor::name);
                    auto value = ReadYamlValue(fieldNode);
                    if (field == schema->serializeFields.end() || !value || !field->accepts(*value) ||
                        !fields.emplace(key, std::move(*value)).second)
                        return fail("invalid or duplicate field: " + schema->name + "." + std::string(key));
                }
            }
        }
        if (prefabName && rootIds.size() != 1)
            return fail("expected one Prefab root");

        // ルートにつながる行だけを移し、孤立や循環で残った行を拒否する
        const auto build = [&](this const auto& self, uint64_t id) -> SerializedEntity
        {
            auto entity = std::move(byId.extract(id).mapped());
            if (const auto found = children.find(id); found != children.end())
            {
                entity.m_children.reserve(found->second.size());
                for (const auto child : found->second)
                    entity.m_children.push_back(self(child));
            }
            return entity;
        };
        std::vector<SerializedEntity> roots;
        roots.reserve(rootIds.size());
        for (const auto id : rootIds)
            roots.push_back(build(id));
        if (!byId.empty())
            return fail("Entity hierarchy is disconnected from its roots");

        return roots;
    }

    AssetHandle<Prefab> AssetLoader<Prefab>::LoadYaml(const AssetInfo& info, const AssetDB& database)
    {
        std::string name;
        auto roots = ReadEntities(PathFromUtf8(database.SourcePath(info)), &name);
        if (!roots)
            return {};

        auto prefab = std::make_unique<Prefab>();
        prefab->m_name = std::move(name);
        prefab->m_root = std::move(roots->front());
        return AssetSystem::RegisterFromLoader(AssetRef{ info.m_guid, 0 }, std::move(prefab));
    }

    std::optional<std::vector<SerializedEntity>> EntityYamlReader::LoadScene(const std::filesystem::path& path)
    {
        return ReadEntities(path, nullptr);
    }
}
