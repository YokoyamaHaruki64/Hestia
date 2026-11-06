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
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

#include "Asset/AssetHandle.h"
#include "Common/Include/DllAPI.h"

namespace Hestia
{
    /// @brief AssetDB が初期対応する CPU Asset の種別。
    enum class AssetType
    {
        Mesh,
        Texture
    };

    /// @brief Mesh の頂点位置。
    struct PositionVertex
    {
        std::array<float, 3> m_position{};
    };

    /// @brief Mesh の法線と接線。
    struct NormalTangentVertex
    {
        std::array<float, 3> m_normal{};
        std::array<float, 4> m_tangent{};
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
        bool HESTIA_ENGINE_API IsValid() const;
    };

    /// @brief 静的 Mesh の CPU 頂点 Stream と Index を保持する。
    struct Mesh
    {
        AssetHandle<Mesh> m_handle;
        std::vector<PositionVertex> m_positions;
        std::vector<NormalTangentVertex> m_normalTangents;
        std::vector<PrimaryTexCoordVertex> m_primaryTexCoords;
        std::vector<ColorVertex> m_colors;
        std::vector<uint32_t> m_indices;

        /// @brief 必須 Stream、属性長、Index、GPU Buffer サイズの妥当性を確認する。
        /// @return 全ての Mesh 契約を満たす場合: true。
        bool HESTIA_ENGINE_API IsValid() const;
    };

    /// @brief GPU Texture の参照情報だけを持つ CPU Asset。
    struct Texture
    {
        AssetHandle<Texture> m_handle;
        TextureDesc m_desc;
    };
}

#endif // _ASSET_TYPE_H_
