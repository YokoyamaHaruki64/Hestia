/*=============================================================================

 File   : EntityYamlWriter.h
 Desc   : EntityのスナップショットをYAMLへ書き出す。

------------------------------------------------------------------------------

 Date   : 2026/11/19
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _ENTITY_YAML_WRITER_H_
#define _ENTITY_YAML_WRITER_H_

#include <filesystem>
#include <span>
#include <string_view>

namespace Hestia
{
    struct SerializedEntity;

    /// @brief Entityとその子孫をPrefab形式のYAMLへ書き出す
    class EntityYamlWriter
    {
    public:
        /// @brief ルートから子孫をたどり、Prefab内のIDを割り当てて保存する
        /// @param root 保存する階層のスナップショット
        /// @param prefabName 空でないUTF-8のPrefab名
        /// @param path 出力先ファイル。親ディレクトリは作成済みであること
        /// @return 保存成功: true / 保存データが不正または出力に失敗: false
        static bool Save(const SerializedEntity& root, std::string_view prefabName,
            const std::filesystem::path& path);

        /// @brief 複数ルートとScene固有の値を1つのYAMLに保存する。
        static bool SaveScene(std::span<const SerializedEntity> roots, const std::filesystem::path& path);
    };
}

#endif // _ENTITY_YAML_WRITER_H_
