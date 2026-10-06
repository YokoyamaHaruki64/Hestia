param(
    [switch]$BuildOnly,
    [string]$Filter = '*'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$testRoot = $PSScriptRoot
$project = Join-Path $testRoot 'Hestia_Tests\Hestia_Tests.vcxproj'
$binary = Join-Path $testRoot 'Build\bin\x64\Test\Hestia_Tests.exe'
$runId = '{0}-{1}' -f (Get-Date -Format 'yyyyMMdd-HHmmss'), [guid]::NewGuid().ToString('N').Substring(0, 8)
$resultDirectory = Join-Path $testRoot "Results\$runId"
[System.IO.Directory]::CreateDirectory($resultDirectory) | Out-Null

$buildText = Join-Path $resultDirectory 'msbuild.log'
$buildBinary = Join-Path $resultDirectory 'msbuild.binlog'
$buildConsole = Join-Path $resultDirectory 'msbuild-console.log'
$testStdout = Join-Path $resultDirectory 'gtest_stdout.log'
$testStderr = Join-Path $resultDirectory 'gtest_stderr.log'
$testConsole = Join-Path $resultDirectory 'gtest-console.log'
$testXml = Join-Path $resultDirectory 'gtest-results.xml'
$summaryPath = Join-Path $resultDirectory 'run-summary.json'
$summaryMarkdownPath = Join-Path $resultDirectory 'Summary.md'
$failuresMarkdownPath = Join-Path $resultDirectory 'Failures.md'

$summary = [ordered]@{
    startTime = (Get-Date).ToUniversalTime().ToString('o')
    endTime = $null
    configuration = 'Test'
    platform = 'x64'
    buildOnly = [bool]$BuildOnly
    filter = $Filter
    project = $project
    binary = $binary
    buildCommand = $null
    testCommand = $null
    buildExitCode = $null
    testExitCode = $null
    testsDiscovered = 0
    testsRun = 0
    testsPassed = 0
    testsFailed = 0
    testsSkipped = 0
    result = 'NotStarted'
    error = $null
    logs = [ordered]@{
        buildText = $buildText
        buildBinary = $buildBinary
        buildConsole = $buildConsole
        testStdout = $testStdout
        testStderr = $testStderr
        testConsole = $testConsole
        testXml = $testXml
        summaryMarkdown = $summaryMarkdownPath
        failuresMarkdown = $failuresMarkdownPath
    }
}

$testCases = @()
$status = 2

try {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path -LiteralPath $vswhere)) {
        throw "vswhere.exe が見つかりません: $vswhere"
    }

    $msbuild = & $vswhere -latest -products '*' -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
    if (-not $msbuild) {
        throw 'MSBuild が見つかりません。Visual Studio C++ ビルドツールを確認してください。'
    }

    $buildArguments = @(
        $project,
        '/t:Build',
        '/p:Configuration=Test;Platform=x64',
        '/m',
        '/nologo',
        '/clp:verbosity=minimal;summary;ForceNoAlign',
        "/flp:logfile=$buildText;verbosity=normal;Encoding=UTF-8",
        "/bl:$buildBinary;ProjectImports=None"
    )
    $summary.buildCommand = @($msbuild) + $buildArguments

    $previousPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        & $msbuild @buildArguments 2>&1 | Out-File -LiteralPath $buildConsole -Encoding utf8
        $summary.buildExitCode = $LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $previousPreference
    }

    if ($summary.buildExitCode -ne 0) {
        $summary.result = 'BuildFailed'
        $status = 1
    }
    elseif ($BuildOnly) {
        $summary.result = 'BuildSucceeded'
        $status = 0
    }
    else {
        if (-not (Test-Path -LiteralPath $binary)) {
            throw "テスト実行ファイルが見つかりません: $binary"
        }

        $testArguments = @("--gtest_filter=$Filter", "--gtest_output=xml:$testXml")
        $summary.testCommand = @($binary) + $testArguments
        $process = Start-Process -FilePath $binary -ArgumentList $testArguments -WorkingDirectory $testRoot `
            -RedirectStandardOutput $testStdout -RedirectStandardError $testStderr -NoNewWindow -Wait -PassThru
        $summary.testExitCode = $process.ExitCode

        $stdout = Get-Content -LiteralPath $testStdout -Raw -Encoding UTF8 -ErrorAction SilentlyContinue
        $stderr = Get-Content -LiteralPath $testStderr -Raw -Encoding UTF8 -ErrorAction SilentlyContinue
        [System.IO.File]::WriteAllText($testConsole, ($stdout + $stderr), [System.Text.UTF8Encoding]::new($false))

        if (Test-Path -LiteralPath $testXml) {
            $xml = [System.Xml.XmlDocument]::new()
            $xml.Load($testXml)
            $summary.testsDiscovered = [int]$xml.testsuites.GetAttribute('tests')
            $summary.testsRun = $summary.testsDiscovered - [int]$xml.testsuites.GetAttribute('disabled')
            $summary.testsFailed = [int]$xml.testsuites.GetAttribute('failures')
            $summary.testsSkipped = [int]$xml.testsuites.GetAttribute('skipped')
            $summary.testsPassed = $summary.testsRun - $summary.testsFailed - $summary.testsSkipped

            foreach ($caseNode in @($xml.SelectNodes('/testsuites/testsuite/testcase'))) {
                $targetNode = $caseNode.SelectSingleNode("properties/property[@name='target']")
                $failureNodes = @($caseNode.SelectNodes('failure|error'))
                $caseResult = '成功'
                $failureDetails = @()

                if ($failureNodes.Count -gt 0) {
                    $caseResult = '失敗'

                    foreach ($failureNode in $failureNodes) {
                        $message = $failureNode.GetAttribute('message')
                        if (-not $message) { $message = $failureNode.InnerText }
                        if ($message) { $failureDetails += $message.Trim() }
                    }
                }
                elseif ($caseNode.SelectSingleNode('skipped')) {
                    $caseResult = 'スキップ'
                }
                elseif ($caseNode.GetAttribute('status') -ne 'run') {
                    $caseResult = '未実行'
                }

                $suiteName = $caseNode.GetAttribute('classname')
                if (-not $suiteName) { $suiteName = $caseNode.ParentNode.GetAttribute('name') }

                $testCases += [pscustomobject]@{
                    name = '{0}.{1}' -f $suiteName, $caseNode.GetAttribute('name')
                    target = if ($targetNode) { $targetNode.GetAttribute('value') } else { '未指定' }
                    result = $caseResult
                    failureDetails = $failureDetails -join "`n`n"
                }
            }
        }

        if ($summary.testExitCode -ne 0) {
            $summary.result = 'TestFailed'
            $status = 1
        }
        elseif (-not (Test-Path -LiteralPath $testXml)) {
            $summary.result = 'MissingXml'
            $status = 1
        }
        elseif (($summary.testsRun - $summary.testsSkipped) -eq 0) {
            $summary.result = 'NoTests'
            $status = 1
        }
        else {
            $summary.result = 'Passed'
            $status = 0
        }
    }
}
catch {
    $summary.result = 'RunnerError'
    $summary.error = $_.Exception.Message
    $status = 2
}
finally {
    $summary.endTime = (Get-Date).ToUniversalTime().ToString('o')

    $resultLabel = switch ($summary.result) {
        'Passed' { '成功' }
        'BuildSucceeded' { 'ビルド成功（テスト未実行）' }
        'BuildFailed' { 'ビルド失敗' }
        'TestFailed' { 'テスト失敗' }
        'MissingXml' { 'テスト結果 XML がありません' }
        'NoTests' { 'テスト0件' }
        'RunnerError' { '実行処理エラー' }
        default { $summary.result }
    }

    $testResultLabel = switch ($summary.result) {
        'Passed' { '成功' }
        'TestFailed' { '失敗' }
        'NoTests' { 'テスト0件' }
        'MissingXml' { '結果 XML なし' }
        'BuildSucceeded' { '未実行（BuildOnly）' }
        'BuildFailed' { '未実行（ビルド失敗）' }
        'RunnerError' {
            if ($null -eq $summary.testExitCode) { '未実行' } else { '実行処理エラー' }
        }
        default { '未実行' }
    }

    $formatCommand = {
        param([string[]]$Arguments)
        if (-not $Arguments -or $Arguments.Count -eq 0) {
            return '未実行'
        }

        return '& ' + (($Arguments | ForEach-Object { "'" + $_.Replace("'", "''") + "'" }) -join ' ')
    }

    $buildCommandText = & $formatCommand $summary.buildCommand
    $testCommandText = & $formatCommand $summary.testCommand
    $started = [DateTimeOffset]::Parse($summary.startTime).ToLocalTime().ToString('yyyy-MM-dd HH:mm:ss zzz')
    $ended = [DateTimeOffset]::Parse($summary.endTime).ToLocalTime().ToString('yyyy-MM-dd HH:mm:ss zzz')
    $resultCode = '`' + $summary.result + '`'
    $configurationLabel = "$($summary.configuration)|$($summary.platform)"
    $filterCode = '`' + $summary.filter + '`'
    $projectCode = '`' + $summary.project + '`'
    $binaryCode = '`' + $summary.binary + '`'
    $buildResultLabel = if ($null -eq $summary.buildExitCode) { '未実行' } elseif ($summary.buildExitCode -eq 0) { '成功' } else { '失敗' }
    $escapeCell = {
        param([string]$Value)
        if ([string]::IsNullOrWhiteSpace($Value)) { return '—' }

        return [System.Net.WebUtility]::HtmlEncode($Value).Replace('|', '&#124;').Replace("`r`n", '<br>').Replace("`n", '<br>').Replace("`r", '<br>')
    }

    if ($testCases.Count -gt 0) {
        $tableLines = @(
            '| テスト名 | 対象関数 | 結果 |',
            '|---|---|---|'
        )
        foreach ($testCase in $testCases) {
            $cells = @($testCase.name, $testCase.target, $testCase.result) |
                ForEach-Object { & $escapeCell $_ }
            $tableLines += '| ' + ($cells -join ' | ') + ' |'
        }
        $testTable = $tableLines -join "`n"
    }
    elseif ($BuildOnly) {
        $testTable = 'BuildOnly のためテストは実行していません。'
    }
    elseif (Test-Path -LiteralPath $testXml) {
        $testTable = '該当するテストはありません。'
    }
    else {
        $testTable = 'テスト結果 XML がないため一覧はありません。'
    }

    $failureCases = @($testCases | Where-Object { $_.result -eq '失敗' })
    if ($failureCases.Count -gt 0) {
        $failureLines = @(
            '| テスト名 | 対象関数 | failure message |',
            '|---|---|---|'
        )
        foreach ($failureCase in $failureCases) {
            $cells = @($failureCase.name, $failureCase.target, $failureCase.failureDetails) |
                ForEach-Object { & $escapeCell $_ }
            $failureLines += '| ' + ($cells -join ' | ') + ' |'
        }
        $failuresContent = @"
# 失敗テスト

失敗: $($failureCases.Count) 件

$($failureLines -join "`n")
"@
    }
    elseif ($summary.result -eq 'Passed') {
        $failuresContent = "# 失敗テスト`n`n失敗はありません（実行 $($summary.testsRun) 件）。`n"
    }
    elseif ($summary.result -eq 'TestFailed') {
        $failuresContent = "# 失敗テスト`n`nテストプロセスは終了コード $($summary.testExitCode) で終了しましたが、XML から失敗したテストを特定できませんでした。`n"
    }
    elseif ($summary.result -eq 'RunnerError') {
        $failuresContent = "# 失敗テスト`n`nテスト結果を生成できませんでした。`n`n$($summary.error)`n"
    }
    else {
        $failuresContent = "# 失敗テスト`n`nテストは実行されていません（$resultLabel）。`n"
    }

    $markdown = @"
