/*=============================================================================

 File   : AssetDB.cpp
 Desc   : Asset 名と登録情報の索引を管理する。

------------------------------------------------------------------------------

 Date   : 2026/10/15
 Author : Yokoyama Haruki

=============================================================================*/

#include "pch.h"
#include "Asset/AssetDB.h"

namespace Hestia
{
    bool AssetDB::Initialize(EntryMap entries)
    {
        EntryMap normalizedEntries;
        normalizedEntries.reserve(entries.size());

        for (auto& [name, info] : entries)
        {
            if (name.empty() || info.m_path.empty())
                return false;

            switch (info.m_type)
            {
            case AssetType::Mesh:
            case AssetType::Texture:
                break;
            default:
                return false;
            }

            std::string normalizedName = NormalizeName(name);
            if (!normalizedEntries.emplace(std::move(normalizedName), std::move(info)).second)
                return false;
        }

        m_entries.swap(normalizedEntries);
        return true;
    }

    const AssetInfo* AssetDB::Find(std::string_view name) const
    {
        if (name.empty())
            return nullptr;

        const auto found = m_entries.find(NormalizeName(name));
        if (found == m_entries.end())
            return nullptr;

        return &found->second;
    }

    bool AssetDB::Contains(std::string_view name) const
    {
        return Find(name) != nullptr;
    }

    std::string AssetDB::NormalizeName(std::string_view name)
    {
        std::string normalized(name);
        for (char& character : normalized)
        {
            if (character >= 'A' && character <= 'Z')
                character += 'a' - 'A';
        }

        return normalized;
    }
}
