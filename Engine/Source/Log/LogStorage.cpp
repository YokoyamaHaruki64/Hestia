/*=============================================================================

 File   : LogStorage.cpp
 Desc   : pending の切り替えとログの受付・移送・取り出しを実装する。

------------------------------------------------------------------------------

 Date   : 2026/10/08
 Author : Yokoyama Haruki

=============================================================================*/

#include "pch.h"
#include "Log/LogStorage.h"

namespace Hestia
{
    bool LogStorage::Initialize(std::size_t storageCapacity)
    {
        std::lock_guard lock(m_pendingMutex);

        if (storageCapacity == 0 || m_storageCapacity != 0)
            return false;

        m_storageCapacity = storageCapacity;
        m_writeIndex = 0;
        m_readIndex = 1;
        m_bufferedCount.store(0);
        m_droppedCount = 0;
        m_accepting = false;
        return true;
    }

    void LogStorage::Finalize()
    {
        std::lock_guard lock(m_pendingMutex);
        m_accepting = false;

        // pending と queue の中身をすべて破棄する
        for (auto& pending : m_pending)
        {
            while (!pending.empty())
            {
                pending.pop();
            }
        }

        while (!m_queue.empty())
        {
            m_queue.pop();
        }

        m_storageCapacity = 0;
        m_bufferedCount.store(0);
        m_droppedCount = 0;
        m_writeIndex = 0;
        m_readIndex = 1;
    }

    void LogStorage::Open()
    {
        std::lock_guard lock(m_pendingMutex);
        assert(m_storageCapacity != 0);
        m_accepting = true;
    }

    void LogStorage::Close()
    {
        std::lock_guard lock(m_pendingMutex);
        m_accepting = false;
    }

    bool LogStorage::IsAccepting() const
    {
        std::lock_guard lock(m_pendingMutex);
        return m_accepting;
    }

    bool LogStorage::Push(LogEntry entry)
    {
        std::lock_guard lock(m_pendingMutex);
        if (!m_accepting)
            return false;

        // pending と queue の合計件数が上限を超える場合は破棄する
        if (m_bufferedCount.load() >= m_storageCapacity)
        {
            ++m_droppedCount;
            return false;
        }

        m_pending[m_writeIndex].push(std::move(entry));
        // index の交換前に保持件数を反映し、処理側への公開と整合させる。
        m_bufferedCount.fetch_add(1);
        return true;
    }

    bool LogStorage::SwitchPending()
    {
        std::lock_guard lock(m_pendingMutex);
        if (!m_pending[m_readIndex].empty() || m_pending[m_writeIndex].empty())
            return false;

        std::swap(m_writeIndex, m_readIndex);
        return true;
    }

    std::size_t LogStorage::TransferPending(std::size_t maxCount)
    {
        if (maxCount == 0)
            return 0;

        // 読み取り側が空のとき、書き込み側と入れ替えを試みる。書き込み側が空なら何もしない。
        if (m_pending[m_readIndex].empty())
            SwitchPending();

        auto& pending = m_pending[m_readIndex];
        std::size_t transferred = 0;

        // 移送
        while (transferred < maxCount && !pending.empty())
        {
            m_queue.push(std::move(pending.front()));
            pending.pop();
            ++transferred;
        }

        return transferred;
    }

    std::size_t LogStorage::Drain(std::vector<LogEntry>& entries, std::size_t maxCount)
    {
        std::size_t drained = 0;
        while (drained < maxCount && !m_queue.empty())
        {
            entries.push_back(std::move(m_queue.front()));
            m_queue.pop();
            ++drained;
        }

        m_bufferedCount.fetch_sub(drained);
        return drained;
    }

    bool LogStorage::IsEmpty() const
    {
        // queue を処理側以外から直接読まず、全 queue の合計件数で判断する。
        return m_bufferedCount.load() == 0;
    }
}
