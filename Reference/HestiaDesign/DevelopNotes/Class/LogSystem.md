# Log System
---
ログの受付・保持・処理を別のクラスに分け、LogSystemが所有とライフサイクルをまとめるクラス設計と実装契約。
GameのHestiaGame::Logはstatic関数から保持するLogAPIを呼び、LogAPIがLoggerのstatic記録関数へ委譲する。LogAPIは拡張時の接続先としてLogSystemへの非所有ポインタを保持する。
LogSystemが処理スレッドのRunを持ち、LogStorageのTransfer・DrainとLogProcessの整形・出力を繰り返す。

## LogSystem
---
---
**役割**

Logger、LogStorage、LogProcess、処理スレッドを所有し、Loggerの登録・解除と処理順を管理する。
ログ本文の生成、キューの同期、ファイルへの書き込みはそれぞれのクラスへ委譲する。

```cpp
namespace Hestia
{
    class LogSystem
    {
    public:
        bool Initialize(const LogSettings& settings);
        void Finalize();

    private:
        void Run();

        LogStorage m_storage;
        Logger m_logger;
        LogProcess m_process;
        std::thread m_thread;
        std::size_t m_drainCount = 0;
        std::vector<LogEntry> m_entries;

        static LogSystem* s_instance;
    };
}
```

