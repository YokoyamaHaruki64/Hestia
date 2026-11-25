/*=============================================================================

 File   : GameCodeGenerator.cpp
 Desc   : Script Assetの.cppからC++ソースとGUID付きScript登録を生成する。

------------------------------------------------------------------------------

 Date   : 2026/11/25
 Author : Yokoyama Haruki

=============================================================================*/

#include "Common/Include/AssetMetadata.h"
#include "Common/Include/StringUtils.h"
#include "Common/Include/WindowsHeaders.h"
#include "ScriptGeneration.h"

#include <algorithm>
#include <format>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <process.h>
#include <regex>
#include <sstream>
#include <unordered_set>

namespace
{
    using Path = std::filesystem::path;

    struct FileStamp
    {
        uint64_t m_time;
        uint64_t m_size;

        bool operator==(const FileStamp&) const = default;
    };

    struct CachedScript
    {
        FileStamp m_source{};
        FileStamp m_meta{};
        std::string m_type;
        Hestia::AssetGuid m_guid;
    };

    using ScriptCache = std::map<std::string, CachedScript>;

    struct ScriptSource
    {
        Path m_path;
        CachedScript m_record;
        std::string m_header;
        std::string m_source;
        size_t m_headerLine = 0;
        size_t m_sourceLine = 0;
        bool m_needsOutput = true;
    };

    bool ReportError(const Path& path, std::string_view error)
    {
        std::cerr << std::format("{}: error: {}\n", Hestia::PathToUtf8(path), error);
        return false;
    }

    bool IsScriptType(const std::string& name)
    {
        static const std::regex syntax("(::)?[A-Za-z_][A-Za-z_0-9]*(::[A-Za-z_][A-Za-z_0-9]*)*");
        return std::regex_match(name, syntax);
    }

    std::string_view Trim(std::string_view text)
    {
        const size_t first = text.find_first_not_of(" \r\t");
        if (first == std::string_view::npos)
            return {};

        return text.substr(first, text.find_last_not_of(" \r\t") - first + 1);
    }

    bool ReadLine(std::istream& input, std::string& line, size_t& lineNumber)
    {
        if (!std::getline(input, line))
            return false;

        ++lineNumber;
        if (lineNumber == 1 && line.starts_with("\xef\xbb\xbf"))
            line.erase(0, 3);
        if (line.ends_with('\r'))
            line.pop_back();
        return true;
    }

    bool ReadDirective(std::istream& input, size_t& lineNumber, std::string& line)
    {
        while (ReadLine(input, line, lineNumber))
        {
            if (!Trim(line).empty())
                return true;
        }
        return false;
    }

    bool ReadBlock(std::istream& input, size_t& lineNumber, std::string_view end,
        size_t& firstLine, std::string& body)
    {
        // 最初のC++行を記録し、診断を元ファイルの行へ戻せるようにする
        firstLine = lineNumber + 1;
        std::string line;
        while (ReadLine(input, line, lineNumber))
        {
            const auto directive = Trim(line);
            if (directive == end)
                return true;
            if (directive == "HEADER_BEGIN" || directive == "HEADER_END" ||
                directive == "SOURCE_BEGIN" || directive == "SOURCE_END")
                return false;

            body += line;
            body += '\n';
        }
        return false;
    }

