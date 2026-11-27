/*=============================================================================

 File   : EntitySerializer.cpp
 Desc   : EntityのスナップショットからRapidYAMLのTreeを作成する。

------------------------------------------------------------------------------

 Date   : 2026/11/19
 Author : Yokoyama Haruki

=============================================================================*/

#include "pch.h"
#include "Serialize/Serializer.h"

#include <string>

#include "ECS/ComponentRegistry.h"
#include "ECS/ECSSystem.h"
#include "Log/Logger.h"
#include "Serialize/SerializedEntity.h"
#include "Serialize/YamlFile.h"

namespace Hestia
{
    namespace
    {
        ryml::NodeRef BeginPrefab(ryml::Tree& tree, std::string_view prefabName)
        {
            if (prefabName.empty())
            {
                Logger::Error("Entity YAML requires a Prefab name");
                return {};
            }

            auto root = tree.rootref();
            root |= ryml::MAP;
            root["prefabName"].save(ryml::csubstr(prefabName.data(), prefabName.size()), ryml::VAL_DQUO);
            auto entities = root["entities"];
            entities |= ryml::SEQ;
            return entities;
        }

        bool WriteComponents(ryml::NodeRef components, const SerializedEntity& entity,
            const ComponentRegistry& registry)
        {
            // フィールドはMapの反復順に依存せず、Registryの宣言順で出力する
            components |= ryml::MAP;
            for (const auto& component : entity.m_components)
            {
                const auto* info = registry.FindInfo(component.m_typeName);
                if (info == nullptr)
                {
                    Logger::Error("Entity YAML contains an unregistered Component: " + component.m_typeName);
                    return false;
                }

                const ryml::csubstr typeName(component.m_typeName.data(), component.m_typeName.size());
                if (components.has_child(typeName))
                {
                    Logger::Error("Entity YAML contains a duplicate Component: " + component.m_typeName);
                    return false;
                }
                auto fields = components.append_child();
                fields.save_key(typeName, ryml::KEY_DQUO);
                fields |= ryml::MAP;

                size_t writtenFields = 0;
                for (const auto& field : info->serializeFields)
                {
                    const auto found = component.m_fields.find(std::string(field.name));
                    if (found == component.m_fields.end())
                        continue;

                    auto fieldNode = fields.append_child();
                    fieldNode.save_key(ryml::csubstr(field.name.data(), field.name.size()), ryml::KEY_DQUO);
                    WriteYamlValue(fieldNode, found->second);
                    ++writtenFields;
                }
                if (writtenFields != component.m_fields.size())
                {
                    Logger::Error("Entity YAML fields do not match the Registry: " + component.m_typeName);
                    return false;
                }
            }
            return true;
        }
        bool WriteEntity(ryml::NodeRef entities, const SerializedEntity& entity,
            const ComponentRegistry& registry, uint64_t id, uint64_t parentId, bool scene)
        {
            // 親子関係にはファイル内のIDを使う
            auto entry = entities.append_child();
            entry |= ryml::MAP;
            entry["id"].save(id);
            if (parentId != 0)
                entry["parent"].save(parentId);

            if (scene)
            {
                entry["active"].set_val(entity.m_active ? "true" : "false");
                if (!entity.m_scripts.empty())
                {
                    auto scripts = entry["scripts"];
                    scripts |= ryml::SEQ;
                    for (const auto& ref : entity.m_scripts)
                    {
                        if (!ref.m_guid || ref.m_localId != 0)
                        {
                            Logger::Error("Scene YAML contains an invalid Script reference");
                            return false;
                        }
                        WriteYamlValue(scripts.append_child(), SerializedValue{ ref });
                    }
                }
            }
            return WriteComponents(entry["components"], entity, registry);
        }

        bool WriteHierarchy(ryml::NodeRef entities, std::span<const SerializedEntity> roots, bool scene)
        {
            const auto* registry = ECSSystem::GetRegistry();
            if (!registry)
            {
                Logger::Error("Entity YAML requires an initialized ComponentRegistry");
                return false;
            }

            // ルートをまたいで使うファイル内IDを、深さ優先で割り当てる
            uint64_t nextId = 1;
            const auto append = [&](this const auto& self, const SerializedEntity& entity, uint64_t parentId) -> bool
            {
                const uint64_t id = nextId++;
                if (!WriteEntity(entities, entity, *registry, id, parentId, scene))
                    return false;

                for (const auto& child : entity.m_children)
                {
                    if (!self(child, id))
                        return false;
                }
                return true;
            };

            for (const auto& root : roots)
            {
                if (!append(root, 0))
                    return false;
            }
            return true;
        }

    }

    std::optional<ryml::Tree> Serializer<Prefab>::Serialize(const SerializedEntity& root, std::string_view prefabName)
    {
        ryml::Tree tree;
        auto entities = BeginPrefab(tree, prefabName);
        if (!entities.readable() || !WriteHierarchy(entities, { &root, 1 }, false))
            return std::nullopt;
        return tree;
    }

    std::optional<ryml::Tree> Serializer<std::span<const SerializedEntity>>::Serialize(
        std::span<const SerializedEntity> roots)
    {
        // Sceneのバージョンを付けてから、Prefabと同じEntity形式で階層を出力する
        ryml::Tree tree;
        auto document = tree.rootref();
        document |= ryml::MAP;
        document["version"].save(1);
        auto entities = document["entities"];
        entities |= ryml::SEQ;
        if (roots.empty())
            entities |= ryml::FLOW_SL;
        if (!WriteHierarchy(entities, roots, true))
            return std::nullopt;
        return tree;
    }
}
