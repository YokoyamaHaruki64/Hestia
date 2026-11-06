param(
    [ValidateSet('Debug_Editor', 'Release_Editor')]
    [string[]]$Configuration = @('Debug_Editor', 'Release_Editor')
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$source = Join-Path $PSScriptRoot 'TransformFacadeConsumer.cpp'
$runId = '{0}-{1}' -f (Get-Date -Format 'yyyyMMdd-HHmmss'), [guid]::NewGuid().ToString('N').Substring(0, 8)
$resultRoot = Join-Path $repositoryRoot "Test\Results\FacadeDll-$runId"
[System.IO.Directory]::CreateDirectory($resultRoot) | Out-Null
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$installation = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | Select-Object -First 1
if (-not $installation) { throw 'Visual Studio C++ tools are unavailable' }
$developerCommand = Join-Path $installation 'Common7\Tools\VsDevCmd.bat'
$results = @()

foreach ($buildConfiguration in $Configuration) {
    $directory = Join-Path $resultRoot $buildConfiguration
    [System.IO.Directory]::CreateDirectory($directory) | Out-Null
    $dllDirectory = Join-Path $repositoryRoot "Build\bin\x64\$buildConfiguration"
    $importLibrary = Join-Path $dllDirectory 'Engine.lib'
    if (-not (Test-Path -LiteralPath $importLibrary)) { throw "Build Engine $buildConfiguration first" }
    $binary = Join-Path $directory 'TransformFacadeConsumer.exe'
    $object = Join-Path $directory 'TransformFacadeConsumer.obj'
    $pdb = Join-Path $directory 'TransformFacadeConsumer.pdb'
    $include = Join-Path $repositoryRoot 'Engine\Include'
    $options = if ($buildConfiguration -eq 'Debug_Editor') { '/MDd /Od /D_DEBUG' } else { '/MD /O2 /DNDEBUG' }
    $command = 'call "{0}" -no_logo -arch=x64 -host_arch=x64 && cl.exe /nologo /std:c++latest /EHsc /utf-8 {1} /I"{2}" /I"{3}" "{4}" /Fo"{5}" /Fd"{6}" /Fe"{7}" /link "{8}"' -f $developerCommand, $options, $repositoryRoot, $include, $source, $object, $pdb, $binary, $importLibrary
    $compileLog = Join-Path $directory 'compile.log'
    & $env:ComSpec /d /s /c $command > $compileLog 2>&1
    $compileExitCode = $LASTEXITCODE
    if ($compileExitCode -ne 0) { throw "Consumer compile failed: $compileLog" }

    $stdout = Join-Path $directory 'stdout.log'
    $stderr = Join-Path $directory 'stderr.log'
    $process = Start-Process -FilePath $binary -WorkingDirectory $dllDirectory -WindowStyle Hidden `
        -RedirectStandardOutput $stdout -RedirectStandardError $stderr -Wait -PassThru
    $row = [ordered]@{ configuration = $buildConfiguration; compileExitCode = $compileExitCode; exitCode = $process.ExitCode; debugAssertExitCode = $null }
    if ($process.ExitCode -ne 0) { throw "Consumer failed: $stderr (exit $($process.ExitCode))" }

    if ($buildConfiguration -eq 'Debug_Editor') {
        $assertError = Join-Path $directory 'assert-stderr.log'
        $assertProcess = Start-Process -FilePath $binary -ArgumentList '--assert' -WorkingDirectory $dllDirectory -WindowStyle Hidden `
            -RedirectStandardOutput (Join-Path $directory 'assert-stdout.log') -RedirectStandardError $assertError -Wait -PassThru
        $row.debugAssertExitCode = $assertProcess.ExitCode
        $assertText = [System.IO.File]::ReadAllText($assertError)
        if ($assertProcess.ExitCode -eq 0 -or $assertProcess.ExitCode -eq 99 -or $assertText -notmatch 'Transform Get requires a live Entity') {
            throw "Expected DLL Debug assertion was not observed: $assertError"
        }
    }
    $results += $row
    Write-Output "$buildConfiguration consumer PASS"
}

$results | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $resultRoot 'results.json') -Encoding utf8
Write-Output "Logs: $resultRoot"
