HEADER_BEGIN
/*=============================================================================

 File   : FieldGenerator.cpp
 Desc   : 選択されたフィールドの配置 JSON から描画 Entity を生成する。

------------------------------------------------------------------------------

 Date   : 2026/11/26
 Author : Yokoyama Haruki

=============================================================================*/

#include "Common/Include/EntityHandle.h"

namespace HestiaGame
{
    /// @brief 起動時にフィールドの描画配置を生成する。
    class FieldGenerator
    {
    public:
        /// @brief 選択中の bundle を読み込み、現在の Scene に配置する。
        void OnStart(Hestia::EntityHandle);
    };
}
HEADER_END

SOURCE_BEGIN
#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "Common/Include/StringUtils.h"
#include "Engine/Include/Game/MeshRenderer.h"
#include "Game/Include/GameAssets.h"
#include "Game/Include/GameLogger.h"
#include "Game/Include/World.h"
#include "Engine/Include/Game/Transform.h"
#include <RapidYaml/rapidyaml.hpp>

#include "GeneratedScript/Scripts/Gameplay/Player.generated.h"

namespace HestiaGame
{
    namespace
    {
        const std::filesystem::path ASSET_ROOT = "../Assets";
        const std::filesystem::path FIELD_ROOT = "Scenes/Fields";

        struct FieldModel
        {
            Hestia::AssetHandle<Hestia::Mesh> m_mesh;
            std::vector<size_t> m_slots;
        };

        struct FieldPlacement
        {
            Hestia::AssetHandle<Hestia::Mesh> m_mesh;
            Vector3 m_position;
            Quaternion m_rotation;
            Vector3 m_scale;
            std::vector<Hestia::AssetHandle<Hestia::Material>> m_materials;
        };

        std::optional<ryml::Tree> ReadJson(const std::filesystem::path& relativePath)
        {
            const auto fail = [&](std::string_view reason) -> std::optional<ryml::Tree>
            {
                Logger::Error("FieldGenerator: " + Hestia::PathToUtf8(relativePath) + ": " + std::string(reason));
                return std::nullopt;
            };

            // AssetSystem と同じ Assets ルートからデータを開く
            std::ifstream file(ASSET_ROOT / relativePath, std::ios::binary);
            if (!file)
                return fail("could not open JSON");
            const std::string source((std::istreambuf_iterator<char>(file)), {});
            if (file.bad() || source.empty())
                return fail("could not read JSON");

            // RapidYAML の構文エラーをログへ戻す
            struct ParseError { std::string m_message; };
            auto callbacks = ryml::get_callbacks();
            callbacks.set_error_parse([](ryml::csubstr message, const ryml::ErrorDataParse&, void*)
            {
                throw ParseError{ std::string(message.str, message.len) };
            });
            ryml::EventHandlerTree handler(callbacks);
            ryml::Parser parser(&handler);
            try
            {
                return ryml::parse_json_in_arena(&parser, ryml::to_csubstr(source));
            }
            catch (const ParseError& error)
            {
                return fail(error.m_message);
            }
        }

        std::string ReadString(ryml::ConstNodeRef node)
        {
            if (!node.readable() || !node.has_val() || !node.is_val_quoted())
                return {};
            const auto value = node.val();
            return std::string(value.str, value.len);
        }

        template<class T>
        bool ReadNumber(ryml::ConstNodeRef node, T& result)
        {
            if (!node.readable() || !node.has_val() || node.is_val_quoted())
                return false;
            const auto value = node.val();
            const auto end = value.str + value.len;
            const auto parsed = std::from_chars(value.str, end, result);
            return parsed.ec == std::errc{} && parsed.ptr == end;
        }

        template<size_t Count>
        bool ReadVector(ryml::ConstNodeRef node, std::array<float, Count>& values)
        {
            if (!node.readable() || !node.is_seq() || node.num_children() != Count)
                return false;
            for (size_t index = 0; index < Count; ++index)
            {
                if (!ReadNumber(node[index], values[index]) || !std::isfinite(values[index]))
                    return false;
            }
            return true;
        }

