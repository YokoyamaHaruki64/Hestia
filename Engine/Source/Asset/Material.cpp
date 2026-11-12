/*=============================================================================

 File   : Material.cpp
 Desc   : Material の Property 値、GPU 転送用キャッシュ、Shader 参照を実装する。

------------------------------------------------------------------------------

 Date   : 2026/11/06
 Author : Yokoyama Haruki

=============================================================================*/

#include "pch.h"
#include "Asset/Material.h"
#include "Asset/MaterialShader.h"
#include "Common/Include/StringUtils.h"

#include <cstring>

#include "Log/Logger.h"

namespace Hestia
{
    namespace
    {
        // CPU 側の型を HLSL の定数バッファにパックする
        // 1つの Property の値を 1 つの PackedConstant に変換
        // Matrix4x4が64Byteであるため、最大64Byteまでのパックを想定
        struct PackedConstant
        {
            std::array<std::byte, 64> m_bytes{};
            std::size_t m_size = 0;
        };

        PackedConstant PackConstant(const MaterialPropertyValue& value, bool rowMajor)
        {
            PackedConstant result;

            // std::visit で型ごとに処理するための Visitor
            struct Visitor
            {
                PackedConstant& m_result;
                bool m_rowMajor;

                Visitor(PackedConstant& result, bool rowMajor) : m_result(result), m_rowMajor(rowMajor) {}
                ~Visitor() = default;

                // 書き込みの共通処理
                void Write(const void* data, std::size_t size)
                {
                    m_result.m_size = size;
                    std::memcpy(m_result.m_bytes.data(), data, size);
                }

                // 型ごとの書き込み処理
                void operator()(bool value)
                {
                    // boolは32bitにパックする
                    uint32_t intValue = value ? 1u : 0u;
                    Write(&intValue, sizeof(uint32_t));
                }
                void operator()(const int32_t value) { Write(&value,sizeof(int32_t)); }
                void operator()(const float value) { Write(&value, sizeof(float)); }
                void operator()(const Vector2& value) { Write(&value, sizeof(Vector2)); }
                void operator()(const Vector3& value) { Write(&value, sizeof(Vector3)); }
                void operator()(const Vector4& value) { Write(&value, sizeof(Vector4)); }
                void operator()(const Color& value) { Write(&value, sizeof(Color)); }
                void operator()(const Matrix3x3& value)
                {
                    constexpr std::size_t vectorStride = sizeof(float) * 4;
                    m_result.m_size = vectorStride * 3;

                    // Reflection の major 順に各行列ベクトルを 16-byte stride で配置
                    for (std::size_t majorIndex = 0; majorIndex < 3; ++majorIndex)
                    {
                        for (std::size_t minorIndex = 0; minorIndex < 3; ++minorIndex)
                        {
                            const std::size_t row = m_rowMajor ? majorIndex : minorIndex;
                            const std::size_t column = m_rowMajor ? minorIndex : majorIndex;
                            const std::size_t offset = majorIndex * vectorStride + minorIndex * sizeof(float);
                            std::memcpy(m_result.m_bytes.data() + offset, &value.m[row][column], sizeof(float));
                        }
                    }
                }
                void operator()(const Matrix4x4& value)
                {
                    if (m_rowMajor) Write(&value, sizeof(Matrix4x4));
                    else
                    {
                        auto transposed = value.Transpose();
                        Write(&transposed, sizeof(Matrix4x4));
                    }
                }


                // Texture は SRV で扱うため、定数バッファには書き込まない
                void operator()(const AssetHandle<Texture>&) {}
            };

            // std::visit で型ごとに処理
            std::visit(Visitor{ result, rowMajor }, value);

            return result;
        }
    }

    std::size_t Material::GetPackedConstantSize(const MaterialPropertyDesc& property)
    {
        return PackConstant(property.m_defaultValue, property.m_rowMajor).m_size;
    }

    Material::~Material()
    {
        if (m_shader != nullptr)
            std::erase(m_shader->m_materials, this);
    }

    void Material::OnShaderDestroyed()
    {
        m_shader = nullptr;
        m_values.clear();
        m_constantBuffer.clear();
        ++m_valueRevision;
    }

    MaterialPropertyID Material::MakePropertyID(std::string_view name)
    {
        return StringToHash(name);
    }

    void Material::SetShader(MaterialShader* shader)
    {
        if (shader == m_shader)
            return;

        // 新しい定義を既定値で埋め、移行できる値だけを引き継ぐ
        std::vector<MaterialPropertyValue> values;
        std::vector<std::byte> constants;
        if (shader != nullptr)
        {
            values.reserve(shader->m_properties.size());
            constants.resize(shader->m_constantBufferSize);
            for (const MaterialPropertyDesc& property : shader->m_properties)
            {
                const MaterialPropertyValue* value = &property.m_defaultValue;
                if (m_shader != nullptr)
                {
                    const auto previous = m_shader->m_propertyIndices.find(MakePropertyID(property.m_name));
                    if (previous != m_shader->m_propertyIndices.end() &&
                        m_shader->m_properties[previous->second].m_name == property.m_name &&
                        m_values[previous->second].index() == value->index())
                        value = &m_values[previous->second];
                }

                values.push_back(*value);
                if (property.m_offset != UINT32_MAX)
                {
                    const PackedConstant packed = PackConstant(*value, property.m_rowMajor);
                    std::memcpy(constants.data() + property.m_offset, packed.m_bytes.data(), packed.m_size);
                }
            }

            // 新しい逆索引を登録できてから旧 Shader との参照を解除
            shader->m_materials.push_back(this);
        }

        if (m_shader != nullptr)
            std::erase(m_shader->m_materials, this);

        m_shader = shader;
        m_values = std::move(values);
        m_constantBuffer = std::move(constants);
        ++m_valueRevision;
    }

    bool Material::SetProperty(MaterialPropertyID id, const MaterialPropertyValue& value)
    {
        const MaterialPropertyDesc* property = m_shader != nullptr ? m_shader->FindProperty(id) : nullptr;
        if (property == nullptr || property->m_defaultValue.index() != value.index())
        {
            Logger::Error("Material property is missing or has a different type");
            return false;
        }

        MaterialPropertyValue& current = m_values[property - m_shader->m_properties.data()];

        // Texture は値だけを更新し、数値系は変更箇所だけキャッシュへ反映
        if (const auto* texture = std::get_if<AssetHandle<Texture>>(&value))
        {
            const auto previous = std::get<AssetHandle<Texture>>(current);
            if (previous.m_index == texture->m_index && previous.m_generation == texture->m_generation)
                return true;
        }
        else
        {
            const PackedConstant packed = PackConstant(value, property->m_rowMajor);
            if (packed.m_bytes == PackConstant(current, property->m_rowMajor).m_bytes)
                return true;

            if (property->m_offset != UINT32_MAX)
            {
                std::memcpy(m_constantBuffer.data() + property->m_offset,
                    packed.m_bytes.data(), packed.m_size);
            }
        }

        current = value;
        ++m_valueRevision;
        return true;
    }

    const MaterialPropertyValue* Material::FindPropertyValue(MaterialPropertyID id) const
    {
        const MaterialPropertyDesc* property = m_shader != nullptr ? m_shader->FindProperty(id) : nullptr;
        if (property == nullptr)
        {
            Logger::Error("Material property is missing");
            return nullptr;
        }

        return &m_values[property - m_shader->m_properties.data()];
    }
}
