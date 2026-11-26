/*=============================================================================

 File   : GameScene.h
 Desc   : Game Script へ現在の Scene と切り替え予約を公開する。

------------------------------------------------------------------------------

 Date   : 2026/11/26
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _HESTIA_GAME_SCENE_H_
#define _HESTIA_GAME_SCENE_H_

#include <string_view>

#include "Engine/Include/API/SceneAPI.h"

namespace HestiaGame
{
    namespace Detail { class GameRuntime; }

    /// @brief Engine が所有する Scene の取得と切り替え予約の Facade。
    /// @note GameRuntime と同じスレッドで使用する。取得した名前とパスは切り替え完了か Runtime 終了まで有効。
    class Scene
    {
    public:
        /// @brief 現在の Scene のファイル名から拡張子を除いた名前を返す。Scene の復元前は空。
        static std::string_view GetCurrentSceneName() noexcept
        {
            return s_api ? s_api->GetCurrentSceneName() : std::string_view{};
        }

        /// @brief 現在の Scene の Asset ルートからの相対パスを返す。Scene の復元前は空。
        static std::string_view GetCurrentScenePath() noexcept
        {
            return s_api ? s_api->GetCurrentScenePath() : std::string_view{};
        }

        /// @brief フレーム末尾の Scene 切り替えを予約する。
        /// @param path Asset ルートからの相対パス。
        /// @return 切り替え要求を受理した場合は true。
        static bool LoadScene(std::string_view path)
        {
            return s_api && s_api->LoadScene(path);
        }

    private:
        friend class Detail::GameRuntime;

        static void Bind(const Hestia::SceneAPI* api) noexcept { s_api = api; }
        static void Unbind() noexcept { s_api = nullptr; }

        inline static const Hestia::SceneAPI* s_api = nullptr;
    };
}

#endif // _HESTIA_GAME_SCENE_H_
