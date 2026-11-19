param(
    [ValidateSet('Debug_Editor', 'Release_Game')]
    [string[]]$Configuration = @('Debug_Editor', 'Release_Game'),

    [ValidateRange(1, 600)]
    [int]$TimeoutSeconds = 60
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$runId = '{0}-{1}' -f (Get-Date -Format 'yyyyMMdd-HHmmss'), [guid]::NewGuid().ToString('N').Substring(0, 8)
$resultRoot = Join-Path $repositoryRoot "Test\Results\GameDll-$runId"
[System.IO.Directory]::CreateDirectory($resultRoot) | Out-Null
Write-Output "Logs: $resultRoot"

# 非表示で起動した App のウィンドウを PID とクラス名で特定して閉じる。
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;

public static class HestiaGameDllSmokeWindow
{
    [DllImport("user32.dll", CharSet = CharSet.Unicode, ExactSpelling = true)]
    private static extern IntPtr FindWindowExW(IntPtr parent, IntPtr after, string className, string windowName);

    [DllImport("user32.dll")]
    private static extern uint GetWindowThreadProcessId(IntPtr window, out uint processId);

    [DllImport("user32.dll", ExactSpelling = true)]
    private static extern bool PostMessageW(IntPtr window, uint message, IntPtr wParam, IntPtr lParam);

    public static bool TryClose(uint processId)
    {
        IntPtr window = IntPtr.Zero;
        while ((window = FindWindowExW(IntPtr.Zero, window, "Hestia Application", null)) != IntPtr.Zero)
        {
            uint owner;
            GetWindowThreadProcessId(window, out owner);
            if (owner == processId)
                return PostMessageW(window, 0x0010, IntPtr.Zero, IntPtr.Zero);
        }
        return false;
    }
}
'@

function Read-EngineLog {
    param([string]$Directory)

    if (-not (Test-Path -LiteralPath $Directory)) {
        return ''
    }

    $logs = Get-ChildItem -LiteralPath $Directory -File -Filter '*.txt'
    return ($logs | ForEach-Object {
        Get-Content -LiteralPath $_.FullName -Raw -ErrorAction SilentlyContinue
    }) -join [Environment]::NewLine
}

function Read-PeTable {
    param(
        [string]$Option,
        [string]$Binary,
        [string]$Log
    )

    $lines = & $dumpbin /nologo $Option $Binary
    if ($LASTEXITCODE -ne 0) {
        throw "dumpbin $Option failed for $Binary"
    }
    $lines | Set-Content -LiteralPath $Log -Encoding UTF8
    return $lines -join [Environment]::NewLine
}

function Invoke-BinaryCheck {
    param(
        [string]$BinaryDirectory,
        [string]$OutputDirectory
    )

    $game = Join-Path $BinaryDirectory 'Game.dll'
    $engine = Join-Path $BinaryDirectory 'Engine.dll'
    $gameExports = Read-PeTable -Option /exports -Binary $game -Log (Join-Path $OutputDirectory 'Game-exports.log')
    if ($gameExports -notmatch '(?m)^\s*\d+\s+[0-9A-Fa-f]+\s+[0-9A-Fa-f]+\s+GetGameRuntimeAPI\b') {
        throw 'Game.dll does not export GetGameRuntimeAPI'
    }

    $gameImports = Read-PeTable -Option /imports -Binary $game -Log (Join-Path $OutputDirectory 'Game-imports.log')
    $engineImports = Read-PeTable -Option /imports -Binary $engine -Log (Join-Path $OutputDirectory 'Engine-imports.log')
    if ($gameImports -match '(?im)^\s*Engine\.dll\s*$' -or $engineImports -match '(?im)^\s*Game\.dll\s*$') {
        throw 'Unexpected static DLL dependency between Game.dll and Engine.dll'
    }
}

function Build-GameCoreProbe {
    param([string]$CaseRoot, [string]$BinaryDirectory)

    # 本番 Game.dll と同じ Runtime を Test 構成の静的ライブラリから組み込む。
    $gameLibrary = Join-Path $repositoryRoot 'Test\Build\lib\x64\Test\Game.lib'
    if (-not (Test-Path -LiteralPath $gameLibrary)) {
        throw 'Build the Test x64 configuration to generate Game.lib before running the Debug_Editor smoke cases'
    }

    $source = Join-Path $PSScriptRoot 'GameCoreSmokeProbe.cpp'
    $commonInclude = Join-Path $repositoryRoot 'Common\Include'
    $thirdPartyInclude = Join-Path $repositoryRoot 'ThirdParty\Include'
    $binary = Join-Path $BinaryDirectory 'Game.dll'
    $object = Join-Path $CaseRoot 'GameCoreSmokeProbe.obj'
    $importLibrary = Join-Path $CaseRoot 'GameCoreSmokeProbe.lib'
    $compileLog = Join-Path $CaseRoot 'compile.log'
    $command = 'call "{0}" -no_logo -arch=x64 -host_arch=x64 && cl.exe /nologo /LDd /MDd /std:c++latest /EHsc /utf-8 /DHESTIA_TEST=1 /I"{1}" /I"{2}" /I"{3}" "{4}" /Fo"{5}" /Fe"{6}" /link "{7}" /IMPLIB:"{8}" /EXPORT:GetGameRuntimeAPI=GetSmokeGameRuntimeAPI' -f
        $developerCommand, $repositoryRoot, $commonInclude, $thirdPartyInclude, $source, $object, $binary, $gameLibrary, $importLibrary
    & $env:ComSpec /d /s /c $command > $compileLog 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "Game core probe compile failed: $compileLog"
    }

    # 実行される試験用 Game.dll 自体の import にも Engine.dll がないことを確認する。
    $imports = Read-PeTable -Option /imports -Binary $binary -Log (Join-Path $CaseRoot 'Game-imports.log')
    if ($imports -match '(?im)^\s*Engine\.dll\s*$') {
        throw 'Game core probe imports Engine.dll'
    }
}

