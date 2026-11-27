/*=============================================================================

 File   : SceneAPI.h
 Desc   : GameRuntime に Scene の取得と切り替え予約を公開する。

------------------------------------------------------------------------------

 Date   : 2026/11/26
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _SCENE_API_H_
#define _SCENE_API_H_

#include <string_view>

namespace Hestia
{
    /// @brief GameRuntime に渡す Scene の操作テーブル。
    struct SceneAPI
    {
        /// @brief Engine 側で構成した操作テーブルを返す。
        static const SceneAPI& Get() noexcept;

        /// @brief 現在の相対パスから拡張子を除いたファイル名を借用する。
        std::string_view (*GetCurrentSceneName)() noexcept = nullptr;
        /// @brief 復元が完了した Scene の Asset ルート相対パスを借用する。
        std::string_view (*GetCurrentScenePath)() noexcept = nullptr;

        /// @brief Asset ルート相対パスで切り替えを予約する。
        /// @return 切り替え要求を受理した場合は true。
        bool (*LoadScene)(std::string_view path) = nullptr;
    };
}

#endif // _SCENE_API_H_
