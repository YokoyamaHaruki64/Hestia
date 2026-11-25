/*=============================================================================

 File   : GameSetup.h
 Desc   : GameCoreが生成されたScript型登録を呼ぶ契約を定義する。

------------------------------------------------------------------------------

 Date   : 2026/11/19
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _GAME_SETUP_H_
#define _GAME_SETUP_H_

namespace HestiaGame::Setup
{
    /// @brief Game で使用する Script 型を登録する。失敗時は false。
    bool RegisterScripts();

}

#endif // _GAME_SETUP_H_
