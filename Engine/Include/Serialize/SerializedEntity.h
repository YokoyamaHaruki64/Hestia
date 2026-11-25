/*=============================================================================

 File   : SerializedEntity.h
 Desc   : Entityから取得したComponentの名前と値を所有する。

------------------------------------------------------------------------------

 Date   : 2026/11/19
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _SERIALIZED_ENTITY_H_
#define _SERIALIZED_ENTITY_H_

#include <cstdint>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include "Common/Include/EntityHandle.h"
#include "Engine/Include/Asset/AssetHandle.h"

namespace Hestia
{
    struct SerializedValue
    {
        using Array = std::vector<SerializedValue>;

        std::variant<bool, int64_t, uint64_t, double, std::string, Array, AssetRef> m_value;
    };

    struct SerializedComponent
    {
        std::string m_typeName;
        std::unordered_map<std::string, SerializedValue> m_fields;
    };

    /// @brief EntityのComponent構成と保存フィールド。階層を保持する場合は子を所有する
    struct SerializedEntity
    {
        EntityHandle m_source;
        std::vector<SerializedComponent> m_components;
        std::vector<SerializedEntity> m_children;
        bool m_active = true;
        std::vector<AssetRef> m_scripts;
    };
}

#endif // _SERIALIZED_ENTITY_H_