function Invoke-AppCase {
    param(
        [string]$BuildConfiguration,
        [string]$BinaryDirectory,
        [string]$CaseRoot,
        [string]$CaseName,
        [int]$ExpectedExitCode,
        [string[]]$ExpectedMessages
    )

    $bin = Join-Path $CaseRoot 'bin'
    $logDirectory = Join-Path $CaseRoot 'Logs'
    $probeLog = Join-Path $bin 'GameCoreProbe.log'
    $usesProbe = $CaseName -eq 'core-probe'
    $row = [ordered]@{
        configuration = $BuildConfiguration
        case = $CaseName
        passed = $false
        exitCode = $null
        expectedExitCode = $ExpectedExitCode
        logDirectory = $logDirectory
        failure = $null
    }
    $process = $null

    try {
        # App・DLL・ログをケースごとに分離し、Engineの相対Assetsパスを保つ。
        [System.IO.Directory]::CreateDirectory($bin) | Out-Null
        New-Item -ItemType Junction -Path (Join-Path $CaseRoot 'Assets') -Value (Join-Path $repositoryRoot 'Assets') | Out-Null
        foreach ($file in @('App.exe', 'Engine.dll', 'dxcompiler.dll', 'dxil.dll')) {
            Copy-Item -LiteralPath (Join-Path $BinaryDirectory $file) -Destination $bin
        }
        if ($CaseName -eq 'normal') {
            Copy-Item -LiteralPath (Join-Path $BinaryDirectory 'Game.dll') -Destination $bin
        }
        elseif ($usesProbe) {
            Build-GameCoreProbe -CaseRoot $CaseRoot -BinaryDirectory $bin
        }
        elseif ($CaseName -eq 'missing-export') {
            Copy-Item -LiteralPath (Join-Path $BinaryDirectory 'Engine.dll') -Destination (Join-Path $bin 'Game.dll')
        }

        # フレーム完了を待ち、プローブでは EndFrame の記録を確認してから WM_CLOSE を送る。
        $launch = @{
            FilePath = Join-Path $bin 'App.exe'
            WorkingDirectory = $bin
            WindowStyle = 'Hidden'
            RedirectStandardOutput = Join-Path $CaseRoot 'stdout.log'
            RedirectStandardError = Join-Path $CaseRoot 'stderr.log'
            PassThru = $true
        }
        $process = Start-Process @launch
        # Windows PowerShell で子プロセスの終了コードを読むまでハンドルを保持する。
        $null = $process.Handle
        if ($CaseName -eq 'normal' -or $usesProbe) {
            $timer = [Diagnostics.Stopwatch]::StartNew()
            $closePosted = $false
            while (-not $process.HasExited -and $timer.Elapsed.TotalSeconds -lt $TimeoutSeconds) {
                $logText = Read-EngineLog -Directory $logDirectory
                $probeReady = -not $usesProbe -or
                    (Get-Content -LiteralPath $probeLog -ErrorAction SilentlyContinue) -contains 'Frame probe passed'
                if ($probeReady -and $logText.Contains('FrameExecute: DeltaTime=') -and
                    [HestiaGameDllSmokeWindow]::TryClose([uint32]$process.Id)) {
                    $closePosted = $true
                    break
                }
                Start-Sleep -Milliseconds 50
            }
            if (-not $closePosted) {
                $waitTarget = if ($usesProbe) { "a rendered frame and the EndFrame marker in $probeLog" } else { 'a rendered frame' }
                throw "App exited or timed out while waiting for $waitTarget before WM_CLOSE"
            }
        }

        if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
            throw 'App did not exit before the timeout'
        }
        $row.exitCode = $process.ExitCode
        if ($row.exitCode -ne $ExpectedExitCode) {
            throw "App exit code $($row.exitCode), expected $ExpectedExitCode"
        }

        $logText = Read-EngineLog -Directory $logDirectory
        foreach ($message in $ExpectedMessages) {
            if (-not $logText.Contains($message)) {
                throw "Missing Engine log message: $message"
            }
        }

        # WM_CLOSE 後は Runtime の終了まで残した Script の OnDestroy を確認する。
        if ($usesProbe -and (Get-Content -LiteralPath $probeLog) -notcontains 'Shutdown probe passed') {
            throw "Missing shutdown marker: $probeLog"
        }
        $row.passed = $true
    }
    catch {
        $row.failure = $_.Exception.Message
    }
    finally {
        if ($null -ne $process) {
            if (-not $process.HasExited) {
                $process.Kill()
                $process.WaitForExit()
            }
            $row.exitCode = $process.ExitCode
            $process.Dispose()
        }
    }

    $result = if ($row.passed) { 'PASS' } else { "FAIL $($row.failure)" }
    Write-Host "$BuildConfiguration $CaseName $result"
    return $row
}

