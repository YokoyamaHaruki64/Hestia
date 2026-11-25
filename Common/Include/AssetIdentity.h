/*=============================================================================

 File   : AssetIdentity.h
 Desc   : Assetの種別と永続GUIDを定義する。

------------------------------------------------------------------------------

 Date   : 2026/11/25
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _ASSET_IDENTITY_H_
#define _ASSET_IDENTITY_H_

#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

namespace Hestia
{
    enum class AssetType
    {
        Mesh,
        Texture,
        MaterialShader,
        Model,
        Prefab,
        Scene,
        Script
    };

    /// @brief 元Assetファイルの永続ID
    struct AssetGuid
    {
        std::array<uint64_t, 2> m_words{};
        bool operator==(const AssetGuid&) const = default;
        explicit operator bool() const { return m_words[0] != 0 || m_words[1] != 0; }

        /// @brief GUID文字列を解釈する。失敗時は空のGUID
        static AssetGuid Parse(std::string_view text);
        /// @brief YAMLに使うハイフン付き文字列を返す
        std::string ToString() const;

        struct Hash
        {
            size_t operator()(const AssetGuid& guid) const noexcept
            {
                return std::hash<uint64_t>{}(guid.m_words[0]) ^
                    (std::hash<uint64_t>{}(guid.m_words[1]) >> 1);
            }
        };
    };
}

#endif // _ASSET_IDENTITY_H_
