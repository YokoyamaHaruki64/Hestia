/*=============================================================================

 File   : MaterialShader.cpp
 Desc   : MaterialShader の共通レイアウト、Property 検索、参照解除を実装する。

------------------------------------------------------------------------------

 Date   : 2026/11/06
 Author : Yokoyama Haruki

=============================================================================*/

#include "pch.h"
#include "Asset/MaterialShader.h"

#include "Asset/Material.h"
#include "Log/Logger.h"

namespace Hestia
{
    std::unique_ptr<MaterialShader> MaterialShader::Create(
        std::string name, std::vector<MaterialPropertyDesc> properties,
        uint32_t constantBufferSize, std::vector<MaterialShaderPass> passes, bool isInstancing)
    {
        if (name.empty())
        {
            Logger::Error("MaterialShader requires a name");
            return nullptr;
        }

        // DX12 が Stream を固定スロットとして使えるよう、作成時に範囲と重複を確認
        for (const auto& pass : passes)
        {
            std::array<bool, static_cast<std::size_t>(VertexStream::Count)> usedStreams{};
            for (const auto stream : pass.m_pipeline.m_requiredVertexStreams)
            {
                const auto slot = static_cast<std::size_t>(stream);
                if (slot >= usedStreams.size() || usedStreams[slot])
                {
                    Logger::Error("MaterialShader Pass has an invalid or duplicate vertex stream");
                    return nullptr;
                }
                usedStreams[slot] = true;
            }
        }

        auto shader = std::unique_ptr<MaterialShader>(new MaterialShader());
        shader->m_propertyIndices.reserve(properties.size());
        std::vector<std::pair<uint32_t, uint32_t>> constantRanges;
        std::unordered_set<uint32_t> resourceSlots;

        // 共通レイアウトを受け取る境界で Property 名と配置を検証
        for (std::size_t index = 0; index < properties.size(); ++index)
        {
            const MaterialPropertyDesc& property = properties[index];
            const MaterialPropertyID id = Material::MakePropertyID(property.m_name);
            if (property.m_name.empty() || !shader->m_propertyIndices.emplace(id, index).second)
            {
                Logger::Error("MaterialShader has an empty, duplicate, or colliding property name");
                return nullptr;
            }

            const bool isTexture = std::holds_alternative<AssetHandle<Texture>>(property.m_defaultValue);
            if (property.m_resourceSlot != UINT32_MAX &&
                (!isTexture || !resourceSlots.insert(property.m_resourceSlot).second))
            {
                Logger::Error("MaterialShader has an invalid or duplicate texture slot");
                return nullptr;
            }

            if (property.m_offset == UINT32_MAX)
                continue;

            const std::size_t packedSize = Material::GetPackedConstantSize(property);
            if (isTexture || property.m_size < packedSize || property.m_offset > constantBufferSize ||
                property.m_size > constantBufferSize - property.m_offset)
            {
                Logger::Error("MaterialShader property exceeds the constant buffer layout");
                return nullptr;
            }

            constantRanges.emplace_back(property.m_offset, property.m_offset + property.m_size);
        }

        // 隣接範囲の交差を調べ、別の Property の書き換えを防ぐ
        std::ranges::sort(constantRanges);
        for (std::size_t index = 1; index < constantRanges.size(); ++index)
        {
            if (constantRanges[index].first < constantRanges[index - 1].second)
            {
                Logger::Error("MaterialShader constant buffer properties overlap");
                return nullptr;
            }
        }

        shader->m_name = std::move(name);
        shader->m_properties = std::move(properties);
        shader->m_constantBufferSize = constantBufferSize;
        shader->m_passes = std::move(passes);
        shader->m_isInstancing = isInstancing;
        return shader;
    }

    MaterialShader::~MaterialShader()
    {
        // Shader が先に破棄される場合も Material の借用参照を解除
        for (Material* material : m_materials)
            material->OnShaderDestroyed();
    }

    const MaterialPropertyDesc* MaterialShader::FindProperty(MaterialPropertyID id) const
    {
        const auto found = m_propertyIndices.find(id);
        return found == m_propertyIndices.end() ? nullptr : &m_properties[found->second];
    }

    const MaterialPropertyDesc* MaterialShader::FindProperty(std::string_view name) const
    {
        return FindProperty(Material::MakePropertyID(name));
    }
}
