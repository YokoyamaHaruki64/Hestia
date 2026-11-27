/*=============================================================================

 File   : MaterialSerialization.cpp
 Desc   : MaterialのPropertyと永続参照をYAMLへ保存・復元する

------------------------------------------------------------------------------

 Date   : 2026/11/26
 Author : Yokoyama Haruki

=============================================================================*/

#include "pch.h"
#include "Asset/AssetSystem.h"
#include "Common/Include/StringUtils.h"
#include "Log/Logger.h"
#include "Serialize/SerializeFields.h"
#include "Serialize/Serializer.h"
#include "Serialize/YamlFile.h"

#include <array>
#include <cmath>
#include <unordered_set>

namespace Hestia
{
    namespace
    {
        constexpr uint32_t MATERIAL_VERSION = 1;

        std::optional<SerializedValue> EncodeFloats(std::span<const float> values)
        {
            SerializedValue::Array result;
            result.reserve(values.size());
            for (const float value : values)
            {
                if (!std::isfinite(value))
                    return std::nullopt;
                result.push_back(SerializeDetail::ToSerializedValue(value));
            }
            return SerializedValue{ std::move(result) };
        }

        std::optional<SerializedValue> EncodeProperty(const MaterialPropertyValue& value)
        {
            return std::visit([]<class T>(const T& item) -> std::optional<SerializedValue>
            {
                if constexpr (std::is_same_v<T, AssetHandle<Texture>>)
                {
                    const auto ref = AssetSystem::GetReference(item);
                    if (item.IsValid() && (!ref.m_guid || ref.m_localId != 0))
                        return std::nullopt;
                    return SerializedValue{ ref };
                }
                else if constexpr (std::is_same_v<T, Vector2>)
                    return EncodeFloats(std::array{ item.x, item.y });
                else if constexpr (std::is_same_v<T, Vector3>)
                    return EncodeFloats(std::array{ item.x, item.y, item.z });
                else if constexpr (std::is_same_v<T, Vector4>)
                    return EncodeFloats(std::array{ item.x, item.y, item.z, item.w });
                else if constexpr (std::is_same_v<T, Color>)
                    return EncodeFloats(std::array{ item.r, item.g, item.b, item.a });
                else if constexpr (std::is_same_v<T, Matrix3x3> || std::is_same_v<T, Matrix4x4>)
                {
                    SerializedValue::Array rows;
                    rows.reserve(std::size(item.m));
                    for (const auto& row : item.m)
                    {
                        auto encoded = EncodeFloats(row);
                        if (!encoded)
                            return std::nullopt;
                        rows.push_back(std::move(*encoded));
                    }
                    return SerializedValue{ std::move(rows) };
                }
                else
                {
                    if constexpr (std::is_same_v<T, float>)
                    {
                        if (!std::isfinite(item))
                            return std::nullopt;
                    }
                    return SerializeDetail::ToSerializedValue(item);
                }
            }, value);
        }

        bool DecodeFloats(const SerializedValue& value, std::span<float> output)
        {
            const auto* entries = std::get_if<SerializedValue::Array>(&value.m_value);
            if (!entries || entries->size() != output.size())
                return false;
            for (size_t i = 0; i < output.size(); ++i)
            {
                const auto decoded = SerializeDetail::FromSerializedValue<float>((*entries)[i]);
                if (!decoded || !std::isfinite(*decoded))
                    return false;
                output[i] = *decoded;
            }
            return true;
        }

        std::optional<MaterialPropertyValue> DecodeProperty(const SerializedValue& value,
            const MaterialPropertyDesc& property)
        {
            // Shaderの定義から期待する型を得て、配列の各成分と参照も検証する
            return std::visit([&]<class T>(const T&) -> std::optional<MaterialPropertyValue>
            {
                if constexpr (std::is_same_v<T, AssetHandle<Texture>>)
                {
                    const auto* ref = std::get_if<AssetRef>(&value.m_value);
                    if (!ref || ref->m_localId != 0)
                        return std::nullopt;
                    if (!ref->m_guid)
                        return T{};

                    const auto handle = AssetSystem::Load<Texture>(*ref);
                    if (handle.IsValid())
                        return handle;
                }
                else if constexpr (std::is_same_v<T, Matrix3x3> || std::is_same_v<T, Matrix4x4>)
                {
                    T decoded{};
                    const auto* rows = std::get_if<SerializedValue::Array>(&value.m_value);
                    if (!rows || rows->size() != std::size(decoded.m))
                        return std::nullopt;
                    for (size_t i = 0; i < rows->size(); ++i)
                    {
                        if (!DecodeFloats((*rows)[i], decoded.m[i]))
                            return std::nullopt;
                    }
                    return decoded;
                }
                else if constexpr (std::is_same_v<T, Vector2> || std::is_same_v<T, Vector3> ||
                    std::is_same_v<T, Vector4> || std::is_same_v<T, Color>)
                {
                    constexpr size_t count = std::is_same_v<T, Vector2> ? 2 : std::is_same_v<T, Vector3> ? 3 : 4;
                    float components[count];
                    if (!DecodeFloats(value, components))
                        return std::nullopt;
                    if constexpr (count == 2)
                        return T{ components[0], components[1] };
                    else if constexpr (count == 3)
                        return T{ components[0], components[1], components[2] };
                    else
                        return T{ components[0], components[1], components[2], components[3] };
                }
                else
                {
                    const auto decoded = SerializeDetail::FromSerializedValue<T>(value);
                    if (decoded && (!std::is_same_v<T, float> || std::isfinite(*decoded)))
                        return *decoded;
                }
                return std::nullopt;
            }, property.m_defaultValue);
        }
    }