        bool FindModelMesh(const Hestia::SerializedEntity& node, Hestia::AssetRef& ref)
        {
            // FBX のモデル原点で書き出した単一 Mesh を Prefab から取り出す
            for (const auto& component : node.m_components)
            {
                if (component.m_typeName != "Hestia::MeshRenderer")
                    continue;
                const auto field = component.m_fields.find("mesh");
                const auto* mesh = field != component.m_fields.end()
                    ? std::get_if<Hestia::AssetRef>(&field->second.m_value) : nullptr;
                if (ref.m_guid || !mesh || !mesh->m_guid)
                    return false;
                ref = *mesh;
            }
            return std::ranges::all_of(node.m_children, [&](const auto& child)
            {
                return FindModelMesh(child, ref);
            });
        }

        bool Spawn(const FieldPlacement& placement)
        {
            const auto entity = World::CreateEntity();
            if (!entity.IsValid())
                return false;

            // 親なし Entity の Local TRS にワールド TRS をそのまま設定する
            auto transform = World::GetProcess<Transform>(entity);
            auto renderer = World::GetProcess<MeshRenderer>(entity);
            if (!transform.SetLocalPosition(placement.m_position) ||
                !transform.SetLocalRotation(placement.m_rotation) ||
                !transform.SetLocalScale(placement.m_scale) ||
                !World::AddProcess<MeshRenderer>(entity) ||
                !renderer.SetMesh(placement.m_mesh))
            {
                World::DestroyEntity(entity);
                return false;
            }

            // 共有 Mesh は変えず、使用する Material を各 Entity の Slot に保持する
            for (size_t slot = 0; slot < placement.m_materials.size(); ++slot)
            {
                if (renderer.SetMaterial(slot, placement.m_materials[slot]))
                    continue;
                World::DestroyEntity(entity);
                return false;
            }
            return true;
        }
    }

