/*=============================================================================

 File   : GameRuntimeAPI.h
 Desc   : Engine と Game の間で使う関数テーブルを定義する。

------------------------------------------------------------------------------

 Date   : 2026/11/13
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _GAME_RUNTIME_API_H_
#define _GAME_RUNTIME_API_H_

#include "DllAPI.h"

namespace Hestia
{
    struct GameEngineAPI;

    /// @brief Game 内の Runtime を Engine から操作する関数テーブル。
    struct GameRuntimeAPI
    {
        /// @brief 初期化済み Runtime の不透明ハンドルを返す。失敗時は nullptr。
        void* (*Create)(const GameEngineAPI*) = nullptr;
        /// @brief Script を終了してから、指定した Runtime を破棄する。
        void (*Destroy)(void*) = nullptr;
        void (*FixedUpdate)(void*) = nullptr;
        void (*Update)(void*) = nullptr;
        void (*LateUpdate)(void*) = nullptr;
        /// @brief ECS の実体破棄前に Script の寿命と次フレームの実行対象を確定する。
        void (*EndFrame)(void*) = nullptr;
    };
}

/// @brief Game の生存中に利用できる Runtime API テーブルを返す。
extern "C" HESTIA_GAME_API const Hestia::GameRuntimeAPI* GetGameRuntimeAPI();

#endif // _GAME_RUNTIME_API_H_
