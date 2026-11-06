/*=============================================================================

 File   : LogSystem.h
 Desc   : ログ関連クラスと処理スレッドの所有・ライフサイクルを定義する。

------------------------------------------------------------------------------

 Date   : 2026/10/08
 Author : Yokoyama Haruki

=============================================================================*/

#ifndef _LOG_SYSTEM_H_
#define _LOG_SYSTEM_H_

#include <cstddef>
#include <thread>
#include <vector>

#include "Log/LogData.h"
#include "Log/LogStorage.h"
#include "Log/Logger.h"
#include "Log/LogProcess.h"

namespace Hestia
{
    /// @brief ログの受付・保持・処理と単一の処理スレッドを所有する System。
    /// @note Initialize／Finalize は管理スレッドで直列に呼ぶ。
    /// 初期化成功後は破棄前に必ず Finalize を呼び、記録元を先に停止する。
    class LogSystem
    {
    public:
        /// @brief 既定の出力先と設定を用いて初期化する。
        /// @return 出力先・スレッド・Logger の準備がすべて成功した場合: true。
        bool Initialize();

        /// @brief StorageとProcessを準備し、処理スレッドを開始して Logger を登録する
        /// @param settings 空パス・容量0・Drain件数0・Drain件数が容量超過は無効
        /// @return 設定・出力先・スレッド・Logger の準備がすべて成功した場合: true
        /// @note 二重初期化は false。失敗時は部分初期化した資源を逆順で解除する
        bool Initialize(const LogSettings& settings);

        /// @brief 受付を止め、joinする。残件を処理してLog系のSubSystemを終了する。
        /// @pre Loggerが停止している。出力先が正常なら受付済みログをすべて出力する。
        /// @note 未初期化・終了済みの場合は何もしない。
        void Finalize();

    private:
        bool CreateDefaultSettings(LogSettings& settings) const;
        void Run();

        LogStorage m_storage;
        Logger m_logger;
        LogProcess m_process;
        std::thread m_thread;
        std::size_t m_drainCount = 0;
        // batch の容量確保も Initialize の失敗処理で扱う。
        std::vector<LogEntry> m_entries;

        static LogSystem* s_instance;
    };
}

#endif // _LOG_SYSTEM_H_
