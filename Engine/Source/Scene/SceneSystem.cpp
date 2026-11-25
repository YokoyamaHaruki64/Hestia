/*=============================================================================

 File   : SceneSystem.cpp
 Desc   : Sceneのスナップショット、全Entityの入れ替え、Script復元を調停する。

------------------------------------------------------------------------------

 Date   : 2026/11/25
 Author : Yokoyama Haruki

=============================================================================*/

#include "pch.h"
#include "Scene/SceneSystem.h"

#include "Asset/AssetDB.h"
#include "Common/Include/GameRuntimeAPI.h"
#include "Common/Include/StringUtils.h"
#include "ECS/ECSSystem.h"
#include "Log/Logger.h"
#include "Serialize/EntityYamlReader.h"
#include "Serialize/EntityYamlWriter.h"
#include "Transform/TransformData.h"

#include <array>
#include <utility>

namespace Hestia
{
    namespace
    {
        std::string NormalizeScenePath(std::string_view input)
        {
            const auto path = PathFromUtf8(input).lexically_normal();
            if (path.empty() || path.has_root_path() || *path.begin() == L".." ||
                ToLowerAscii(PathToUtf8(path.extension())) != ".scene")
                return {};

            return PathToUtf8(path);
        }
    }

    bool SceneSystem::SaveScene(std::string_view path)
    {
        auto normalized = NormalizeScenePath(path);
        if (normalized.empty())
            return false;

        m_saveScenePath = std::move(normalized);
        m_saveRequested = true;
        return true;
    }

    bool SceneSystem::LoadScene(std::string_view path)
    {
        auto normalized = NormalizeScenePath(path);
        if (normalized.empty())
            return false;

        m_nextScenePath = std::move(normalized);
        return true;
    }

    bool SceneSystem::SerializeScene(AssetDB& database)
    {
        if (!std::exchange(m_saveRequested, false))
            return true;

        const auto path = std::exchange(m_saveScenePath, {});
        const auto fail = [&](std::string_view reason)
        {
            Logger::Error("Could not save Scene '" + path + "': " + std::string(reason));
            return false;
        };
        const auto* registry = ECSSystem::GetRegistry();
        const auto output = database.SourcePath(path);
        if (!registry || output.empty())
            return fail("Scene storage is unavailable");

        // 反映済みのWorldをChunk単位で読み、ルートと照会用のHandleを同時に集める
        const std::array required{ *registry->GetId<TransformHierarchy>() };
        std::vector<EntityHandle> roots;
        std::vector<EntityHandle> entities;
        for (const auto* archetype : ECSSystem::QueryArchetypes(required))
        {
            const auto column = archetype->FindColumn(required.front());
            for (size_t index = 0; index < archetype->GetChunkCount(); ++index)
            {
                const auto& chunk = archetype->GetChunk(index);
                const auto rows = chunk.GetEntities();
                for (size_t row = 0; row < rows.size(); ++row)
                {
                    const auto handle = rows[row].GetHandle();
                    entities.push_back(handle);
                    const auto* hierarchy = static_cast<const TransformHierarchy*>(chunk.GetElement(column, row));
                    if (!hierarchy->m_parent.IsValid())
                        roots.push_back(handle);
                }
            }
        }

        // Scene全体のScriptを一度照会し、各ルートの保存へ同じ結果を渡す
        const auto scripts = ECSSystem::QueryScriptAttachments(entities);
        if (!scripts)
            return fail("Script attachments are unavailable");
        std::vector<SerializedEntity> snapshots;
        snapshots.reserve(roots.size());
        for (const auto root : roots)
        {
            auto snapshot = ECSSystem::SaveEntity(root, *scripts);
            if (!snapshot)
                return fail("Could not capture Entity hierarchy");
            snapshots.push_back(std::move(*snapshot));
        }

        // 書き込みが完了してから保存先を置き換え、即時ロード用の索引を登録する
        std::error_code error;
        std::filesystem::create_directories(output.parent_path(), error);
        if (error)
            return fail(error.message());
        auto temporary = output;
        temporary += L".tmp";
        if (!EntityYamlWriter::SaveScene(snapshots, temporary) ||
            !MoveFileExW(temporary.c_str(), output.c_str(), MOVEFILE_REPLACE_EXISTING))
        {
            std::filesystem::remove(temporary, error);
            return fail("Could not write Scene file");
        }
        return database.RegisterFile(path) || fail("Could not register Scene metadata");
    }

    bool SceneSystem::CheckScene(const AssetDB& database, const GameRuntimeAPI& runtimeAPI, void* runtime)
    {
        auto path = std::exchange(m_nextScenePath, {});
        if (path.empty())
            return true;

        const auto fail = [&](std::string_view reason)
        {
            Logger::Error("Could not restore Scene '" + path + "': " + std::string(reason));
            return false;
        };

        // AssetDBでは索引だけを引き、復元元は要求のたびにファイルから読み直す
        const auto* info = database.FindByPath(path);
        if (!info || info->m_type != AssetType::Scene)
            return fail("Scene path is not registered");
        const auto scene = EntityYamlReader::LoadScene(PathFromUtf8(database.SourcePath(*info)));
        if (!scene)
            return fail("Invalid Scene data");

        // 旧Scriptを終了する間は旧Entityを保持し、その後に全Entityを入れ替える
        if (!ECSSystem::ClearSceneEntities(runtime, runtimeAPI.ClearSceneScripts))
            return fail("Could not clear the previous Scene");
        std::vector<SceneScriptAttachment> scripts;
        if (!ECSSystem::InstantiateScene(*scene, scripts))
            return fail("Could not instantiate Entities");

        // 他のEntityや親子関係が使える状態でScriptを生成する
        for (const auto& script : scripts)
        {
            const auto result = runtimeAPI.AttachSceneScript(runtime, script.m_entity, script.m_guid);
            if (result == SceneScriptResult::UnknownType)
            {
                Logger::Error("Scene '" + path + "': Entity " + std::to_string(script.m_entity.m_index) +
                    " references unregistered Script " + script.m_guid.ToString());
            }
            else if (result == SceneScriptResult::Failed)
                return fail("Could not attach Script " + script.m_guid.ToString());
        }
        if (!runtimeAPI.FlushScripts(runtime))
            return fail("Could not commit Script attachments");

        m_currentScenePath = std::move(path);
        return true;
    }
}
