/*=============================================================================

 File   : Engine.h
 Desc   : Engine クラスと所有するサブシステムの更新処理を宣言する。

------------------------------------------------------------------------------

 Date   : 2026/10/06
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _ENGINE_H_
#define _ENGINE_H_

#include "EngineAPI.h"
#include "Time/TimeSystem.h"
#include "Time/TimeAPI.h"
#include "Log/LogSystem.h"
#include "Log/LogAPI.h"

namespace Hestia
{
    /// @brief System の更新と Application からの通知を取りまとめる Engine。
    /// 
    /// EngineAPI の入口だけがこの型を保持し、共有 Header には実体を公開しない。
    /// ログの受付・出力と時間値の更新を管理する。
    class Engine
    {
    public:
        Engine() = default;

        /// @brief Application API を保持し、Engine を利用可能にする。
        /// @param applicationAPI Application から渡された共有 API。
        /// @return 初期化に成功した場合は true。
        bool Initialize(const ApplicationAPI& applicationAPI);

        /// @brief Engine が保持する機能を終了する
        void Finalize();

        /// @brief Window Message を有効な System へ配送する。
        /// @param hwnd メッセージを受け取った Window。
        /// @param message Window Message 識別子。
        /// @param wParam メッセージの追加情報。
        /// @param lParam メッセージの追加情報。
        void ProcessMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

        /// @brief 固定刻みの更新を有効な System へ配送する。
        /// @param deltaTime 固定更新の経過時間（秒）。
        void FixedUpdate(float deltaTime);

        /// @brief フレーム更新を有効な System へ配送する。
        /// @param deltaTime フレームの経過時間（秒）。
        void FrameExecute(float deltaTime);

    private:
        ApplicationAPI m_applicationAPI;

        LogSystem m_log;
        LogAPI m_logAPI;

        TimeSystem m_time;
        TimeAPI m_timeAPI;

        /*
        // System と Boundary API は各 System の設計・実装時に有効化する。
        AssetSystem m_assets;
        GraphicsSystem m_graphics;
        InputSystem m_input;
        AudioSystem m_audio;
        PhysicsSystem m_physics;

        AssetAPI m_assetAPI;
        GraphicsAPI m_graphicsAPI;
        InputAPI m_inputAPI;
        AudioAPI m_audioAPI;
        PhysicsAPI m_physicsAPI;

        HMODULE m_gameModule = nullptr;
        GameEngineAPI m_gameEngineAPI;
        const GameRuntimeAPI* m_gameRuntimeAPI = nullptr;
        void* m_gameRuntime = nullptr;

        bool LoadGame();
        void UnloadGame();
        GameEngineAPI CreateGameEngineAPI();
        */
    };
}

#endif // _ENGINE_H_
