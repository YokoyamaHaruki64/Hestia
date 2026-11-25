/*=============================================================================

 File   : SceneSystem.h
 Desc   : フレーム境界でのScene保存と切り替えを管理する。

------------------------------------------------------------------------------

 Date   : 2026/11/25
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _SCENE_SYSTEM_H_
#define _SCENE_SYSTEM_H_

#include <string>
#include <string_view>

namespace Hestia
{
    class AssetDB;
    struct GameRuntimeAPI;

    /// @brief 同一World上で現在のSceneと保留要求を管理する。
    class SceneSystem
    {
    public:
        /// @brief Assetルート相対パスへ保存を予約する。
        bool SaveScene(std::string_view path);
        /// @brief Assetルート相対パスで切り替え要求を更新する。
        bool LoadScene(std::string_view path);
        /// @brief フレーム末尾の反映後に、保留された保存を実行する。
        bool SerializeScene(AssetDB& database);
        /// @brief 保留されたSceneを読み、旧Sceneと入れ替える。
        /// @return 要求なし、または復元成功時にtrue。
        bool CheckScene(const AssetDB& database, const GameRuntimeAPI& runtimeAPI, void* runtime);
        /// @brief 最後に復元まで完了したSceneのパス。
        std::string_view GetCurrentScenePath() const noexcept { return m_currentScenePath; }

    private:
        std::string m_currentScenePath;
        std::string m_nextScenePath;
        std::string m_saveScenePath;
        bool m_saveRequested = false;
    };
}

#endif // _SCENE_SYSTEM_H_