[LogSettings](#logsettings)  
[LogStorage](#logstorage)  
[Logger](#logger)  
[LogProcess](#logprocess)  
[LogLevel](#loglevel)  
[LogAPI](#logapi)

EngineがLogSystemとLogAPIを値として所有する。s_instanceは既存のSystem設計に合わせた非所有の自己参照であり、今回の記録経路では使用しない。System実体や内部Loggerを取得するpublic Getterは設けない。

`Initialize()` は次の順で利用準備を行う。

1. LogStorageの容量と受付状態を準備する。この時点では受付を閉じておく。
2. LogProcess::Initializeで出力ファイルを追記用に開く。処理batchの容量もスレッド開始前に確保する。
3. LogStorageの受付を開き、LogSystemの処理スレッドでRunを開始する。成功後、所有するLoggerをInitializeでLogStorageへ接続し、Logger::s_instanceへ登録する。
4. 全段階が成功した後に利用可能とし、EngineがLogAPIを接続する。

出力ファイルを開けない場合はfalseを返す。途中まで準備した資源を逆順で解放し、GameへAPIを渡さない。二重初期化は失敗として扱い、稼働中のキューや出力先を書き換えない。

`Finalize()` は記録元の停止後にLoggerの接続を解除し、LogStorageの受付を閉じる。Runがpendingとqueueの残件を処理し終えてからLogSystemが処理スレッドをjoinする。その後LogProcess::Finalizeでflush・closeし、LogStorageの資源を解除する。未初期化・終了済みの場合は何もせず戻る。

EngineはGameの終了処理や他のSubsystemの終了処理がログを記録できる間、LogSystemを維持する。記録元のスレッドを停止し、GameのFacadeとLogAPIを切り離した後、LogSystemを最後に終了する。InitializeとFinalizeはEngineの管理スレッドで行い、同時実行しない。

### スレッドと処理フロー
---
**メインスレッド（呼び出し元）**

HestiaGame::Log::Info／Warning／Error／Fatal
↓
保持するLogAPIの対応する記録関数（InfoはLogAPI::Log）→ Loggerのstatic API → s_instanceの非static Write
↓
LoggerがLogEntryを作成し、LogStorageの書き込み側pending queueへ追加
↓
処理スレッドは待機せず、次の処理へ進む

**処理スレッド（LogSystem::Run）**

読み取り側pending queueが空なら、読み書き側を切り替える
↓
pending queueから最大N件ずつLogStorageのqueueへ移送
↓
queueの先頭から最大N件をDrain
↓
DrainしたbatchをLogProcess::Processへ渡し、整形してファイルへ書き込む
↓
繰り返し。pendingが空なら切り替えを試し、両方空ならTransferだけ通過してDrainへ進む

**終了時**はLogStorageの受付を閉じ、Runがpendingとqueueの残件を書き終える。メインスレッドが処理スレッドをjoinした後、LogProcess::Finalizeでflush・closeする。
pendingのPushと読み書き側の切り替えだけをmutexで同期する。移送・Drain・整形・ファイル出力中はmutexを保持しない。

Runの最小スケッチは次の形とする。TransferPending内で、読み側pendingが空なら切り替えを試み、両方空なら通過する。

```cpp
void Hestia::LogSystem::Run()
{
    while (m_storage.IsAccepting() || !m_storage.IsEmpty())
    {
        m_storage.TransferPending(m_drainCount);

        m_entries.clear();
        m_storage.Drain(m_entries, m_drainCount);
        m_process.Process(m_entries);
    }
}
```

[LogSystem](#logsystem)  
[LogStorage](#logstorage)  
[LogProcess](#logprocess)  
[LogEntry](#logentry)

処理スレッドは待機なしで反復し、ログがない間もCPUを使用する。Drain・整形・ファイル出力の時間も一回の反復に含まれる。

**アクセス制御**：LogSystemのRunはprivateのスレッド入口とする。LogStorageのPush・TransferPending・Drain、Loggerの非staticなInitialize／Finalize、LogProcessの出力操作はEngine内のクラス間連携用public操作とする。Loggerの非static Write、queue・mutex・出力ファイル・接続先ポインタとSwitchPendingはprivateに置く。Gameへ公開する操作はLogAPIで制限し、内部クラス間の連携にfriendは使用しない。

### Logger
---
**役割**

Log・Warning・Error・Fatalを受け付け、呼び出し情報をEngine所有のLogEntryへコピーしてLogStorageへ渡す。
ログの保持やファイルへの書き込みは行わない。

```cpp
namespace Hestia
{
    class Logger
    {
    public:
        static void Log(std::string_view message,
            const std::source_location& source = std::source_location::current());

        static void Warning(std::string_view message,
            const std::source_location& source = std::source_location::current());

        static void Error(std::string_view message,
            const std::source_location& source = std::source_location::current());

        static void Fatal(std::string_view message,
            const std::source_location& source = std::source_location::current());

        bool Initialize(LogStorage* storage);
        void Finalize();

    private:
        void Write(LogLevel level, std::string_view message,
            const std::source_location& source);

        LogStorage* m_storage = nullptr;
        static Logger* s_instance;

    };
}
```

[LogStorage](#logstorage)  
[LogEntry](#logentry)  
[LogLevel](#loglevel)  
[LogSystem](#logsystem)

Loggerでstaticとする関数はLog、Warning、Error、Fatalの四つだけとする。各関数はs_instanceが指すLoggerの非static Writeへ委譲する。s_instanceはLogSystemが所有するLoggerへの非所有ポインタ、m_storageはそのSystemが所有するLogStorageへの非所有ポインタとする。static関数とs_instanceの実体はEngine.dll側に一つ置き、Game.dll側へ同じ実装を別リンクしない。

非staticのInitializeはm_storageを設定し、thisをs_instanceへ登録する。別のLoggerが登録済みの場合はfalseを返す。Finalizeは自身の登録とm_storageを解除する。登録・解除は記録元が動作していない時点で行う。

同時に稼働するLogSystemは一つとし、別のSystemがLoggerへ再登録する初期化は失敗として扱う。Game向けLogAPIも、そのSystemにだけ接続する。Loggerが未接続の期間の記録呼び出しは、何もせず戻る。

四つの関数は重要度だけを変えてWriteへ委譲する。本文・ファイル名・関数名は、呼び出しが戻る前にコピーする。string_viewやsource_locationの文字列参照をキューへ残さず、Game DLLのアンロード後も受付済みのログを処理できるようにする。

呼び出し元の時刻とスレッドIDを受付時に取得する。複数スレッドから記録できるが、Loggerの接続変更は記録元を停止した後に行う。Editorの有無によって受付を無効にしない。

FatalはFatalレベルの記録として扱う草案。ダイアログ表示、強制終了、同期flushを記録関数へ追加するかは未確定であり、今回のAPIからその動作を保証しない。

### LogStorage
---
**役割**

受付済みで未処理のLogEntryを保持し、受付スレッドと処理スレッドの間を同期する。
書き込み側・読み取り側の2本のpending queueと、出力待ちqueue、容量制限、受付の開閉、破棄件数を管理する。

```cpp
namespace Hestia
{
    class LogStorage
    {
    public:
        bool Initialize(std::size_t storageCapacity);
        void Finalize();

        void Open();
        void Close();

        bool IsAccepting() const;
        bool Push(LogEntry entry);
        std::size_t TransferPending(std::size_t maxCount);
        std::size_t Drain(std::vector<LogEntry>& entries,
            std::size_t maxCount);
        bool IsEmpty() const;

    private:
        bool SwitchPending();

        std::array<std::queue<LogEntry>, 2> m_pending;
        std::size_t m_writeIndex = 0;
        std::size_t m_readIndex = 1;
        std::queue<LogEntry> m_queue;
        mutable std::mutex m_pendingMutex;

        std::size_t m_storageCapacity = 0;
        std::atomic<std::size_t> m_bufferedCount = 0;
        std::uint64_t m_droppedCount = 0;
        bool m_accepting = false;

    };
}
```

[LogEntry](#logentry)  
[Logger](#logger)  
[LogProcess](#logprocess)  
[LogSystem](#logsystem)

Pushは完成済みのLogEntryをm_pending[m_writeIndex]の末尾へ追加する。PushとCloseはm_pendingMutexで保護し、Closeより前にPushが成功したログは処理対象に含める。Close以後は新しいログを受け付けない。

pendingは`std::queue<LogEntry>`を2本持ち、受付中は片方をLoggerの書き込み先、もう片方をLogSystemの処理スレッドからの移送元とする。LogEntry内の文字列はLoggerでコピーしてからPushへ渡す。

pending側のqueueはLoggerのPushとLogSystem::RunからのTransferPendingがm_pendingMutexで同期する。読み取り側pending queueと出力待ちqueueはLogSystemの処理スレッドだけが操作する。SwitchPendingはTransferPendingから呼ぶLogStorageのprivate補助関数とする。

IsAcceptingは受付状態をmutexで保護して読み取る。IsEmptyはatomicなm_bufferedCountが0かで、2本のpending queueと出力待ちqueueのすべてが空かを返す。処理側以外からqueueを直接読み取らず、Finalize後のworker終了判定に使う。

SwitchPendingは読み取り側queueが空のときだけ行う。LogStorage内部でm_pendingMutexを取得し、書き込み側queueに残件があればm_writeIndexとm_readIndexを交換してすぐに解除する。残件がなければ交換せずfalseを返す。queueそのものをswapするのではなく、読み書き対象のindexだけを切り替える。

切り替え後、Loggerは空になった新しい書き込み側queueへ追加し、LogSystemの処理スレッドは読み取り側queueからTransferする。読み取り側queueに残件がある間は再交換しない。

TransferPendingは読み取り側queueから最大maxCount件をpopし、出力待ちqueueへpushする。移送ではm_pendingMutexを取得せず、m_bufferedCountも変えない。読み取り側queueが空ならSwitchPendingを試み、切り替え後に移送する。別の移送は並行実行しない。

Drainはqueueの先頭から最大maxCount件を出力batchへ移す。pendingのmutexを取得せず、取り出した件数だけm_bufferedCountを減らす。整形とファイルI/OはDrainが戻ってから行う。

**受付への影響**：Push同士とindex切り替えは同じmutexで競合する。`std::queue`のPushは内部dequeのメモリ確保を行う可能性があり、その間mutexを保持する。今回は単純なqueueを使い、実測で受付待ちが問題になった場合に固定容量queueなどを別途検討する。本文生成・整形・ファイルI/Oはmutexの外で行う。

**処理順**：LogSystem::RunがTransferPending → Drain → LogProcess::Processを呼ぶ。TransferとDrainの一回の上限は同じN件とする。両側のpendingが空ならTransferだけ通過してDrainへ進み、次の反復で再確認する。

LogStorageは処理スレッドの起動や新着通知を行わない。LogSystem::Runがホットループでpendingを確認する。

保持順序はPushが成功した順とする。読み取り側queueを空にしてから次の交換を行うことで、2本のqueueをまたぐ受付順も維持する。複数スレッドの同時呼び出しについて、呼び出し開始時刻の厳密な順序までは保証しない。

**実装契約**：書き込み側pending・読み取り側pending・出力待ちqueueの合計をstorageCapacity以下に制限する。出力batchは別途最大N件に制限する。m_bufferedCountはPush時に増やし、index切り替えとTransferでは変えず、Drain時に減らす。上限判定と増加は他のPushに割り込まれないように行う。満杯の場合は重要度に関係なく新着を破棄し、破棄件数を保持する。本文の最大長や総バイト数制限は未確定。Fatalも容量超過や出力失敗によって失われ得るため、必ず残す要件がある場合は専用経路を別途検討する。

LogStorageが保持するのは未処理ログ。Editorで閲覧する処理済み履歴は別の責務であり、表示機能を追加する段階で設計する。

### LogProcess
---
**役割**

LogSystemから渡されたログbatchを整形し、出力ファイルへ書き込む。
出力ファイルを所有し、Initializeで開き、Finalizeでflush・closeする。

```cpp
namespace Hestia
{
    class LogProcess
    {
    public:
        bool Initialize(const std::filesystem::path& filePath);
        void Finalize();

        void Process(std::span<const LogEntry> entries);

    private:
        std::ofstream m_stream;
    };
}
```

[LogStorage](#logstorage)  
[LogEntry](#logentry)  
[LogSystem](#logsystem)

Initializeは既存内容を消さず追記用に出力先を開き、失敗時はfalseを返す。親フォルダは呼び出し側で用意する。ProcessはLogSystem::Runから呼ばれ、渡されたbatchを同期的に整形・出力する。空のbatchでは何もしない。LogStorageを参照せず、スレッドの起動や反復制御も行わない。

FinalizeはLogSystemが処理スレッドをjoinした後に呼び、出力先をflush・closeする。ProcessとFinalizeを並行実行しない。

LogEntryの所有者はLogSystemのbatchであり、LogProcessはProcessの呼び出し中だけspanで借用する。Processが戻った後、Runが次のbatchのために要素を解放する。整形は重要度、UTC時刻、ファイル・行・関数、スレッドIDを含むヘッダーを出力し、次の行から本文を2スペース下げて出力する。本文中の改行はそのまま出力し、後続行にも同じインデントを付ける。同じログの集約、Console出力、Editorへの配送は必要性を確認して追加する。

出力失敗を同じLoggerへ再記録すると再帰するため、LogProcess内部の失敗状態として保持する。稼働中の書き込み失敗をEngineへ通知する方法、代替出力先、flush間隔は未確定。通常終了による排出は、出力先が正常である場合の保証であり、プロセス強制終了時の保存は保証しない。

### LogAPI
---
**役割**

Gameへログ記録だけを公開し、内部でLoggerのstatic関数を直接呼ぶ。LogSystemへの非所有ポインタは拡張時の接続先として保持するが、四つの記録APIでは参照しない。
LogSystem、Logger、キュー、ファイル、処理スレッドを所有しない。

```cpp
namespace Hestia
{
    class HESTIA_ENGINE_API LogAPI
    {
    public:
        void Initialize(LogSystem* system);
        void Finalize();

        void Log(std::string_view message,
            const std::source_location& source = std::source_location::current());

        void Warning(std::string_view message,
            const std::source_location& source = std::source_location::current());

        void Error(std::string_view message,
            const std::source_location& source = std::source_location::current());

        void Fatal(std::string_view message,
            const std::source_location& source = std::source_location::current());

    private:
        LogSystem* m_system = nullptr;
    };
}
```

[LogSystem](#logsystem)  
[Logger](#logger)

InitializeとFinalizeはEngineによる接続・切り離しに使用する。GameのFacadeへ公開する操作は四つの記録関数に限り、Gameからライフサイクルを変更しない。

委譲の最小スケッチは次の形とする。四つのLogAPIは対応するLoggerのstatic関数を直接呼ぶ。m_systemは拡張用の非所有参照として残し、この記録経路では使用しない。

```cpp
void Hestia::LogAPI::Log(std::string_view message,
    const std::source_location& source)
{
    Logger::Log(message, source);
}

void Hestia::Logger::Log(std::string_view message,
    const std::source_location& source)
{
    s_instance->Write(LogLevel::Log, message, source);
}
```

[LogAPI](#logapi)  
[LogSystem](#logsystem)  
[Logger](#logger)  
[LogLevel](#loglevel)

Gameからの利用はAPI接続中に限る。未接続時の利用は内部契約違反として扱う草案。Facadeを経由する場合はFacadeの呼び出し地点でsource_locationを取得し、そのまま渡す。APIの実装内で取り直さない。

EngineはLogSystemとLogAPIを所有し、InitializeでLogSystem → LogAPI → TimeSystem／TimeAPIの順に接続する。FinalizeではTimeSystem／TimeAPIを先に終了し、LogAPIを切り離してLogSystemを最後に終了する。GameEngineAPIへの格納とGameRuntimeからGameのFacadeへの接続は後続の変更として扱う。

### HestiaGame::Log
---
**役割**

Game側のstatic記録窓口。内部で保持するLogAPIを呼び、ログ状態を所有しない。

```cpp
namespace HestiaGame
{
    class Log
    {
    public:
        static void Initialize(Hestia::LogAPI* api);
        static void Finalize();

        static void Info(std::string_view message,
            const std::source_location& source = std::source_location::current());
        static void Warning(std::string_view message,
            const std::source_location& source = std::source_location::current());

        static void Error(std::string_view message,
            const std::source_location& source = std::source_location::current());

        static void Fatal(std::string_view message,
            const std::source_location& source = std::source_location::current());

    private:
        static Hestia::LogAPI* s_api;
    };
}
```

[LogAPI](#logapi)  
[GameRuntime](./GameRuntime.md#gameruntime)

EngineがLogAPIを所有し、GameRuntimeがGame利用開始前にInitializeでs_apiへ接続する。Gameの記録元を停止した後にFinalizeで切り離す。これらは接続管理用のpublic操作とし、s_apiはprivateの非所有ポインタとする。

各static記録関数はs_apiの対応する操作へ本文とsource_locationをそのまま渡す。LoggerやLogSystemの状態へ直接アクセスしない。

通常ログのFacade APIはInfoとし、LogAPI::Logへ渡す。クラス名Logと同名のstatic関数LogはC++では定義できないため、Logger／LogAPI側のLogとFacade側のInfoを対応させる。

```cpp
void HestiaGame::Log::Initialize(Hestia::LogAPI* api)
{
    s_api = api;
}

void HestiaGame::Log::Finalize()
{
    s_api = nullptr;
}

void HestiaGame::Log::Info(std::string_view message,
    const std::source_location& source)
{
    s_api->Log(message, source);
}

void HestiaGame::Log::Warning(std::string_view message,
    const std::source_location& source)
{
    s_api->Warning(message, source);
}

void HestiaGame::Log::Error(std::string_view message,
    const std::source_location& source)
{
    s_api->Error(message, source);
}

void HestiaGame::Log::Fatal(std::string_view message,
    const std::source_location& source)
{
    s_api->Fatal(message, source);
}
```

[LogAPI](#logapi)

### LogEntry
---
**役割**

受付後の処理に必要な値を所有し、呼び出し元の文字列やGame DLLの寿命から独立させる。

```cpp
namespace Hestia
{
    struct LogEntry
    {
        LogLevel m_level;
        std::string m_message;
        std::chrono::system_clock::time_point m_timestamp;
        std::string m_fileName;
        std::string m_functionName;
        std::uint_least32_t m_line = 0;
        std::thread::id m_threadId;
    };
}
```

[LogLevel](#loglevel)

DrainでLogStorageからLogSystemのbatchへ所有権を渡す。LogProcessは処理中だけそのbatchを借用し、参照を保持しない。source_locationそのものは保持しない。本文の文字コードはUTF-8とする草案で、書式文字列と可変引数によるオーバーロードは必要性と呼び出し位置の取得方法を確認して設計する。

### LogLevel
---
**役割**

記録の重要度を表し、出力表記と容量超過時の扱いを判断する。

```cpp
namespace Hestia
{
    enum class LogLevel
    {
        Log,
        Warning,
        Error,
        Fatal
    };
}
```

### LogSettings
---
**役割**

出力先と未処理ログの保持上限を初期化時に渡す。

```cpp
namespace Hestia
{
    struct LogSettings
    {
        std::filesystem::path m_filePath;
        std::size_t m_storageCapacity;
        std::size_t m_drainCount;
    };
}
```

明示設定で空の出力先、容量0、Drain件数0、Drain件数が保持容量を超える場合は初期化失敗とする。引数なしの `LogSystem::Initialize()` は起動時の作業フォルダにLogsフォルダを作成する。初期化時刻を `yyyyMMdd_HHmmss_ffff.txt` 形式にしたファイルを作成し、容量4096件、Drain256件を既定値とする。出力先決定・フォルダ作成・ファイル名生成はLogSystem内部で行い、Engineは引数なしのInitializeを呼ぶ。

### 記録経路と所有関係
---
```mermaid
flowchart LR
    Game[Gameの呼び出し元] --> Facade[HestiaGame::Log Info Warning Error Fatal]
    Facade --> API[LogAPI]
    API --> Logger[Logger static API]
    Logger --> Instance[Logger s_instanceの非static Write]
    Instance --> Pending[LogStorage pending write queue]
    Pending -->|index切り替え| PendingRead[LogStorage pending read queue]
    PendingRead -->|最大N件ずつ移送| Queue[LogStorage output queue]
    Queue -->|最大N件ずつDrain| Process[LogProcess: 整形と出力]
    Process --> File[ログファイル]
```

Engine → LogSystem／LogAPIは所有、LogSystem → Logger／LogStorage／LogProcess／処理スレッドも所有とする。LogAPI → LogSystem、HestiaGame::Log → LogAPI、Loggerのm_storage → LogStorage、Logger::s_instance → Loggerは非所有参照とする。LogProcessはStorageを参照しない。

確認対象は、受付中の文字列コピー、複数記録元からの配送、容量超過時の破棄、初期化失敗時の後片付け、Finalizeでの残件排出。実装はEngine/Include/LogとEngine/Source/Logに置き、Test/Hestia_Tests/Source/LogTests.cppで検証する。Game DLLとFacadeは未接続のため、実際のDLLアンロードを伴う確認は後続で行う。
