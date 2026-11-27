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
        /// @brief 現在の Scene と保留中の保存・切り替え要求をリセットする。
        static void Finalize() noexcept;

        /// @brief Assetルート相対パスへ保存を予約する。
        /// @param path 保存先。空なら現在のScene、または新規保存先を使う。
        static bool SaveScene(std::string_view path = {});
        /// @brief Assetルート相対パスで切り替え要求を更新する。
        static bool LoadScene(std::string_view path);
        /// @brief フレーム末尾の反映後に、保留された保存を実行する。
        static bool SerializeScene();
        /// @brief 保留されたSceneを読み、旧Sceneと入れ替える。
        /// @return 要求なし、または復元成功時にtrue。
        static bool CheckScene(const AssetDB& database, const GameRuntimeAPI& runtimeAPI, void* runtime);
        /// @brief 現在の相対パスから拡張子を除いたファイル名を借用する。
        static std::string_view GetCurrentSceneName() noexcept;
        /// @brief 最後に復元まで完了したSceneのパス。
        static std::string_view GetCurrentScenePath() noexcept { return s_currentScenePath; }
        /// @brief 最後に保存が成功したSceneの相対パス。保存前は空。
        static std::string_view GetLastSavedScenePath() noexcept { return s_lastSavedScenePath; }

    private:
        SceneSystem() = delete;

        static std::string s_currentScenePath;
        static std::string s_nextScenePath;
        static std::string s_saveScenePath;
        static std::string s_lastSavedScenePath;
        static bool s_saveRequested;
    };
}

#endif // _SCENE_SYSTEM_H_
