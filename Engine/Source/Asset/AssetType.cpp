/*=============================================================================

 File   : AssetType.cpp
 Desc   : Mesh / Texture の CPU データ妥当性 API を定義する。

------------------------------------------------------------------------------

 Date   : 2026/10/15
 Author : Yokoyama Haruki

=============================================================================*/

#include "pch.h"
#include "Asset/AssetType.h"

namespace Hestia
{
    bool TextureDesc::IsValid() const
    {
        if (m_width == 0 || m_height == 0 || m_mipLevels != 1)
            return false;

        switch (m_format)
        {
        case TextureFormat::RGBA8Unorm:
        case TextureFormat::RGBA8Srgb:
            return true;
        }

        return false;
    }

    bool Mesh::IsValid() const
    {
        if (m_positions.empty() || m_indices.empty())
            return false;

        const std::size_t maxBufferBytes = std::numeric_limits<uint32_t>::max();
        const auto fitsBuffer = [maxBufferBytes](std::size_t count, std::size_t elementSize)
        {
            return count <= maxBufferBytes / elementSize;
        };

        if (!fitsBuffer(m_positions.size(), sizeof(PositionVertex)) ||
            !fitsBuffer(m_normalTangents.size(), sizeof(NormalTangentVertex)) ||
            !fitsBuffer(m_primaryTexCoords.size(), sizeof(PrimaryTexCoordVertex)) ||
            !fitsBuffer(m_colors.size(), sizeof(ColorVertex)) ||
            !fitsBuffer(m_indices.size(), sizeof(uint32_t)))
            return false;

        const std::size_t vertexCount = m_positions.size();
        if ((!m_normalTangents.empty() && m_normalTangents.size() != vertexCount) ||
            (!m_primaryTexCoords.empty() && m_primaryTexCoords.size() != vertexCount) ||
            (!m_colors.empty() && m_colors.size() != vertexCount))
            return false;

        for (const uint32_t index : m_indices)
        {
            if (index >= vertexCount)
                return false;
        }

        return true;
    }
}
