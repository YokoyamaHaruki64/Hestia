/*=============================================================================

 File   : ScriptWatcher.cpp
 Desc   : 保存通知をまとめ、Gameプロジェクトの登録一覧に沿ってScriptを再生成する

------------------------------------------------------------------------------

 Date   : 2026/11/26
 Author : Yokoyama Haruki

=============================================================================*/

#include "ScriptGeneration.h"
#include "Common/Include/StringUtils.h"
#include "Common/Include/WindowsHeaders.h"
#include "RapidYaml/rapidyaml.hpp"

#include <array>
#include <atomic>
#include <cstdio>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace
{
    using Path = std::filesystem::path;
    using WinHandle = std::unique_ptr<void, decltype(&CloseHandle)>;
    constexpr DWORD SAVE_DELAY_MS = 200;
    std::atomic_bool s_stopRequested = false;

    BOOL WINAPI RequestStop(DWORD control)
    {
        if (control != CTRL_C_EVENT && control != CTRL_BREAK_EVENT)
            return FALSE;

        s_stopRequested = true;
        return TRUE;
    }

    class DirectoryWatch
    {
    public:
        ~DirectoryWatch()
        {
            // OSがバッファを使用し終えるまで待ってから監視対象を解放する
            if (m_pending)
            {
                CancelIoEx(m_directory.get(), &m_read);
                DWORD bytes = 0;
                GetOverlappedResult(m_directory.get(), &m_read, &bytes, TRUE);
            }
        }

        bool Start(const Path& path, bool recursive)
        {
            // 非同期読み取りで編集中の通知も保持する
            const auto handle = CreateFileW(path.c_str(), FILE_LIST_DIRECTORY,
                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
                FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, nullptr);
            if (handle == INVALID_HANDLE_VALUE)
                return false;

            m_directory.reset(handle);
            m_event.reset(CreateEventW(nullptr, TRUE, FALSE, nullptr));
            m_read.hEvent = m_event.get();
            m_recursive = recursive;
            return m_event && Arm();
        }

        HANDLE Event() const
        {
            return m_event.get();
        }

        bool Read(std::vector<Path>& paths, bool& dropped)
        {
            // 完了済み通知を取り出し、生成が始まる前に次の読み取りを受け付ける
            DWORD bytes = 0;
            const bool completed = GetOverlappedResult(m_directory.get(), &m_read, &bytes, FALSE) != FALSE;
            m_pending = false;
            if (!completed && GetLastError() != ERROR_NOTIFY_ENUM_DIR)
                return false;

            dropped = bytes == 0;
            for (DWORD offset = 0; offset < bytes;)
            {
                const auto& event = *reinterpret_cast<const FILE_NOTIFY_INFORMATION*>(m_buffer.data() + offset);
                paths.emplace_back(std::wstring_view(event.FileName, event.FileNameLength / sizeof(wchar_t)));
                if (event.NextEntryOffset == 0)
                    break;
                offset += event.NextEntryOffset;
            }
            return Arm();
        }

    private:
        bool Arm()
        {
            ResetEvent(m_event.get());
            m_pending = ReadDirectoryChangesW(m_directory.get(), m_buffer.data(),
                static_cast<DWORD>(m_buffer.size()), m_recursive,
                FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME |
                FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SIZE,
                nullptr, &m_read, nullptr) != FALSE;
            return m_pending;
        }

        WinHandle m_directory{nullptr, CloseHandle};
        WinHandle m_event{nullptr, CloseHandle};
        OVERLAPPED m_read{};
        alignas(DWORD) std::array<std::byte, 32 * 1024> m_buffer{};
        bool m_recursive = false;
        bool m_pending = false;
    };

    bool ReadProjectInputs(std::vector<Path>& inputs)
    {
        // プロジェクトの評価結果を使い、ビルド時と同じ一覧と登録順を得る
        constexpr wchar_t command[] = L"MSBuild.exe Game\\Game.vcxproj -nologo -getItem:_GameCppScript "
            L"-p:Configuration=Debug_Editor -p:Platform=x64 2>&1";
        std::unique_ptr<FILE, decltype(&_pclose)> pipe(_wpopen(command, L"rb"), _pclose);
        if (!pipe)
        {
            std::cerr << "Game.vcxproj: error: Run the watcher from Visual Studio Developer PowerShell\n";
            return false;
        }

        std::string output;
        std::array<char, 4096> buffer;
        size_t size = 0;
        while ((size = std::fread(buffer.data(), 1, buffer.size(), pipe.get())) != 0)
            output.append(buffer.data(), size);
        if (_pclose(pipe.release()) != 0)
        {
            std::cerr << "Game.vcxproj: error: MSBuild could not evaluate the Script list\n" << output;
            return false;
        }

        // 評価中のプロジェクト保存などで取得に失敗した場合は以前の一覧を保持する
        struct ParseError {};
        auto callbacks = ryml::get_callbacks();
        callbacks.set_error_parse([](ryml::csubstr, const ryml::ErrorDataParse&, void*) { throw ParseError{}; });
        ryml::EventHandlerTree handler(callbacks);
        ryml::Parser parser(&handler);
        ryml::Tree tree;
        try
        {
            auto json = std::string_view(output);
            if (json.starts_with("\xef\xbb\xbf"))
                json.remove_prefix(3);
            tree = ryml::parse_json_in_arena(&parser, ryml::csubstr(json.data(), json.size()));
        }
        catch (const ParseError&)
        {
            std::cerr << "Game.vcxproj: error: Could not read MSBuild's Script list\n";
            return false;
        }

        // 抽出条件はMSBuildに任せ、Asset相対パスだけを順に取得する
        const auto items = tree.crootref()["Items"]["_GameCppScript"];
        if (!items.readable() || !items.is_seq())
        {
            std::cerr << "Game.vcxproj: error: Missing _GameCppScript in MSBuild's Script list\n";
            return false;
        }

        std::vector<Path> evaluated;
        evaluated.reserve(items.num_children());
        for (const auto item : items.children())
        {
            const auto path = item["AssetPath"];
            if (!path.readable() || !path.has_val())
            {
                std::cerr << "Game.vcxproj: error: Missing AssetPath in MSBuild's Script list\n";
                return false;
            }
            const auto value = path.val();
            evaluated.push_back(Hestia::PathFromUtf8({value.str, value.len}).lexically_normal());
        }
        inputs = std::move(evaluated);
        return true;
    }

    bool SamePath(const Path& left, const Path& right)
    {
        const ScriptPathLess less;
        return !less(left, right) && !less(right, left);
    }

    bool CollectScriptChanges(Path path, const ScriptPaths& registered, ScriptPaths& changed)
    {
        // 隣接する.metaのイベントも元Scriptのパスにまとめる
        if (SamePath(path.extension(), L".meta"))
            path.replace_extension();
        if (const auto script = registered.find(path); script != registered.end())
        {
            changed.insert(*script);
            return true;
        }

        // 親フォルダの移動・削除では、個々のScriptへのイベントが届かないことがある
        bool affected = false;
        for (const auto& script : registered)
        {
            for (auto parent = script.parent_path(); !parent.empty(); parent = parent.parent_path())
            {
                if (SamePath(parent, path))
                {
                    changed.insert(script);
                    affected = true;
                    break;
                }
            }
        }
        return affected;
    }
}

