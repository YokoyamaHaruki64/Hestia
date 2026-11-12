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
        // 描画に必要な Stream と Index の有無を確認
        if (m_positions.empty() || m_indices.empty())
            return false;

        // 各 Stream と Index が GPU Buffer のサイズ上限に収まることを確認
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

        // 頂点ごとの属性数を揃え、Index が共有頂点配列を参照することを確認
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

        // 描画範囲の終端を加算せず、共有 Index 配列の残り要素数で検証
        const std::size_t indexCount = m_indices.size();
        for (const SubMesh& subMesh : m_subMeshes)
        {
            if (subMesh.m_indexCount == 0 ||
                subMesh.m_firstIndex >= indexCount ||
                subMesh.m_indexCount > indexCount - subMesh.m_firstIndex)
                return false;
        }

        return true;
    }

    std::size_t Mesh::GetSubMeshCount() const
    {
        return m_subMeshes.empty() ? 1 : m_subMeshes.size();
    }

    bool Mesh::GetSubMesh(std::size_t index, SubMesh& out) const
    {
        if (index >= GetSubMeshCount())
            return false;

        // SubMesh を持たない既存 Mesh は全 Index を Slot 0 で描画する範囲を返す
        out = m_subMeshes.empty()
            ? SubMesh{ 0, static_cast<uint32_t>(m_indices.size()), 0 }
            : m_subMeshes[index];
        return true;
    }
}
