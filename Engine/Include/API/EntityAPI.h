/*=============================================================================

 File   : EntityAPI.h
 Desc   : Engine が GameRuntime へ公開する Entity と Process の操作を定義する。

------------------------------------------------------------------------------

 Date   : 2026/11/17
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _ENTITY_API_H_
#define _ENTITY_API_H_

#include <cstddef>

#include "Common/Include/EntityHandle.h"

namespace Hestia
{
    /// @brief Game 向け Facade が利用できる登録済み Process。
    struct ProcessHandle
    {
        static constexpr std::size_t INVALID_INDEX = static_cast<std::size_t>(-1);
        std::size_t m_index = INVALID_INDEX;

        /// @brief 登録時に発行されたハンドルか確認する。
        bool IsValid() const noexcept { return m_index != INVALID_INDEX; }
    };

    /// @brief Engine の検索作業配列から借用する結果。
    /// @note 次の複数件検索、明示クリア、または Engine 終了まで有効。
    struct EntitySearchResult
    {
        const EntityHandle* data = nullptr;
        std::size_t size = 0;
    };

    /// @brief Engine が所有する Entity の操作。Handle、値、検索結果の借用を受け渡す。
    /// @note すべての操作は Engine の更新と同じスレッドで呼ぶ。
    struct EntityAPI
    {
        /// @brief 成功時は生成済みまたは生成待ちの Handle、失敗時は無効 Handle を返す。
        EntityHandle (*CreateEntity)() = nullptr;
        /// @brief Entity の破棄を予約する。
        void (*DestroyEntity)(EntityHandle) = nullptr;
        /// @brief 生成済みかつ破棄予約前なら true を返す。
        bool (*Exists)(EntityHandle) = nullptr;
        /// @brief 生存中かつ更新対象なら true を返す。
        bool (*IsActive)(EntityHandle) = nullptr;
        /// @brief 生存中の Entity の Active 設定を変更する。設定済みの値も成功とする。
        bool (*SetActive)(EntityHandle, bool) = nullptr;
        /// @brief 名前を UTF-8 のポインタとバイト数で指定する。成功時は true。
        bool (*SetName)(EntityHandle, const char*, std::size_t) = nullptr;
        /// @brief タグを設定する。成功時は true。
        bool (*SetTag)(EntityHandle, uint32_t) = nullptr;
        /// @brief 名前に一致する最初の生存中の Entity を返す。一致なしは無効 Handle。
        EntityHandle (*FindFirstEntityByName)(const char*, std::size_t) = nullptr;
        /// @brief タグが一致する全 Entity の検索結果を借用する。
        EntitySearchResult (*FindEntitiesByTag)(uint32_t) = nullptr;
        /// @brief 登録済み Process の要求 Component を追加する。受付時は true。
        bool (*AddProcess)(ProcessHandle, EntityHandle) = nullptr;
        /// @brief Process に一致する全 Entity の検索結果を借用する。
        EntitySearchResult (*FindEntitiesWithProcess)(ProcessHandle) = nullptr;
        /// @brief 検索結果の借用を終了する。作業配列の容量は再利用する。
        void (*ClearEntitySearchResults)() = nullptr;
        /// @brief フレーム末尾の破棄予約を添字で読み取る。末尾に達したら false。
        bool (*TryGetPendingDestroy)(std::size_t, EntityHandle*) = nullptr;
        /// @brief Entity の Active 設定の変更通知先を設定する。nullptr で解除する。
        void (*SetActiveChangedCallback)(void*, void (*)(void*, EntityHandle, bool)) = nullptr;
    };
}

#endif // _ENTITY_API_H_
