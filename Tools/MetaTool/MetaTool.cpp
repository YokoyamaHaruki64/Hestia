/*=============================================================================

 File   : MetaTool.cpp
 Desc   : Assetルートを走査し、不足しているメタデータを作成する。

------------------------------------------------------------------------------

 Date   : 2026/11/25
 Author : Yokoyama Haruki

=============================================================================*/

#include "Common/Include/AssetMetadata.h"
#include "Common/Include/StringUtils.h"

#include <iostream>

int wmain(int argc, wchar_t* argv[])
{
    // 拡張子の引数を先に揃え、走査の入力として借用する
    std::vector<std::string> extensions;
    for (int index = 1; index < argc; ++index)
    {
        if (std::wstring_view(argv[index]) != L"--ext" || index + 1 >= argc)
        {
            std::cerr << "Usage: MetaTool [--ext .extension]...\n";
            return 2;
        }
        extensions.push_back(Hestia::PathToUtf8(argv[++index]));
    }

    std::vector<std::string_view> filter(extensions.begin(), extensions.end());
    std::vector<Hestia::AssetInfo> entries;
    std::vector<std::string> warnings;
    std::string error;
    const bool succeeded = Hestia::AssetMetadata::Scan("Assets", filter, entries, warnings, error);
    for (const auto& warning : warnings)
        std::cerr << "MetaTool: " << warning << '\n';
    if (!succeeded)
    {
        std::cerr << "MetaTool: " << error << '\n';
        return 1;
    }
    return 0;
}