    bool ReadScript(ScriptSource& script)
    {
        const auto path = Hestia::PathToUtf8(script.m_path);
        std::ifstream input(script.m_path, std::ios::binary);
        size_t lineNumber = 0;
        const auto fail = [&](std::string_view message)
        {
            std::cerr << std::format("{}({}): error: {}\n", path, lineNumber, message);
            return false;
        };
        if (!input)
            return fail("Could not open Script .cpp");

        // 通常はファイル名を型名に使い、先頭に型指定がある場合だけ上書きする
        std::string line;
        if (!ReadDirective(input, lineNumber, line))
            return fail("Expected HEADER_BEGIN or SCRIPT_TYPE(Type)");

        auto& typeName = script.m_record.m_type;
        typeName = Hestia::PathToUtf8(script.m_path.stem());
        auto typeLine = Trim(line);
        if (typeLine != "HEADER_BEGIN")
        {
            if (typeLine.starts_with("//"))
                typeLine = Trim(typeLine.substr(2));
            constexpr std::string_view typeDirective = "SCRIPT_TYPE";
            if (!typeLine.starts_with(typeDirective))
                return fail("Expected HEADER_BEGIN or SCRIPT_TYPE(Type)");

            const auto argument = typeLine.substr(typeDirective.size());
            const auto type = Trim(argument);
            if (type.size() >= 2 && type.front() == '(' && type.back() == ')')
                typeName = Trim(type.substr(1, type.size() - 2));
            else if (argument.starts_with(' '))
            {
                // 従来の空白区切りでは、従来どおりグローバル名前空間を起点とする
                typeName = type;
                if (!typeName.starts_with("::"))
                    typeName.insert(0, "::");
            }
            else
                return fail("Expected SCRIPT_TYPE(Type)");

            if (!ReadDirective(input, lineNumber, line))
                return fail("Expected HEADER_BEGIN after SCRIPT_TYPE");
        }

        // ディレクティブだけを検証し、ブロック内のC++はコンパイラに渡す
        if (!IsScriptType(typeName))
            return fail("Invalid Script type name; use a C++ identifier as the filename or specify SCRIPT_TYPE(Type)");

        if (Trim(line) != "HEADER_BEGIN" ||
            !ReadBlock(input, lineNumber, "HEADER_END", script.m_headerLine, script.m_header))
            return fail("Expected matching HEADER_BEGIN and HEADER_END");
        if (!ReadDirective(input, lineNumber, line) || Trim(line) != "SOURCE_BEGIN" ||
            !ReadBlock(input, lineNumber, "SOURCE_END", script.m_sourceLine, script.m_source))
            return fail("Expected matching SOURCE_BEGIN and SOURCE_END");
        if (ReadDirective(input, lineNumber, line) || input.bad())
            return fail("Unexpected input after SOURCE_END");

        Hestia::AssetInfo metadata;
        std::string error;
        if (!Hestia::AssetMetadata::Read(script.m_path, metadata, error))
            return fail(error);

        script.m_record.m_guid = metadata.m_guid;
        return true;
    }

    bool WriteChanged(const Path& path, const std::string& content)
    {
        // 同じ生成内容ではタイムスタンプを維持し、C++の再コンパイルを避ける
        std::ifstream previous(path, std::ios::binary);
        if (previous)
        {
            const std::string previousContent((std::istreambuf_iterator<char>(previous)), {});
            if (previousContent == content)
                return true;
            previous.close();
        }

        // 読み込み側から途中の内容が見えないよう、同じフォルダで置き換える
        std::error_code error;
        std::filesystem::create_directories(path.parent_path(), error);
        if (error)
            return ReportError(path, error.message());

        auto temporary = path;
        temporary += L"." + std::to_wstring(GetCurrentProcessId()) + L".tmp";
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        output << content;
        output.close();
        if (output && MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING))
            return true;

