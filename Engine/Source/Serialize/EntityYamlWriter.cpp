/*=============================================================================

 File   : EntityYamlWriter.cpp
 Desc   : EntityのスナップショットからRapidYAMLのTreeを作成する。

------------------------------------------------------------------------------

 Date   : 2026/11/19
 Author : Yokoyama Haruki

=============================================================================*/

#include "pch.h"
#include "Serialize/EntityYamlWriter.h"

#include <charconv>
#include <cmath>
#include <fstream>
#include <string>
#include <type_traits>

#include "ECS/ComponentRegistry.h"
#include "ECS/ECSSystem.h"
#include "Log/Logger.h"
#include "RapidYaml/rapidyaml.hpp"
#include "Serialize/SerializedEntity.h"

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

        void WriteValue(ryml::NodeRef node, const SerializedValue& value)
        {
            // 配列は要素を再帰的に展開し、文字列は数値や真偽値と区別する
            std::visit([node](const auto& item) mutable
            {
                using Value = std::decay_t<decltype(item)>;
                if constexpr (std::is_same_v<Value, SerializedValue::Array>)
                {
                    node |= ryml::SEQ | ryml::FLOW_SL;
                    for (const auto& element : item)
                        WriteValue(node.append_child(), element);
                }
                else if constexpr (std::is_same_v<Value, std::string>)
                    node.save(ryml::csubstr(item.data(), item.size()), ryml::VAL_DQUO);
                else if constexpr (std::is_same_v<Value, bool>)
                    node.set_val(item ? "true" : "false");
                else if constexpr (std::is_same_v<Value, AssetRef>)
                {
                    if (!item.m_guid)
                    {
                        node.set_val("null");
                        return;
                    }

                    node |= ryml::MAP | ryml::FLOW_SL;
                    node["guid"].save(ryml::to_csubstr(item.m_guid.ToString()));
                    node["localId"].save(item.m_localId);
                }
                else if constexpr (std::is_same_v<Value, double>)
                {
                    // 指数表記で精度を保ち、整数に見える値も浮動小数として保存する
                    if (std::isnan(item))
                        node.set_val(".nan");
                    else if (std::isinf(item))
                        node.set_val(std::signbit(item) ? "-.inf" : ".inf");
                    else
                    {
                        char buffer[32];
                        const auto result = std::to_chars(buffer, buffer + sizeof(buffer), item,
                            std::chars_format::scientific);
                        node.save(ryml::csubstr(buffer, static_cast<size_t>(result.ptr - buffer)));
                    }
                }
                else
                    node.save(item);
            }, value.m_value);
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
                    WriteValue(fieldNode, found->second);
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
                        WriteValue(scripts.append_child(), SerializedValue{ ref });
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

        bool SaveTree(const ryml::Tree& tree, const std::filesystem::path& path)
        {
            // Treeが完成してから出力先を開き、close時の書き込み失敗も確認する
            const auto yaml = ryml::emitrs_yaml<std::string>(tree);
            std::ofstream file(path, std::ios::binary | std::ios::trunc);
            if (!file)
            {
                Logger::Error("Could not open Entity YAML for writing: " + path.string());
                return false;
            }

            file.write(yaml.data(), static_cast<std::streamsize>(yaml.size()));
            file.close();
            if (!file)
            {
                Logger::Error("Could not write Entity YAML: " + path.string());
                return false;
            }
            return true;
        }
    }

    bool EntityYamlWriter::Save(const SerializedEntity& root, std::string_view prefabName,
        const std::filesystem::path& path)
    {
        ryml::Tree tree;
        auto entities = BeginPrefab(tree, prefabName);
        return entities.readable() && WriteHierarchy(entities, { &root, 1 }, false) && SaveTree(tree, path);
    }

    bool EntityYamlWriter::SaveScene(std::span<const SerializedEntity> roots, const std::filesystem::path& path)
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
        return WriteHierarchy(entities, roots, true) && SaveTree(tree, path);
    }
}
