/*=============================================================================

 File   : AssetDB.cpp
 Desc   : .metaを走査してAssetのGUIDとPathを索引化する。

------------------------------------------------------------------------------

 Date   : 2026/10/15
 Author : Yokoyama Haruki

=============================================================================*/

#include "pch.h"
#include "Common/Include/StringUtils.h"
#include "Asset/AssetDB.h"
#include "Log/Logger.h"

namespace Hestia
{
    bool AssetDB::Initialize(const std::filesystem::path& assetsRoot)
    {
        // 走査の結果が揃ってから、検索用の索引を組み立てる
        std::vector<AssetInfo> scanned;
        std::vector<std::string> warnings;
        std::string error;
        const bool success = AssetMetadata::Scan(assetsRoot, {}, scanned, warnings, error);
        for (const auto& warning : warnings)
            Logger::Warning(warning);
        if (!success)
        {
            Logger::Error("AssetDB " + error);
            return false;
        }

        decltype(m_entries) entries;
        decltype(m_paths) paths;
        for (auto& info : scanned)
        {
            if (!paths.emplace(ToLowerAscii(info.m_path), info.m_guid).second ||
                !entries.emplace(info.m_guid, std::move(info)).second)
            {
                Logger::Error("AssetDB could not index duplicate asset metadata");
                return false;
            }
        }

        m_root = assetsRoot;
        m_entries.swap(entries);
        m_paths.swap(paths);
        return true;
    }

    const AssetInfo* AssetDB::FindByGuid(const AssetGuid& guid) const
    {
        const auto found = m_entries.find(guid);
        return found == m_entries.end() ? nullptr : &found->second;
    }

    const AssetInfo* AssetDB::FindByPath(std::string_view path) const
    {
        const auto normalized = PathFromUtf8(path).lexically_normal();
        const auto found = m_paths.find(ToLowerAscii(PathToUtf8(normalized)));
        return found == m_paths.end() ? nullptr : FindByGuid(found->second);
    }

    std::string AssetDB::SourcePath(const AssetInfo& info) const
    {
        return PathToUtf8(m_root / PathFromUtf8(info.m_path));
    }

    std::filesystem::path AssetDB::SourcePath(std::string_view relativePath) const
    {
        const auto path = PathFromUtf8(relativePath).lexically_normal();
        if (m_root.empty() || path.empty() || path.has_root_path() || *path.begin() == L"..")
            return {};

        return m_root / path;
    }

    bool AssetDB::RegisterFile(std::string_view relativePath)
    {
        const auto source = SourcePath(relativePath);
        if (source.empty())
            return false;

        const auto path = PathToUtf8(source.lexically_relative(m_root));
        if (FindByPath(path))
            return true;

        // メタデータを確定し、GUIDとパスの両方で引ける状態にする
        AssetInfo info;
        std::string error;
        if (!AssetMetadata::Ensure(source, info, error))
        {
            Logger::Error("AssetDB " + error);
            return false;
        }
        if (m_entries.contains(info.m_guid))
        {
            Logger::Error("AssetDB duplicate GUID for '" + path + "'");
            return false;
        }

        info.m_path = path;
        const auto guid = info.m_guid;
        m_entries.emplace(guid, std::move(info));
        m_paths.emplace(ToLowerAscii(path), guid);
        return true;
    }

    bool AssetDB::SaveModelMeta(const AssetInfo& previous, const AssetInfo& updated) const
    {
        if (previous.m_subAssets == updated.m_subAssets && previous.m_materials == updated.m_materials)
            return true;

        std::string error;
        if (AssetMetadata::SaveModel(PathFromUtf8(SourcePath(updated)), updated, error))
            return true;

        Logger::Error("AssetDB " + error);
        return false;
    }
}
