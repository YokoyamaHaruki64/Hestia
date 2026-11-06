/*=============================================================================

 File   : LogStorage.h
 Desc   : ログの受付キューと処理キューの同期・容量制限を管理する。

------------------------------------------------------------------------------

 Date   : 2026/10/08
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _LOG_STORAGE_H_
#define _LOG_STORAGE_H_

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <queue>
#include <vector>

#include "Log/LogData.h"

namespace Hestia
{
    /// @brief 未処理ログの保持先
    /// @note TransferPending／Drain は単一の処理スレッドだけが呼ぶ
    class LogStorage
    {
    public:

        /// @brief 保持容量を設定する。受付は閉じたままにする
        /// @param storageCapacity pending と出力待ち queue の合計上限。0 は無効。
        /// @return 成功: true。初期化済みor失敗: false。
        bool Initialize(std::size_t storageCapacity);

        /// @brief 保持中のログと設定を解除する。終了済みの場合は何もしない。
        /// @pre 記録元と処理スレッドが停止している。
        void Finalize();

        /// @brief storageCapacity > 0のとき受付を開始する
        void Open();

        /// @brief 受付を閉じる。戻った後の Push は失敗する。
        void Close();

        /// @brief 受付状態を取得
        /// @return 受付中: true
        bool IsAccepting() const;

        /// @brief LogEntry を書き込み側 pending に追加する。満杯なら破棄する。
        /// @param entry 書き込み側 pending に追加するLogEntry
        /// @return 追加成功時: true / 閉鎖中または満杯: false
        /// @note 満杯では重要度に関係なく新着を破棄し、破棄件数を保持する。
        bool Push(LogEntry entry);

        /// @brief 読み取り側 pending から出力待ち queue へ移送する。
        /// @param maxCount 一回の最大移送件数。0 では何もしない
        /// @return 移送した件数
        std::size_t TransferPending(std::size_t maxCount);

        /// @brief 出力待ち queue の先頭から取り出し、entries の末尾に追加する。
        /// @param entries LogEntry の格納先。呼び出し中だけ借用する
        /// @param maxCount 一回の最大取り出し件数。
        /// @return 取り出した件数
        std::size_t Drain(std::vector<LogEntry>& entries, std::size_t maxCount);

        /// @brief pending と出力待ち queue がすべて空かを調べる。
        /// @return 未処理の保持件数が0: true
        /// @note Drainで取り出された後はstorageの管理下にない
        bool IsEmpty() const;

    private:
        // 書き込みと読み取りを入れ替える
        bool SwitchPending();

        std::array<std::queue<LogEntry>, 2> m_pending;
        std::size_t m_writeIndex = 0;
        std::size_t m_readIndex = 1;
        std::queue<LogEntry> m_queue;
        mutable std::mutex m_pendingMutex;  // pending の書き込みと読み取りの排他制御

        std::size_t m_storageCapacity = 0;
        std::atomic<std::size_t> m_bufferedCount = 0;
        std::uint64_t m_droppedCount = 0;
        bool m_accepting = false;
    };
}

#endif // _LOG_STORAGE_H_
