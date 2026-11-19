/*=============================================================================

 File   : GameSetup.h
 Desc   : GameCore が Game の Script 登録と初期配置を呼ぶ契約を定義する。

------------------------------------------------------------------------------

 Date   : 2026/11/19
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _GAME_SETUP_H_
#define _GAME_SETUP_H_

#include <functional>

namespace HestiaGame::Setup
{
    /// @brief Game で使用する Script 型を登録する。失敗時は false。
    bool RegisterScripts();

    /// @brief 初期シーンに Script を配置する。失敗時は false。
    /// @param applyScripts 大量追加時に保留中の Script を反映する。SetupScene の呼び出し中だけ利用できる。
    bool SetupScene(const std::function<void()>& applyScripts);
}

#endif // _GAME_SETUP_H_
