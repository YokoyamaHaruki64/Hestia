/*=============================================================================

 File   : AssetHandle.h
 Desc   : AssetSystem と Graphics で共有する型付き Asset 識別子を定義する。

------------------------------------------------------------------------------

 Date   : 2026/10/15
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _ASSET_HANDLE_H_
#define _ASSET_HANDLE_H_

#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

namespace Hestia
{
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

    /// @brief 主Assetを0、SubAssetを正のIDで指定する永続参照
    struct AssetRef
    {
        AssetGuid m_guid;
        uint32_t m_localId = 0;
        bool operator==(const AssetRef&) const = default;

        struct Hash
        {
            size_t operator()(const AssetRef& ref) const noexcept
            {
                return AssetGuid::Hash{}(ref.m_guid) ^ std::hash<uint32_t>{}(ref.m_localId);
            }
        };
    };

    /// @brief Asset 型と Index / Generation を組み合わせた識別子。
    template<class T>
    struct AssetHandle
    {
        /// @brief AssetStorage と GPUResourceStorage で共有する Index。
        uint32_t m_index = UINT32_MAX;

        /// @brief 将来の Index 再利用に備える世代番号。
        uint32_t m_generation = 0;

        /// @brief 無効値でないかを確認する。
        /// @return Index が無効値でない場合: true。
        bool IsValid() const
        {
            return m_index != UINT32_MAX;
        }
    };
}

#endif // _ASSET_HANDLE_H_
