/*=============================================================================

 File   : EngineAPI.h
 Desc   : Application と Engine.dll の間で共有する API テーブルを定義する。

------------------------------------------------------------------------------

 Date   : 2026/10/06
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _ENGINE_API_H_
#define _ENGINE_API_H_

#include "Common/Include/DllAPI.h"
#include "Common/Include/WindowsHeaders.h"
#include "Application/Include/ApplicationAPI.h"

namespace Hestia
{
    /// @brief Engine.dll 内の Engine 実体を指す不透明なハンドル。
    using EngineHandle = void*;

    /// @brief Application と Engine.dll の間で共有する関数テーブル。
    struct EngineAPI
    {
        /// @brief Engine を生成して初期化する。
        EngineHandle (*Create)(const ApplicationAPI* applicationAPI);

        /// @brief Engine を終了処理して破棄する。
        void (*Destroy)(EngineHandle engine);

        /// @brief Window Message を Engine へ渡す。
        void (*ProcessMessage)(EngineHandle engine, HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

        /// @brief 固定更新を Engine に渡す。
        void (*FixedUpdate)(EngineHandle engine, float deltaTime);

        /// @brief フレーム更新を Engine に渡す。
        void (*FrameExecute)(EngineHandle engine, float deltaTime);
    };
}

/// @brief Engine.dll から Engine API テーブルを取得する。
/// @return Engine API テーブルへのポインタ。
extern "C" HESTIA_ENGINE_API const Hestia::EngineAPI* GetEngineAPI();

#endif // _ENGINE_API_H_