$programFilesX86 = [Environment]::GetFolderPath([Environment+SpecialFolder]::ProgramFilesX86)
$vswhere = Join-Path $programFilesX86 'Microsoft Visual Studio\Installer\vswhere.exe'
$installation = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath |
    Select-Object -First 1
if (-not $installation) { throw 'Visual Studio C++ tools are unavailable' }
$developerCommand = Join-Path $installation 'Common7\Tools\VsDevCmd.bat'
$dumpbin = Get-ChildItem -LiteralPath (Join-Path $installation 'VC\Tools\MSVC') -Directory |
    Sort-Object Name -Descending |
    ForEach-Object { Join-Path $_.FullName 'bin\Hostx64\x64\dumpbin.exe' } |
    Where-Object { Test-Path -LiteralPath $_ } |
    Select-Object -First 1
if (-not $dumpbin) { throw 'dumpbin.exe is unavailable' }

$results = @()
foreach ($buildConfiguration in $Configuration) {
    $outputDirectory = Join-Path $resultRoot $buildConfiguration
    [System.IO.Directory]::CreateDirectory($outputDirectory) | Out-Null
    $binaryDirectory = Join-Path $repositoryRoot "Build\bin\x64\$buildConfiguration"
    Invoke-BinaryCheck -BinaryDirectory $binaryDirectory -OutputDirectory $outputDirectory

    $cases = @(
        @{ name = 'normal'; exitCode = 0; messages = @('GameRuntime initialized', 'GameRuntime finalized') }
        @{ name = 'missing-dll'; exitCode = -1; messages = @('Game.dll: load failed') }
        @{ name = 'missing-export'; exitCode = -1; messages = @('Game.dll: GetGameRuntimeAPI export is missing') }
    )
    if ($buildConfiguration -eq 'Debug_Editor') {
        $cases += @{ name = 'core-probe'; exitCode = 0; messages = @('GameRuntime initialized', 'GameRuntime finalized') }
    }
    foreach ($case in $cases) {
        $parameters = @{
            BuildConfiguration = $buildConfiguration
            BinaryDirectory = $binaryDirectory
            CaseRoot = Join-Path $outputDirectory $case.name
            CaseName = $case.name
            ExpectedExitCode = $case.exitCode
            ExpectedMessages = $case.messages
        }
        $results += Invoke-AppCase @parameters
    }
}

ConvertTo-Json -InputObject $results -Depth 4 |
    Set-Content -LiteralPath (Join-Path $resultRoot 'results.json') -Encoding UTF8
if ($results | Where-Object { -not $_.passed }) {
    exit 1
}
