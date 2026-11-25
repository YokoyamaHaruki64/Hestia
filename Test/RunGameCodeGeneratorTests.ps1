param(
    [string]$BinaryDirectory
)

$ErrorActionPreference = 'Stop'
$toolPath = Join-Path $PSScriptRoot '..\Tools\MetaTool\MetaTool.exe'
$generatorPath = Join-Path $PSScriptRoot '..\Tools\GameCodeGenerator\GameCodeGenerator.exe'
if ($BinaryDirectory) {
    $toolPath = Join-Path $BinaryDirectory 'MetaTool.exe'
    $generatorPath = Join-Path $BinaryDirectory 'GameCodeGenerator.exe'
}
$toolPath = [IO.Path]::GetFullPath($toolPath)
$generatorPath = [IO.Path]::GetFullPath($generatorPath)
$root = Join-Path ([IO.Path]::GetTempPath()) ('HestiaGenerator-' + [Guid]::NewGuid().ToString('N'))
$assets = Join-Path $root 'Assets'
$actors = Join-Path $assets 'Actors'
$output = Join-Path $root 'Generated'
$listing = Join-Path $root 'Scripts.txt'
$encoding = [Text.UTF8Encoding]::new($false)

function Assert-Test($condition, [string]$message) {
    if (-not $condition) { throw $message }
}

New-Item -ItemType Directory -Path $actors | Out-Null
Push-Location $root
try {
    $source = Join-Path $actors 'Spin.cpp'
    [IO.File]::WriteAllText($source, @'
SCRIPT_TYPE HestiaGame::Spin
HEADER_BEGIN
namespace HestiaGame { struct Spin {}; }
HEADER_END
SOURCE_BEGIN
SOURCE_END
'@, $encoding)
    [IO.File]::WriteAllText((Join-Path $assets 'Skip.png'), '', $encoding)
    [IO.File]::WriteAllText($listing, "Actors/Spin.cpp`n", $encoding)

    # 拡張子を絞ってメタデータを生成し、Scriptの型登録に同じGUIDを出す
    & $toolPath --ext .cpp
    Assert-Test ($LASTEXITCODE -eq 0) 'MetaTool failed'
    Assert-Test (-not (Test-Path (Join-Path $assets 'Skip.png.meta'))) 'Extension filter was ignored'
    $meta = Get-Content -LiteralPath "$source.meta" -Raw
    $guidMatch = [regex]::Match($meta, '(?m)^guid:\s*([0-9a-f-]{36})')
    Assert-Test ($guidMatch.Success) 'Script metadata has no GUID'
    & $generatorPath $assets $listing $output
    Assert-Test ($LASTEXITCODE -eq 0) 'Generator failed on valid source'
    $setup = Join-Path $output 'GameSetup.generated.cpp'
    $header = Join-Path $output 'Scripts\Actors\Spin.generated.h'
    Assert-Test ((Get-Content -Raw $setup).Contains($guidMatch.Groups[1].Value)) 'Generated registration lost the GUID'
    Assert-Test ((Get-Content -Raw $header).Contains('#line 3')) 'Source line mapping was lost'

    # 同じ入力で生成物を書き換えず、別の型に同じGUIDが割り当てられたら拒否する
    $oldTimestamp = [DateTime]::new(2020, 1, 1, 0, 0, 0, [DateTimeKind]::Utc)
    [IO.File]::SetLastWriteTimeUtc($setup, $oldTimestamp)
    & $generatorPath $assets $listing $output
    Assert-Test ($LASTEXITCODE -eq 0) 'Repeat generation failed'
    Assert-Test ([IO.File]::GetLastWriteTimeUtc($setup) -eq $oldTimestamp) 'Identical output was rewritten'
    [IO.File]::WriteAllText((Join-Path $actors 'Other.cpp'),
        ([IO.File]::ReadAllText($source).Replace('Spin', 'Other')), $encoding)
    [IO.File]::WriteAllText((Join-Path $actors 'Other.cpp.meta'), $meta, $encoding)
    [IO.File]::AppendAllText($listing, "Actors/Other.cpp`n", $encoding)
    & $generatorPath $assets $listing $output
    Assert-Test ($LASTEXITCODE -ne 0) 'Duplicate GUID was accepted'
    Assert-Test ([IO.File]::GetLastWriteTimeUtc($setup) -eq $oldTimestamp) 'Invalid inputs changed existing outputs'
    Write-Output 'GameCodeGenerator contracts passed'
}
finally {
    Pop-Location
    $resolved = (Resolve-Path -LiteralPath $root).Path
    $tempRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath())
    if ($resolved.StartsWith($tempRoot, [StringComparison]::OrdinalIgnoreCase) -and
        (Split-Path -Leaf $resolved).StartsWith('HestiaGenerator-')) {
        Remove-Item -LiteralPath $resolved -Recurse -Force
    }
}

exit 0
