/*=============================================================================

 File   : AssetType.h
 Desc   : 初期 Asset の CPU データと共有型を定義する。

------------------------------------------------------------------------------

 Date   : 2026/10/15
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _ASSET_TYPE_H_
#define _ASSET_TYPE_H_

#include <array>
#include <cstdint>
#include <vector>

#include "AssetHandle.h"

namespace Hestia
{
    /// @brief AssetDB が初期対応する CPU Asset の種別。
    enum class AssetType
    {
        Mesh,
        Texture,
        MaterialShader
    };

    /// @brief Mesh の頂点位置。
    struct PositionVertex
    {
        std::array<float, 3> m_position{};
    };

    /// @brief Mesh の法線と接線。片方だけ設定する場合も、もう片方は既定値で埋まる。
    struct NormalTangentVertex
    {
        std::array<float, 3> m_normal{ 0.0f, 0.0f, -1.0f };
        std::array<float, 4> m_tangent{ 1.0f, 0.0f, 0.0f, 1.0f };
    };

    /// @brief Mesh の第一 UV 座標。
    struct PrimaryTexCoordVertex
    {
        std::array<float, 2> m_uv{};
    };

    /// @brief Mesh の頂点カラー。
    struct ColorVertex
    {
        std::array<float, 4> m_color{};
    };

    /// @brief CPU Mesh が保持する頂点 Stream の区分。
    enum class VertexStream
    {
        Position,
        NormalTangent,
        PrimaryTexCoord,
        Color,
        Count
    };

    /// @brief GPU Texture と CPU Asset 間で共有する形式。
    enum class TextureFormat
    {
        RGBA8Unorm,
        RGBA8Srgb
    };

    /// @brief 2D Texture の寸法、Mip 数、形式。
    struct TextureDesc
    {
        uint32_t m_width = 0;
        uint32_t m_height = 0;
        uint32_t m_mipLevels = 1;
        TextureFormat m_format = TextureFormat::RGBA8Unorm;

        /// @brief 初期 Texture 条件を満たす記述か確認する。
        /// @return 寸法が正で Mip が一つの場合: true。
        bool IsValid() const;
    };

    /// @brief Mesh 全体で共有する Index 配列内の描画範囲と MaterialSlot
    struct SubMesh
    {
        uint32_t m_firstIndex = 0;
        uint32_t m_indexCount = 0;
        uint32_t m_materialSlot = 0;
    };

    /// @brief 静的 Mesh の CPU 頂点 Stream、Index と SubMesh を保持する
    struct Mesh
    {
        AssetHandle<Mesh> m_handle;
        std::vector<PositionVertex> m_positions;
        std::vector<NormalTangentVertex> m_normalTangents;
        std::vector<PrimaryTexCoordVertex> m_primaryTexCoords;
        std::vector<ColorVertex> m_colors;
        std::vector<uint32_t> m_indices;
        std::vector<SubMesh> m_subMeshes;

        /// @brief 必須 Stream、属性長、Index、GPU Buffer サイズ、SubMesh の範囲を検証する
        /// @return 全ての Mesh 契約を満たす場合: true。
        bool IsValid() const;

        /// @brief 描画範囲の数を返す。SubMesh 未指定の場合は全 Index を一つの範囲とする
        /// @pre IsValid() が true
        std::size_t GetSubMeshCount() const;

        /// @brief 描画範囲を取得する。SubMesh 未指定の場合は全 Index を MaterialSlot 0 に対応付ける
        /// @param index 取得する SubMesh の添字
        /// @param out 成功時に描画範囲を格納する
        /// @return 取得成功: true / 添字が範囲外: false
        /// @pre IsValid() が true
        bool GetSubMesh(std::size_t index, SubMesh& out) const;
    };

    /// @brief GPU Texture の参照情報だけを持つ CPU Asset。
    struct Texture
    {
        AssetHandle<Texture> m_handle;
        TextureDesc m_desc;
    };

    inline bool TextureDesc::IsValid() const
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

    inline bool Mesh::IsValid() const
    {
        // 描画に必要な Stream と Index の有無を確認
        if (m_positions.empty() || m_indices.empty())
            return false;

        // 各 Stream と Index が GPU Buffer のサイズ上限に収まることを確認
        const std::size_t maxBufferBytes = UINT32_MAX;
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

    inline std::size_t Mesh::GetSubMeshCount() const
    {
        return m_subMeshes.empty() ? 1 : m_subMeshes.size();
    }

    inline bool Mesh::GetSubMesh(std::size_t index, SubMesh& out) const
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

#endif // _ASSET_TYPE_H_
