/*=============================================================================

 File   : LogSystem.cpp
 Desc   : LogSystem の初期化、処理スレッド、ログ出力先を管理する。

------------------------------------------------------------------------------

 Date   : 2026/10/08
 Author : Yokoyama Haruki

=============================================================================*/

#include "pch.h"
#include "Log/LogSystem.h"

#include <ctime>
#include <iomanip>
#include <sstream>

namespace Hestia
{
    LogSystem* LogSystem::s_instance = nullptr;

    bool LogSystem::CreateDefaultSettings(LogSettings& settings) const
    {
        try
        {
            // Logs フォルダを作成する。存在してもエラーにならない。
            const std::filesystem::path logDirectory = "../Logs";
            std::filesystem::create_directories(logDirectory);

            // 時間から一意のファイル名を生成する
            auto timestamp = std::chrono::system_clock::now();
            for (int attempt = 0; attempt < 10000; ++attempt)
            {
                // 時刻の秒と100μs単位のサブ秒を取得する
                const auto seconds = std::chrono::floor<std::chrono::seconds>(timestamp);
                const auto subsecond = std::chrono::duration_cast<std::chrono::microseconds>(
                    timestamp - seconds).count();

                const auto tick = subsecond / 100;
                const std::time_t time = std::chrono::system_clock::to_time_t(seconds);

                std::tm localTime = {};
                if (localtime_s(&localTime, &time) != 0)
                    return false;

                // ファイル名を生成する
                std::ostringstream fileName;
                fileName << std::put_time(&localTime, "%Y%m%d_%H%M%S_")
                    << std::setfill('0') << std::setw(4) << tick << ".txt";
                const auto filePath = logDirectory / fileName.str();
                const HANDLE file = CreateFileW(filePath.c_str(), GENERIC_WRITE, 0, nullptr,
                    CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);

                // ファイルが作成できた場合は設定を更新して成功
                if (file != INVALID_HANDLE_VALUE)
                {
                    if (!CloseHandle(file))
                        return false;

                    settings.m_filePath = filePath;
                    settings.m_storageCapacity = 4096;
                    settings.m_drainCount = 256;
                    return true;
                }

                // ファイルが作成できなかった場合
                // 既存ファイルとの衝突の場合は時刻を進めて再試行する。その他のエラーは失敗とする。
                const DWORD error = GetLastError();
                if (error != ERROR_FILE_EXISTS && error != ERROR_ALREADY_EXISTS)
                {
                    return false;
                }
                timestamp += std::chrono::microseconds(100);
            }
        }
        catch (const std::filesystem::filesystem_error&)
        {
            return false;
        }

        return false;
    }

    bool LogSystem::Initialize()
    {
        LogSettings settings;
        if (!CreateDefaultSettings(settings))
            return false;

        return Initialize(settings);
    }

    bool LogSystem::Initialize(const LogSettings& settings)
    {
        if (m_thread.joinable() || s_instance != nullptr)
            return false;

        // 設定の妥当性を確認する
        if (settings.m_filePath.empty() ||
            settings.m_storageCapacity == 0 ||
            settings.m_drainCount == 0 ||
            settings.m_drainCount > settings.m_storageCapacity)
            return false;

        if (!m_storage.Initialize(settings.m_storageCapacity))
            return false;

        try
        {
            if (!m_process.Initialize(settings.m_filePath))
            {
                Finalize();
                return false;
            }

            m_drainCount = settings.m_drainCount;
            m_entries.reserve(m_drainCount);
            m_storage.Open();
            m_thread = std::thread(&LogSystem::Run, this);

            if (!m_logger.Initialize(&m_storage))
            {
                Finalize();
                return false;
            }

            s_instance = this;
            return true;
        }
        catch (...)
        {
            Finalize();
            return false;
        }
    }

    void LogSystem::Finalize()
    {
        m_logger.Finalize();
        m_storage.Close();

        if (m_thread.joinable())
            m_thread.join();

        // Log残件を全て処理
        while (!m_storage.IsEmpty())
        {
            m_storage.TransferPending(m_drainCount);

            m_entries.clear();
            m_storage.Drain(m_entries, m_drainCount);
            m_process.Process(m_entries);
        }

        m_process.Finalize();
        m_storage.Finalize();
        m_entries.clear();
        m_drainCount = 0;
        if (s_instance == this)
            s_instance = nullptr;
    }

    void LogSystem::Run()
    {
        // 受付中または残件がある限り、DrainしてProcessする
        while (m_storage.IsAccepting() || !m_storage.IsEmpty())
        {
            m_storage.TransferPending(m_drainCount);

            m_entries.clear();
            m_storage.Drain(m_entries, m_drainCount);
            m_process.Process(m_entries);
        }
    }
}