    void FieldGenerator::OnStart(Hestia::EntityHandle)
    {
        // 起動設定から使用する bundle の名前を取得する
        const auto selection = ReadJson(FIELD_ROOT / "active.json");
        if (!selection)
            return;
        const auto selected = selection->crootref();
        const auto bundle = selected.is_map() ? ReadString(selected.find_child("bundle")) : std::string{};
        if (bundle.empty() || bundle.find_first_not_of(
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-") != std::string::npos)
        {
            Logger::Error("FieldGenerator: invalid bundle name in active.json");
            return;
        }

        const auto fail = [&](std::string_view reason)
        {
            Logger::Error("FieldGenerator [" + bundle + "]: " + std::string(reason));
        };
        const auto directory = FIELD_ROOT / bundle;
        const auto materialsJson = ReadJson(directory / "material.json");
        const auto modelsJson = ReadJson(directory / "modelSlots.json");
        const auto sceneJson = ReadJson(directory / "scene.json");
        if (!materialsJson || !modelsJson || !sceneJson)
            return;
        if (!materialsJson->crootref().is_seq() || !modelsJson->crootref().is_seq() || !sceneJson->crootref().is_seq())
            return fail("expected arrays in material.json, modelSlots.json and scene.json");

        // 同じ材質名の定義を bundle 内で一度だけロードする
        std::unordered_map<std::string, Hestia::AssetHandle<Hestia::Material>> materials;
        for (const auto row : materialsJson->crootref().children())
        {
            if (!row.is_map())
                return fail("invalid material definition");
            auto name = ReadString(row.find_child("name"));
            const auto path = ReadString(row.find_child("asset"));
            if (name.empty() || path.empty())
                return fail("material definition requires name and asset");
            const auto material = Assets::LoadMaterial(path);
            if (!material.IsValid() || !materials.emplace(name, material).second)
                return fail("could not load unique material " + name);
        }

        // FBX を共有 Mesh に解決し、インポート後の Slot を元の FBX スロットへ対応づける
        std::unordered_map<std::string, FieldModel> models;
        for (const auto row : modelsJson->crootref().children())
        {
            if (!row.is_map())
                return fail("invalid model slot definition");
            auto path = ReadString(row.find_child("model"));
            const auto slots = row.find_child("slots");
            if (path.empty() || !slots.readable() || !slots.is_seq() || !slots.has_children())
                return fail("model slot definition requires model and slots");

            const auto* prefab = Assets::Get(Assets::Load<Hestia::Prefab>(path));
            Hestia::AssetRef ref;
            if (!prefab || !FindModelMesh(prefab->m_root, ref) || !ref.m_guid)
                return fail("could not resolve a single Mesh from " + path);

            FieldModel model;
            model.m_mesh = Assets::Load<Hestia::Mesh>(ref);
            const auto* mesh = Assets::Get(model.m_mesh);
            if (!mesh)
                return fail("could not load Mesh from " + path);
            for (const auto slot : slots.children())
            {
                size_t index = 0;
                if (!ReadNumber(slot, index))
                    return fail("invalid source slot in " + path);
                model.m_slots.push_back(index);
            }
            if (std::ranges::any_of(mesh->m_subMeshes, [&](const auto& subMesh)
            {
                return subMesh.m_materialSlot >= model.m_slots.size();
            }))
                return fail("missing source slot in " + path);
            if (!models.emplace(path, std::move(model)).second)
                return fail("duplicate model slot definition " + path);
        }

        // 配置をすべて解決してから Scene の生成へ進む
        std::vector<FieldPlacement> placements;
        placements.reserve(sceneJson->crootref().num_children());
        for (const auto row : sceneJson->crootref().children())
        {
            const auto rowLabel = "scene.json entry " + std::to_string(placements.size());
            if (!row.is_map())
                return fail("invalid " + rowLabel);

            const auto path = ReadString(row.find_child("model"));
            const auto model = models.find(path);
            const auto transform = row.find_child("transform");
            const auto names = row.find_child("materials");
            if (model == models.end() || !transform.readable() || !transform.is_map() ||
                !names.readable() || !names.is_seq())
                return fail("invalid model, transform or materials in " + rowLabel + " (" + path + ")");

            std::array<float, 3> position, scale;
            std::array<float, 4> rotation;
            if (!ReadVector(transform.find_child("position"), position) ||
                !ReadVector(transform.find_child("rotation"), rotation) ||
                !ReadVector(transform.find_child("scale"), scale) ||
                std::ranges::any_of(scale, [](float value) { return value <= 0.0f; }))
                return fail("invalid TRS in " + rowLabel);

            FieldPlacement placement{
                .m_mesh = model->second.m_mesh,
                .m_position = { position[0], position[1], position[2] },
                .m_rotation = { rotation[0], rotation[1], rotation[2], rotation[3] },
                .m_scale = { scale[0], scale[1], scale[2] },
            };
            for (const auto slot : model->second.m_slots)
            {
                const auto name = slot < names.num_children() ? ReadString(names[slot]) : std::string{};
                const auto material = materials.find(name);
                if (material == materials.end())
                    return fail("unknown material for " + rowLabel + " (" + path + ")");
                placement.m_materials.push_back(material->second);
            }
            placements.push_back(std::move(placement));
        }

        // Script が動く Scene の World へ、配置一件につき描画 Entity 一件を追加する
        size_t spawned = 0;
        for (const auto& placement : placements)
        {
            if (!Spawn(placement))
            {
                fail("could not spawn scene.json entry " + std::to_string(spawned));
                break;
            }
            ++spawned;
        }
        Logger::Log("FieldGenerator [" + bundle + "]: render objects " + std::to_string(spawned) +
            "/" + std::to_string(placements.size()) + ", shared models " + std::to_string(models.size()) +
            ", materials " + std::to_string(materials.size()));

        auto player = World::Instantiate(Assets::Load<Hestia::Prefab>("Models/Y-Bot.fbx"));
        if (!player.IsValid()) return;
        World::AddScript<Player>(player);
    }
}
SOURCE_END
