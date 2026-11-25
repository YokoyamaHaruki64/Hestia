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
#include "AssetIdentity.h"
#include "EntityHandle.h"

namespace Hestia
{
    struct GameEngineAPI;

    using ScriptAttachmentVisitor = void (*)(void*, EntityHandle, AssetGuid);
    using ScriptAttachmentQuery = bool (*)(void*, const EntityHandle*, size_t, void*, ScriptAttachmentVisitor);

    enum class SceneScriptResult { Attached, UnknownType, Failed };

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
        /// @brief Engine所有の出力に、反映済みのScript型参照を渡す。
        ScriptAttachmentQuery QueryScriptAttachments = nullptr;
        /// @brief GUIDから既定のScriptを生成し、追加を予約する。
        SceneScriptResult (*AttachSceneScript)(void*, EntityHandle, AssetGuid) = nullptr;
        /// @brief 新SceneのScriptの追加を確定する。
        bool (*FlushScripts)(void*) = nullptr;
        /// @brief 登録を維持したまま旧SceneのScriptを終了して解放する。
        void (*ClearSceneScripts)(void*) = nullptr;
    };
}

/// @brief Game の生存中に利用できる Runtime API テーブルを返す。
extern "C" HESTIA_GAME_API const Hestia::GameRuntimeAPI* GetGameRuntimeAPI();

#endif // _GAME_RUNTIME_API_H_