# テスト実行サマリー

## 結果

**$resultLabel** ($resultCode)

| 項目 | 値 |
|---|---|
| 開始 | $started |
| 終了 | $ended |
| 構成 | $configurationLabel |
| フィルター | $filterCode |
| テストプロジェクト | $projectCode |
| 実行ファイル | $binaryCode |

## ビルド

- 結果: $buildResultLabel
- 終了コード: $($summary.buildExitCode)
- コマンド:

~~~powershell
$buildCommandText
~~~

## テスト

- 結果: $testResultLabel
- 終了コード: $($summary.testExitCode)
- 検出: $($summary.testsDiscovered) 件
- 実行: $($summary.testsRun) 件
- 成功: $($summary.testsPassed) 件
- 失敗: $($summary.testsFailed) 件
- スキップ: $($summary.testsSkipped) 件
- コマンド:

~~~powershell
$testCommandText
~~~

## テスト結果一覧

[失敗テストの詳細](Failures.md)

$testTable

$(if ($summary.error) { "## エラー`n`n$($summary.error)`n" })
"@

    [System.IO.File]::WriteAllText($summaryMarkdownPath, $markdown, [System.Text.UTF8Encoding]::new($true))
    [System.IO.File]::WriteAllText($failuresMarkdownPath, $failuresContent, [System.Text.UTF8Encoding]::new($true))
    $summary | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $summaryPath -Encoding UTF8
    Write-Host "結果: $($summary.result) / ビルド終了コード: $($summary.buildExitCode) / テスト終了コード: $($summary.testExitCode) / 実行件数: $($summary.testsRun)"
    Write-Host "ログ: $resultDirectory"
    if ($summary.error) { Write-Error $summary.error -ErrorAction Continue }
}

exit $status
