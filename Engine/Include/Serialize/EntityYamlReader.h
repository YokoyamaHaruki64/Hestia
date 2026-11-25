/*=============================================================================

 File   : EntityYamlReader.h
 Desc   : Scene YAMLからEntityのスナップショットを取得する。

------------------------------------------------------------------------------

 Date   : 2026/11/25
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _ENTITY_YAML_READER_H_
#define _ENTITY_YAML_READER_H_

#include <filesystem>
#include <optional>
#include <vector>

#include "Serialize/SerializedEntity.h"

namespace Hestia
{
    /// @brief Entity階層のYAMLを読み取る。
    class EntityYamlReader
    {
    public:
        /// @brief Sceneを検証し、ルートの値を返す。読み込み失敗時は空。
        static std::optional<std::vector<SerializedEntity>> LoadScene(const std::filesystem::path& path);
    };
}

#endif // _ENTITY_YAML_READER_H_
