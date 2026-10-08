/*=============================================================================

 File   : LogTests.cpp
 Desc   : ログの容量・順序・並行受付・初期化失敗・終了時保存を検証する。

------------------------------------------------------------------------------

 Date   : 2026/10/08
 Author : Yokoyama Haruki

=============================================================================*/

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <latch>
#include <memory>
#include <regex>
#include <source_location>
#include <string>
#include <thread>
#include <vector>

#include "EngineAPI.h"
#include "Log/LogAPI.h"
#include "Log/Logger.h"
#include "Log/LogStorage.h"
#include "Log/LogSystem.h"

namespace
{
    Hestia::LogEntry MakeEntry(const std::string& message)
    {
        Hestia::LogEntry entry;
        entry.m_message = message;
        return entry;
    }

    class LogStorageTest : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            ASSERT_TRUE(m_storage.Initialize(4));
            m_storage.Open();
        }

        void TearDown() override
        {
            m_logger.Finalize();
            m_storage.Close();
            m_storage.Finalize();
        }

        Hestia::LogStorage m_storage;
        Hestia::Logger m_logger;
    };

    TEST_F(LogStorageTest, CapacityCoversEveryQueueAndTransfersKeepAcceptanceOrder)
    {
        RecordProperty("target", "LogStorage::Push / TransferPending / Drain");
        for (int index = 0; index < 4; ++index)
        {
            ASSERT_TRUE(m_storage.Push(MakeEntry(std::to_string(index))));
        }

        EXPECT_EQ(m_storage.TransferPending(2), 2);
        EXPECT_FALSE(m_storage.Push(MakeEntry("full")));
        std::vector<Hestia::LogEntry> entries;
        EXPECT_EQ(m_storage.Drain(entries, 1), 1);
        ASSERT_TRUE(m_storage.Push(MakeEntry("4")));
        EXPECT_FALSE(m_storage.Push(MakeEntry("full")));

        EXPECT_EQ(m_storage.TransferPending(2), 2);
        EXPECT_EQ(m_storage.Drain(entries, 2), 2);
        EXPECT_EQ(m_storage.TransferPending(2), 1);
        EXPECT_EQ(m_storage.Drain(entries, 2), 2);

        ASSERT_EQ(entries.size(), 5);
        for (std::size_t index = 0; index < entries.size(); ++index)
        {
            EXPECT_EQ(entries[index].m_message, std::to_string(index));
        }
        EXPECT_TRUE(m_storage.IsEmpty());
    }

    TEST_F(LogStorageTest, CloseRejectsNewRecordsAndKeepsAcceptedRecords)
    {
        RecordProperty("target", "LogStorage::Close / Push / IsEmpty");
        ASSERT_TRUE(m_storage.Push(MakeEntry("accepted")));
        m_storage.Close();
        EXPECT_FALSE(m_storage.IsAccepting());
        EXPECT_FALSE(m_storage.Push(MakeEntry("rejected")));
        EXPECT_FALSE(m_storage.IsEmpty());
        EXPECT_EQ(m_storage.TransferPending(4), 1);

        std::vector<Hestia::LogEntry> entries;
        EXPECT_EQ(m_storage.Drain(entries, 4), 1);
        ASSERT_EQ(entries.size(), 1);
        EXPECT_EQ(entries.front().m_message, "accepted");
        EXPECT_TRUE(m_storage.IsEmpty());

        m_storage.Open();
        EXPECT_TRUE(m_storage.Push(MakeEntry("reopened")));
    }

    TEST_F(LogStorageTest, ZeroLimitsDoNotConsumeRecordsAndDoubleInitializeKeepsCapacity)
    {
        RecordProperty("target", "LogStorage::Initialize / TransferPending / Drain");
        EXPECT_FALSE(m_storage.Initialize(1));
        EXPECT_TRUE(m_storage.Push(MakeEntry("0")));
        EXPECT_TRUE(m_storage.Push(MakeEntry("1")));
        EXPECT_EQ(m_storage.TransferPending(0), 0);
        EXPECT_EQ(m_storage.TransferPending(1), 1);

        std::vector<Hestia::LogEntry> entries;
        EXPECT_EQ(m_storage.Drain(entries, 0), 0);
        EXPECT_TRUE(entries.empty());
        EXPECT_EQ(m_storage.Drain(entries, 1), 1);
        EXPECT_EQ(entries.front().m_message, "0");
    }

    TEST_F(LogStorageTest, ConcurrentProducersAndConsumerKeepEveryRecordAndPerProducerOrder)
    {
        RecordProperty("target", "LogStorage concurrent Push / TransferPending / Drain");
        m_storage.Finalize();
        ASSERT_TRUE(m_storage.Initialize(512));
        m_storage.Open();

        constexpr int PRODUCER_COUNT = 4;
        constexpr int RECORDS_PER_PRODUCER = 64;
        std::atomic<int> running = PRODUCER_COUNT;
        std::atomic<int> rejected = 0;
        std::latch start(1);
        std::vector<std::thread> producers;
        for (int producer = 0; producer < PRODUCER_COUNT; ++producer)
        {
            producers.emplace_back([&, producer]
            {
                start.wait();
                for (int index = 0; index < RECORDS_PER_PRODUCER; ++index)
                {
                    if (!m_storage.Push(MakeEntry(
                        std::to_string(producer) + ":" + std::to_string(index))))
                    {
                        ++rejected;
                    }
                }
                --running;
            });
        }

        std::vector<Hestia::LogEntry> entries;
        entries.reserve(PRODUCER_COUNT * RECORDS_PER_PRODUCER);
        start.count_down();
        while (running.load() != 0 || !m_storage.IsEmpty())
        {
            m_storage.TransferPending(7);
            m_storage.Drain(entries, 7);
        }

        for (auto& producer : producers)
        {
            producer.join();
        }

        EXPECT_EQ(rejected.load(), 0);
        ASSERT_EQ(entries.size(), PRODUCER_COUNT * RECORDS_PER_PRODUCER);
        std::array<int, PRODUCER_COUNT> nextIndex{};
        for (const auto& entry : entries)
        {
            const auto separator = entry.m_message.find(':');
            ASSERT_NE(separator, std::string::npos);
            const int producer = std::stoi(entry.m_message.substr(0, separator));
            const int index = std::stoi(entry.m_message.substr(separator + 1));
            ASSERT_GE(producer, 0);
            ASSERT_LT(producer, PRODUCER_COUNT);
            EXPECT_EQ(index, nextIndex[producer]++);
        }
        EXPECT_TRUE(m_storage.IsEmpty());
    }

    TEST_F(LogStorageTest, LoggerOwnsTextLocationAndLevelBeforeReturning)
    {
        RecordProperty("target", "Logger::Log / Warning / Error / Fatal / Write");
        ASSERT_TRUE(m_logger.Initialize(&m_storage));
        std::string message = "日本語の本文";
        const auto source = std::source_location::current();
        const auto earliest = std::chrono::system_clock::now();
        Hestia::Logger::Log(message, source);
        message.assign("changed");
        Hestia::Logger::Warning("warning", source);
        Hestia::Logger::Error("error", source);
        Hestia::Logger::Fatal("fatal", source);
        const auto latest = std::chrono::system_clock::now();

        EXPECT_EQ(m_storage.TransferPending(4), 4);
        std::vector<Hestia::LogEntry> entries;
        EXPECT_EQ(m_storage.Drain(entries, 4), 4);
        ASSERT_EQ(entries.size(), 4);
        EXPECT_EQ(entries[0].m_message, "日本語の本文");
        EXPECT_EQ(entries[0].m_level, Hestia::LogLevel::Log);
        EXPECT_EQ(entries[1].m_level, Hestia::LogLevel::Warning);
        EXPECT_EQ(entries[2].m_level, Hestia::LogLevel::Error);
        EXPECT_EQ(entries[3].m_level, Hestia::LogLevel::Fatal);
        EXPECT_EQ(entries[0].m_fileName, source.file_name());
        EXPECT_EQ(entries[0].m_functionName, source.function_name());
        EXPECT_EQ(entries[0].m_line, source.line());
        EXPECT_EQ(entries[0].m_threadId, std::this_thread::get_id());
        EXPECT_GE(entries[0].m_timestamp, earliest);
        EXPECT_LE(entries[0].m_timestamp, latest);
    }

    TEST_F(LogStorageTest, DetachedLoggerIsSilentAndSecondRegistrationDoesNotReplaceOwner)
    {
        RecordProperty("target", "Logger::Initialize / Finalize / detached recording");
        Hestia::Logger::Log("before");
        Hestia::Logger::Warning("before");
        Hestia::Logger::Error("before");
        Hestia::Logger::Fatal("before");
        EXPECT_TRUE(m_storage.IsEmpty());

        ASSERT_TRUE(m_logger.Initialize(&m_storage));
        Hestia::Logger second;
        EXPECT_FALSE(second.Initialize(&m_storage));
        second.Finalize();
        Hestia::Logger::Log("owner");
        m_logger.Finalize();
        Hestia::Logger::Error("after");
        EXPECT_EQ(m_storage.TransferPending(4), 1);

        std::vector<Hestia::LogEntry> entries;
        EXPECT_EQ(m_storage.Drain(entries, 4), 1);
        ASSERT_EQ(entries.size(), 1);
        EXPECT_EQ(entries.front().m_message, "owner");
    }

    class LogSystemTest : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            static std::atomic<unsigned int> nextDirectory = 0;
            m_directory = std::filesystem::current_path() / "Results" /
                ("LogSystem-" + std::to_string(
                    std::chrono::steady_clock::now().time_since_epoch().count())
                    + "-" + std::to_string(nextDirectory.fetch_add(1)));
            std::filesystem::create_directories(m_directory);
            RecordProperty("outputDirectory", m_directory.string());
        }

        void TearDown() override
        {
            m_api.Finalize();
            m_system.Finalize();
        }

        Hestia::LogSettings Settings(const std::string& name = "output.log") const
        {
            return { m_directory / name, 4096, 7 };
        }

        std::string Read(const std::filesystem::path& filePath) const
        {
            std::ifstream stream(filePath, std::ios::binary);
            return { std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>() };
        }

        std::filesystem::path m_directory;
        Hestia::LogSystem m_system;
        Hestia::LogAPI m_api;
    };

    TEST_F(LogSystemTest, InvalidSettingsAndOpenFailureLeaveSystemReusable)
    {
        RecordProperty("target", "LogSystem::Initialize rollback / Finalize");
        auto settings = Settings();
        auto invalid = settings;
        invalid.m_filePath.clear();
        EXPECT_FALSE(m_system.Initialize(invalid));
        invalid = settings;
        invalid.m_storageCapacity = 0;
        EXPECT_FALSE(m_system.Initialize(invalid));
        invalid = settings;
        invalid.m_drainCount = 0;
        EXPECT_FALSE(m_system.Initialize(invalid));
        invalid = settings;
        invalid.m_drainCount = invalid.m_storageCapacity + 1;
        EXPECT_FALSE(m_system.Initialize(invalid));
        invalid = settings;
        invalid.m_filePath = m_directory / "missing" / "output.log";
        EXPECT_FALSE(m_system.Initialize(invalid));

        m_system.Finalize();
        ASSERT_TRUE(m_system.Initialize(settings));
        Hestia::Logger::Log("recovered");
        m_system.Finalize();
        m_system.Finalize();
        EXPECT_NE(Read(settings.m_filePath).find("recovered"), std::string::npos);
    }

    TEST_F(LogSystemTest, DoubleInitializeDoesNotChangeActiveOutputAndOtherSystemCannotRegister)
    {
        RecordProperty("target", "LogSystem::Initialize singleton / double initialization");
        const auto first = Settings("first.log");
        const auto second = Settings("second.log");
        ASSERT_TRUE(m_system.Initialize(first));
        Hestia::Logger::Log("before");
        EXPECT_FALSE(m_system.Initialize(second));

        Hestia::LogSystem other;
        EXPECT_FALSE(other.Initialize(second));
        other.Finalize();
        Hestia::Logger::Log("after");
        m_system.Finalize();
        const auto output = Read(first.m_filePath);
        EXPECT_NE(output.find("before"), std::string::npos);
        EXPECT_NE(output.find("after"), std::string::npos);
        EXPECT_FALSE(std::filesystem::exists(second.m_filePath));

        ASSERT_TRUE(m_system.Initialize(second));
        Hestia::Logger::Log("reinitialized");
        m_system.Finalize();
        EXPECT_NE(Read(second.m_filePath).find("reinitialized"), std::string::npos);
    }

    TEST_F(LogSystemTest, LoggerRegistrationFailureRollsBackThreadAndOutput)
    {
        RecordProperty("target", "LogSystem::Initialize partial thread rollback");
        Hestia::LogStorage storage;
        Hestia::Logger external;
        ASSERT_TRUE(storage.Initialize(4));
        storage.Open();
        ASSERT_TRUE(external.Initialize(&storage));

        const auto settings = Settings();
        EXPECT_FALSE(m_system.Initialize(settings));
        Hestia::Logger::Log("external-owner");
        external.Finalize();
        storage.Close();
        std::vector<Hestia::LogEntry> entries;
        EXPECT_EQ(storage.TransferPending(4), 1);
        EXPECT_EQ(storage.Drain(entries, 4), 1);
        storage.Finalize();
        ASSERT_EQ(entries.size(), 1);
        EXPECT_EQ(entries.front().m_message, "external-owner");

        ASSERT_TRUE(m_system.Initialize(settings));
        Hestia::Logger::Log("recovered");
        m_system.Finalize();
        EXPECT_NE(Read(settings.m_filePath).find("recovered"), std::string::npos);
    }

    TEST_F(LogSystemTest, FinalizeSavesAllRecordsInOrderAndAppendsWithoutLosingCallSite)
    {
        RecordProperty("target", "LogAPI source forwarding / LogSystem::Finalize / LogProcess");
        const auto settings = Settings();
        {
            std::ofstream stream(settings.m_filePath, std::ios::binary);
            stream << "existing-content\n";
        }

        ASSERT_TRUE(m_system.Initialize(settings));
        m_api.Initialize(&m_system);
        const auto source = std::source_location::current();
        m_api.Log("日本語\n次の行", source);
        m_api.Warning("literal\\ntext", source);
        m_api.Error("error", source);
        m_api.Fatal("fatal", source);
        for (int index = 0; index < 100; ++index)
        {
            Hestia::Logger::Log("record-" + std::to_string(index) + "-end", source);
        }

        m_api.Finalize();
        m_system.Finalize();
        const auto output = Read(settings.m_filePath);
        EXPECT_TRUE(output.starts_with("existing-content\n"));
        EXPECT_NE(output.find(")\n  日本語\n  次の行\n"), std::string::npos);
        EXPECT_NE(output.find(")\n  literal\\ntext\n"), std::string::npos);
        EXPECT_NE(output.find("[Warning]"), std::string::npos);
        EXPECT_NE(output.find("[Error]"), std::string::npos);
        EXPECT_NE(output.find("[Fatal]"), std::string::npos);
        EXPECT_NE(output.find(std::to_string(source.line())), std::string::npos);
        EXPECT_NE(output.find("LogTests.cpp"), std::string::npos);
        EXPECT_NE(output.find(source.function_name()), std::string::npos);
        EXPECT_NE(output.find("Z]"), std::string::npos);
        EXPECT_NE(output.find("[thread:"), std::string::npos);
        EXPECT_EQ(std::count(output.begin(), output.end(), '\n'), 210);

        std::size_t previous = 0;
        for (int index = 0; index < 100; ++index)
        {
            const auto position = output.find(
                "record-" + std::to_string(index) + "-end", previous);
            ASSERT_NE(position, std::string::npos);
            previous = position + 1;
        }
    }

    TEST_F(LogSystemTest, FinalizeAfterConcurrentProducersSavesEveryAcceptedRecord)
    {
        RecordProperty("target", "Logger multi-thread recording / LogSystem::Finalize");
        const auto settings = Settings();
        ASSERT_TRUE(m_system.Initialize(settings));

        std::latch start(1);
        std::vector<std::thread> producers;
        for (int producer = 0; producer < 4; ++producer)
        {
            producers.emplace_back([producer, &start]
            {
                start.wait();
                for (int index = 0; index < 64; ++index)
                {
                    Hestia::Logger::Log("producer-" + std::to_string(producer)
                        + "-record-" + std::to_string(index) + "-end");
                }
            });
        }
        start.count_down();
        for (auto& producer : producers)
        {
            producer.join();
        }

        m_system.Finalize();
        const auto output = Read(settings.m_filePath);
        EXPECT_EQ(std::count(output.begin(), output.end(), '\n'), 256);
        for (int producer = 0; producer < 4; ++producer)
        {
            std::size_t previous = 0;
            for (int index = 0; index < 64; ++index)
            {
                const auto position = output.find("producer-" + std::to_string(producer)
                    + "-record-" + std::to_string(index) + "-end", previous);
                ASSERT_NE(position, std::string::npos);
                previous = position + 1;
            }
        }
    }

    TEST_F(LogSystemTest, EngineConnectsLoggerAndDestroyFlushesBeforeDisconnecting)
    {
        RecordProperty("target", "Engine::Initialize / Engine::Finalize log ownership");
        const auto logDirectory = std::filesystem::current_path() / L"Logs";
        std::vector<std::filesystem::path> previousFiles;
        if (std::filesystem::exists(logDirectory))
        {
            for (const auto& entry : std::filesystem::directory_iterator(logDirectory))
            {
                previousFiles.push_back(entry.path());
            }
        }

        Hestia::ApplicationAPI applicationAPI;
        const Hestia::EngineAPI* api = GetEngineAPI();
        ASSERT_NE(api, nullptr);
        std::unique_ptr<void, void (*)(void*)> engine(
            api->Create(&applicationAPI), api->Destroy);
        ASSERT_NE(engine.get(), nullptr);

        const std::string message = m_directory.filename().string();
        Hestia::Logger::Log(message);
        engine.reset();
        std::filesystem::path filePath;
        const std::regex fileNamePattern(R"(^\d{8}_\d{6}_\d{4}\.txt$)");
        for (const auto& entry : std::filesystem::directory_iterator(logDirectory))
        {
            if (std::find(previousFiles.begin(), previousFiles.end(), entry.path())
                == previousFiles.end()
                && std::regex_match(entry.path().filename().string(), fileNamePattern))
            {
                filePath = entry.path();
                break;
            }
        }
        ASSERT_FALSE(filePath.empty());
        const auto output = Read(filePath);
        EXPECT_NE(output.find(message), std::string::npos);
        Hestia::Logger::Log(message + "-after-destroy");
        EXPECT_EQ(Read(filePath), output);
    }
}