    std::optional<ryml::Tree> Serializer<Material>::Serialize(const Material& material)
    {
        const auto* shader = material.GetShader();
        const auto shaderRef = shader ? AssetSystem::GetReference(shader->GetHandle()) : AssetRef{};
        if (!shaderRef.m_guid || shaderRef.m_localId != 0)
        {
            Logger::Error("Material requires a persistent MaterialShader reference");
            return std::nullopt;
        }

        // 既定値と異なるPropertyをShaderの宣言順で保存する
        ryml::Tree tree;
        auto root = tree.rootref();
        root |= ryml::MAP;
        root["version"].save(MATERIAL_VERSION);
        WriteYamlValue(root["shader"], SerializedValue{ shaderRef });
        auto properties = root["properties"];
        properties |= ryml::MAP;
        for (const auto& property : shader->GetProperties())
        {
            const auto* current = material.FindPropertyValue(Material::MakePropertyID(property.m_name));
            const auto encoded = current ? EncodeProperty(*current) : std::nullopt;
            if (!encoded)
            {
                Logger::Error("Could not serialize Material Property '" + property.m_name + "'");
                return std::nullopt;
            }
            const auto defaultValue = EncodeProperty(property.m_defaultValue);
            if (defaultValue && *defaultValue == *encoded)
                continue;

            auto node = properties.append_child();
            node.save_key(ryml::to_csubstr(property.m_name), ryml::KEY_DQUO);
            WriteYamlValue(node, *encoded);
        }
        if (properties.num_children() == 0)
            properties |= ryml::FLOW_SL;
        return tree;
    }

    AssetHandle<Material> AssetLoader<Material>::Load(const AssetRef& ref, std::string_view path)
    {
        const auto fail = [&](std::string_view reason)
        {
            Logger::Error("Material [" + std::string(path) + "]: " + std::string(reason));
            return AssetHandle<Material>{};
        };

        // ファイル形式とShader参照を確認してからPropertyの型を決める
        std::string error;
        const auto tree = YamlReader::Read(PathFromUtf8(path), error);
        if (!tree)
            return fail(error);
        const auto root = tree->crootref();
        if (!root.is_map())
            return fail("expected a YAML map");

        const auto properties = root.find_child("properties");
        const auto versionValue = ReadYamlValue(root.find_child("version"));
        if (!versionValue || SerializeDetail::FromSerializedValue<uint32_t>(*versionValue) != MATERIAL_VERSION)
            return fail("unsupported Material version");
        if (root.num_children() != (properties.readable() ? 3u : 2u) ||
            (properties.readable() && !properties.is_map()))
            return fail("expected version, shader and optional properties");

        const auto shaderValue = ReadYamlValue(root.find_child("shader"));
        const auto* shaderRef = shaderValue ? std::get_if<AssetRef>(&shaderValue->m_value) : nullptr;
        if (!shaderRef || !shaderRef->m_guid || shaderRef->m_localId != 0)
            return fail("invalid MaterialShader reference");
        auto* shader = AssetSystem::Get(AssetSystem::Load<MaterialShader>(*shaderRef));
        if (!shader)
            return fail("could not load MaterialShader " + shaderRef->m_guid.ToString());

        // 既定値を持つ一時Materialに差分を適用し、全項目が揃ってから登録する
        auto material = std::make_unique<Material>();
        material->SetShader(shader);
        if (properties.readable())
        {
            std::unordered_set<std::string_view> names;
            for (const auto node : properties.children())
            {
                const auto key = node.key();
                const std::string_view name(key.str, key.len);
                const auto* property = shader->FindProperty(name);
                if (!property || !names.insert(name).second)
                    return fail("unknown or duplicate Property '" + std::string(name) + "'");

                const auto value = ReadYamlValue(node);
                const auto decoded = value ? DecodeProperty(*value, *property) : std::nullopt;
                if (!decoded || !material->SetProperty(Material::MakePropertyID(name), *decoded))
                    return fail("invalid value or reference for Property '" + std::string(name) + "'");
            }
        }
        return AssetSystem::RegisterFromLoader(ref, std::move(material));
    }
}