        std::filesystem::remove(temporary, error);
        return ReportError(path, "Could not replace generated file");
    }

    std::optional<FileStamp> ReadFileStamp(const Path& path)
    {
        WIN32_FILE_ATTRIBUTE_DATA data{};
        if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data) ||
            (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            return std::nullopt;

        return FileStamp{
            (uint64_t{data.ftLastWriteTime.dwHighDateTime} << 32) | data.ftLastWriteTime.dwLowDateTime,
            (uint64_t{data.nFileSizeHigh} << 32) | data.nFileSizeLow
        };
    }

    std::string CacheIdentity(const Path& assets)
    {
        wchar_t module[32768];
        const auto length = GetModuleFileNameW(nullptr, module, static_cast<DWORD>(std::size(module)));
        if (length == 0 || length == std::size(module))
            return {};

        const auto executable = ReadFileStamp(module);
        if (!executable)
            return {};

        // 実行ファイルや生成コードに含むAsset絶対パスが変われば全件を再解析する
        return std::format("HESTIA_SCRIPT_CACHE 1 {} {} {}", executable->m_time, executable->m_size,
            Hestia::PathToUtf8(assets));
    }

    ScriptCache ReadCache(const Path& path, const std::string& identity)
    {
        std::ifstream input(path, std::ios::binary);
        std::string line;
        if (identity.empty() || !std::getline(input, line) || line != identity)
            return {};

        ScriptCache cache;
        while (std::getline(input, line))
        {
            std::istringstream record(line);
            std::string key;
            std::string guid;
            CachedScript script;
            if (!(record >> std::quoted(key) >> script.m_source.m_time >> script.m_source.m_size
                >> script.m_meta.m_time >> script.m_meta.m_size >> script.m_type >> guid) ||
                !(record >> std::ws).eof() || key.empty() || !IsScriptType(script.m_type))
                return {};

            script.m_guid = Hestia::AssetGuid::Parse(guid);
            if (!script.m_guid || !cache.emplace(std::move(key), std::move(script)).second)
                return {};
        }
        if (input.bad())
            return {};

        return cache;
    }

    bool WriteCache(const Path& path, const std::string& identity, const ScriptCache& cache)
    {
        // パス順で保存し、登録順だけの変更でキャッシュを書き換えない
        std::ostringstream manifest;
        manifest << identity << '\n';
        for (const auto& [relative, record] : cache)
        {
            manifest << std::quoted(relative) << ' '
                << record.m_source.m_time << ' ' << record.m_source.m_size << ' '
                << record.m_meta.m_time << ' ' << record.m_meta.m_size << ' '
                << record.m_type << ' ' << record.m_guid.ToString() << '\n';
        }

        return WriteChanged(path, manifest.str());
    }

    std::string_view ScriptTypeKey(std::string_view type)
    {
        // HestiaGame内の型は、短い名前と明示的な修飾を同じ名前として検証する
        constexpr std::string_view gameScope = "HestiaGame::";
        if (type.starts_with("::HestiaGame::"))
            type.remove_prefix(2);
        if (type.starts_with(gameScope))
            type.remove_prefix(gameScope.size());
        return type;
    }

    Path GeneratedHeader(const Path& script)
    {
        auto header = Path("Scripts") / script;
        return header.replace_extension(".generated.h");
    }

    bool PrepareMetadata(const Path& assets, std::span<const Path> inputs, ScriptMetadataPreparation metadata)
    {
        if (metadata == ScriptMetadataPreparation::Existing)
            return true;

        // 監視中は登録されたScriptに不足メタデータがあるときだけAssetsを走査する
        if (metadata == ScriptMetadataPreparation::Missing && std::ranges::none_of(inputs, [&](const Path& input)
        {
            const auto path = assets / input;
            return ReadFileStamp(path) && !ReadFileStamp(Path(path).concat(L".meta"));
        }))
            return true;

        constexpr wchar_t tool[] = L"Tools\\MetaTool\\MetaTool.exe";
        const wchar_t* arguments[] = {tool, L"--ext", L".cpp", nullptr};
        return _wspawnv(_P_WAIT, tool, arguments) == 0 ||
            ReportError(tool, "MetaTool failed. Build MetaTool in Release|x64 before generating Game scripts");
    }

    bool RemoveObsoleteOutputs(const Path& output, const ScriptPaths& expected)
    {
        // キャッシュを作り直した場合も専用出力先を照合すれば削除済みのScriptを判定できる
        const auto scripts = output / "Scripts";
        std::error_code error;
        const bool exists = std::filesystem::exists(scripts, error);
        if (error)
            return ReportError(scripts, error.message());
        if (!exists)
            return true;

        std::filesystem::recursive_directory_iterator file(scripts, error);
        const std::filesystem::recursive_directory_iterator end;
        while (!error && file != end)
        {
            const auto& path = file->path();
            const auto& name = path.native();
            if ((name.ends_with(L".generated.h") || name.ends_with(L".generated.cpp")) &&
                !expected.contains(path) && file->is_regular_file(error))
            {
                std::filesystem::remove(path, error);
            }
            if (!error)
                file.increment(error);
        }
        return !error || ReportError(scripts, error.message());
    }

    bool IsScriptPath(const Path& relative)
    {
        return !relative.empty() && !relative.has_root_path() && *relative.begin() != L".." &&
            Hestia::ToLowerAscii(Hestia::PathToUtf8(relative.extension())) == ".cpp";
    }

    bool ReadInputList(const Path& path, std::vector<Path>& inputs)
    {
        std::ifstream list(path, std::ios::binary);
        if (!list)
            return ReportError(path, "Could not open input list");

        std::string line;
        size_t lineNumber = 0;
        while (ReadLine(list, line, lineNumber))
        {
            const auto name = Trim(line);
            if (name.empty())
                continue;

            auto relative = Hestia::PathFromUtf8(name).lexically_normal();
            if (!IsScriptPath(relative))
            {
                std::cerr << std::format("{}({}): error: Expected an Asset-relative .cpp path\n",
                    Hestia::PathToUtf8(path), lineNumber);
                return false;
            }
            inputs.push_back(std::move(relative));
        }
        return !list.bad() || ReportError(path, "Could not read input list");
    }
}