int WatchScripts(const ScriptGenerationPaths& paths)
{
    // 初回照合の前からAssets・プロジェクト設定の変更を受け付ける
    const std::array directories{paths.m_assets, Path("Game"), Path("Props")};
    std::array<DirectoryWatch, directories.size()> watches;
    std::array<HANDLE, watches.size()> events;
    for (size_t index = 0; index < watches.size(); ++index)
    {
        if (!watches[index].Start(directories[index], index == 0))
        {
            std::cerr << Hestia::PathToUtf8(directories[index]) << ": error: Could not start directory watch\n";
            return 1;
        }
        events[index] = watches[index].Event();
    }
    if (!SetConsoleCtrlHandler(RequestStop, TRUE))
    {
        std::cerr << "error: Could not set the Ctrl+C handler\n";
        return 1;
    }

    // 一覧を更新しても、変更していないScriptの差分キャッシュは再利用する
    std::vector<Path> inputs;
    ScriptPaths registered;
    ScriptPaths changed;
    bool refreshProject = true;
    bool inspectAll = false;
    const auto generate = [&]()
    {
        if (refreshProject)
        {
            if (!ReadProjectInputs(inputs))
                return;
            registered = ScriptPaths(inputs.begin(), inputs.end());
            refreshProject = false;
        }
        if (GenerateScripts(paths, inputs, ScriptMetadataPreparation::Missing, inspectAll ? registered : changed))
        {
            inspectAll = false;
            changed.clear();
        }
    };

    std::cout << "Watching Game scripts. Press Ctrl+C to stop.\n" << std::flush;
    generate();

    // エディターの保存に伴う複数の通知をまとめて1回生成する
    ULONGLONG deadline = 0;
    while (!s_stopRequested)
    {
        const auto now = GetTickCount64();
        if (deadline != 0 && now >= deadline)
        {
            deadline = 0;
            generate();
            continue;
        }

        const auto timeout = deadline == 0 ? SAVE_DELAY_MS : static_cast<DWORD>(deadline - now);
        const auto event = WaitForMultipleObjects(static_cast<DWORD>(events.size()), events.data(), FALSE, timeout);
        if (event == WAIT_TIMEOUT)
            continue;

        const auto index = event - WAIT_OBJECT_0;
        std::vector<Path> notifications;
        bool dropped = false;
        if (index >= watches.size() || !watches[index].Read(notifications, dropped))
        {
            std::cerr << "error: Directory watch could not read save notifications\n";
            return 1;
        }

        // 欠落した通知があれば一覧と全Scriptを確認し直す
        bool relevant = dropped;
        if (dropped)
        {
            refreshProject = true;
            inspectAll = true;
        }
        for (auto& notification : notifications)
        {
            if (index == 0)
                relevant |= CollectScriptChanges(notification.lexically_normal(), registered, changed);
            else if (SamePath(notification, index == 1 ? L"Game.vcxproj" : L"GameScripts.props"))
            {
                refreshProject = true;
                relevant = true;
            }
        }
        if (relevant)
            deadline = GetTickCount64() + SAVE_DELAY_MS;
    }
    return 0;
}
