/*=============================================================================

 File   : Serializer.h
 Desc   : Assetごとの保存データを共通のYAML Treeへ変換する入口

------------------------------------------------------------------------------

 Date   : 2026/11/19
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _SERIALIZER_H_
#define _SERIALIZER_H_

#include <optional>
#include <span>
#include <string_view>

#include "RapidYaml/rapidyaml.hpp"

namespace Hestia
{
    class Material;
    struct Prefab;
    struct SerializedEntity;

    template<class T>
    struct Serializer;

    /// @brief ルートと子孫をPrefab形式へ変換する
    template<>
    struct Serializer<Prefab>
    {
        /// @brief Prefab全体のYAML Treeを作る
        /// @param root 保存する階層のスナップショット
        /// @param prefabName 空でないUTF-8のPrefab名
        /// @return 変換成功時はTree、不正な保存データではstd::nullopt
        static std::optional<ryml::Tree> Serialize(const SerializedEntity& root, std::string_view prefabName);
    };

    /// @brief 複数ルートとScene固有の値をScene形式へ変換する
    template<>
    struct Serializer<std::span<const SerializedEntity>>
    {
        /// @brief Scene全体のYAML Treeを作る
        /// @param roots 保存するルートの列
        /// @return 変換成功時はTree、不正な保存データではstd::nullopt
        static std::optional<ryml::Tree> Serialize(std::span<const SerializedEntity> roots);
    };

    /// @brief MaterialをShader参照とPropertyの差分を持つ形式へ変換する
    template<>
    struct Serializer<Material>
    {
        /// @brief Material全体のYAML Treeを作る
        /// @param material 保存するMaterial
        /// @return 変換成功時はTree、解決できない参照や不正な値があればstd::nullopt
        static std::optional<ryml::Tree> Serialize(const Material& material);
    };
}

#endif // _SERIALIZER_H_