bool ScriptPathLess::operator()(const Path& left, const Path& right) const
{
    return CompareStringOrdinal(left.c_str(), -1, right.c_str(), -1, TRUE) == CSTR_LESS_THAN;
}

bool GenerateScripts(const ScriptGenerationPaths& paths, std::span<const Path> inputs,
    ScriptMetadataPreparation metadata, const ScriptPaths& changed)
{
    // 監視とビルドの間で、メタデータ補完から出力の確定までを直列化する
    std::unique_ptr<void, decltype(&CloseHandle)> lock(nullptr, CloseHandle);
    if (!paths.m_cache.empty())
    {
        std::error_code error;
        std::filesystem::create_directories(paths.m_cache.parent_path(), error);
        if (error)
            return ReportError(paths.m_cache, error.message());

        const auto lockPath = paths.m_cache.parent_path() / "ScriptGeneration.lock";
        const auto handle = CreateFileW(lockPath.c_str(), GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (handle == INVALID_HANDLE_VALUE)
            return ReportError(lockPath, "Could not open generation lock");

        lock.reset(handle);
        OVERLAPPED range{};
        if (!LockFileEx(lock.get(), LOCKFILE_EXCLUSIVE_LOCK, 0, 1, 0, &range))
            return ReportError(lockPath, "Could not acquire generation lock");
    }

    // 入力パスを確認し、生成先が同じになる重複登録もここで拒否する
    ScriptPaths inputPaths;
    for (const auto& relative : inputs)
    {
        if (!IsScriptPath(relative))
            return ReportError(relative, "Expected an Asset-relative .cpp path");
        if (!inputPaths.insert(relative).second)
            return ReportError(relative, "Duplicate Script path");
    }
    if (!PrepareMetadata(paths.m_assets, inputs, metadata))
        return false;

    // 変更がないScriptは出力と登録情報を再利用し、重複は登録対象全体で検証する
    const auto identity = paths.m_cache.empty() ? std::string{} : CacheIdentity(paths.m_assets);
    const auto cache = ReadCache(paths.m_cache, identity);
    std::vector<ScriptSource> scripts;
    scripts.reserve(inputs.size());
    std::unordered_set<std::string> types;
    std::unordered_set<Hestia::AssetGuid, Hestia::AssetGuid::Hash> guids;
    for (const auto& relative : inputs)
    {
        auto& script = scripts.emplace_back();
        script.m_path = paths.m_assets / relative;
        const auto metaPath = Path(script.m_path).concat(L".meta");
        const auto source = ReadFileStamp(script.m_path);
        if (!source)
            return ReportError(script.m_path, "Could not find Script .cpp");
        const auto meta = ReadFileStamp(metaPath);
        if (!meta)
            return ReportError(metaPath, "Could not find Script metadata");

        auto& record = script.m_record;
        record.m_source = *source;
        record.m_meta = *meta;
        const auto previous = cache.find(Hestia::PathToUtf8(relative));
        if (previous != cache.end() && previous->second.m_source == *source &&
            previous->second.m_meta == *meta && !changed.contains(relative))
        {
            auto header = paths.m_output / GeneratedHeader(relative);
            if (ReadFileStamp(header) && ReadFileStamp(header.replace_extension(".cpp")))
            {
                record = previous->second;
                script.m_needsOutput = false;
            }
        }

        if (script.m_needsOutput && !ReadScript(script))
            return false;
        if (!types.emplace(ScriptTypeKey(record.m_type)).second || !guids.insert(record.m_guid).second)
            return ReportError(script.m_path, "Duplicate Script type or GUID");
    }

    // 個別の生成コードと現在の登録順を反映し、全出力が揃ってからキャッシュを確定する
    const auto invalidateCache = [&]()
    {
        if (!paths.m_cache.empty())
        {
            std::error_code error;
            std::filesystem::remove(paths.m_cache, error);
            if (error)
                ReportError(paths.m_cache, error.message());
        }
        return false;
    };

    std::string includes = "#include \"Game/Include/GameSetup.h\"\n"
        "#include \"Game/Include/World.h\"\n#include <type_traits>\n";
    std::string registrations;
    ScriptPaths expected;
    ScriptCache updatedCache;
    for (size_t index = 0; index < scripts.size(); ++index)
    {
        auto& script = scripts[index];
        const auto& record = script.m_record;
        const auto& relative = inputs[index];
        const auto header = GeneratedHeader(relative);
        auto source = header;
        source.replace_extension(".cpp");
        if (script.m_needsOutput)
        {
            const auto original = Hestia::PathToUtf8(script.m_path);
            if (!WriteChanged(paths.m_output / header, std::format("#pragma once\n#line {} \"{}\"\n{}",
                    script.m_headerLine, original, script.m_header)) ||
                !WriteChanged(paths.m_output / source, std::format("#include \"{}\"\n#line {} \"{}\"\n{}",
                    Hestia::PathToUtf8(header.filename()), script.m_sourceLine, original, script.m_source)))
                return invalidateCache();
        }

        includes += std::format("#include \"{}\"\n", Hestia::PathToUtf8(header));
        registrations += std::format("        // GUID: {}\n"
            "        static_assert(std::is_default_constructible_v<{}>, "
            "\"Scene Script must be default constructible\");\n"
            "        if (!World::RegisterScript<{}>(Hestia::AssetGuid{{ {{ 0x{:016x}ull, 0x{:016x}ull }} }}))\n"
            "            return false;\n", record.m_guid.ToString(), record.m_type, record.m_type,
            record.m_guid.m_words[0], record.m_guid.m_words[1]);
        if (!paths.m_cache.empty())
        {
            expected.insert(paths.m_output / header);
            expected.insert(paths.m_output / source);
            updatedCache.emplace(Hestia::PathToUtf8(relative), std::move(script.m_record));
        }
    }

    const auto setup = includes + "\nnamespace HestiaGame::Setup\n{\n    bool RegisterScripts()\n    {\n" +
        registrations + "        return true;\n    }\n}\n";
    if (!WriteChanged(paths.m_output / "GameSetup.generated.cpp", setup))
        return invalidateCache();
    if (!paths.m_cache.empty() &&
        (!RemoveObsoleteOutputs(paths.m_output, expected) || !WriteCache(paths.m_cache, identity, updatedCache)))
        return invalidateCache();
    return true;
}

int wmain(int argc, wchar_t* argv[])
{
    const auto root = std::filesystem::current_path();
    const ScriptGenerationPaths projectPaths{
        root / "Assets", root / "GeneratedScript", root / "Cache" / "GameCodeGenerator" / "ScriptInputs.cache"
    };
    std::vector<Path> inputs;
    if (argc == 2 && std::wstring_view(argv[1]) == L"--watch")
        return WatchScripts(projectPaths);
    if (argc == 3 && std::wstring_view(argv[1]) == L"--project")
    {
        return ReadInputList(argv[2], inputs) &&
            GenerateScripts(projectPaths, inputs, ScriptMetadataPreparation::Scan) ? 0 : 1;
    }
    if (argc == 4)
    {
        return ReadInputList(argv[2], inputs) &&
            GenerateScripts({argv[1], argv[3], {}}, inputs, ScriptMetadataPreparation::Existing) ? 0 : 1;
    }

    std::cerr << "Usage: GameCodeGenerator --watch | --project <input-list> | "
        "<asset-root> <input-list> <output-directory>\n";
    return 2;
}
