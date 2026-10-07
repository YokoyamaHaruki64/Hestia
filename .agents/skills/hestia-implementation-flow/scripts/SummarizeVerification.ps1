param(
    [Parameter(Mandatory = $true)][string]$RunDirectory,
    [string]$RepositoryRoot = ''
)

. (Join-Path $PSScriptRoot 'Common.ps1')
try {
    Set-HifRepositoryRoot $RepositoryRoot
    $directory = Get-HifRepositoryPath $RunDirectory
    if (-not (Test-Path -LiteralPath $directory -PathType Container)) { throw "実行フォルダーがありません: $RunDirectory" }
    $summaryMarkdown = Join-Path $directory 'Summary.md'
    $summaryJson = Join-Path $directory 'run-summary.json'
    $verifyJson = Join-Path $directory 'summary.json'
    $failurePath = Join-Path $directory 'Failures.md'
    $xmlFiles = @(Get-ChildItem -LiteralPath $directory -Filter '*results.xml' -File -Recurse -ErrorAction SilentlyContinue)
    $source = $null
    $exitCode = $null
    $buildExitCode = $null
    $testExitCode = $null
    $counts = [ordered]@{ discovered = $null; run = $null; passed = $null; failed = $null; skipped = $null }
    $failureDetails = @()
    $logPaths = @()

    if (Test-Path -LiteralPath $summaryMarkdown -PathType Leaf) {
        $source = Get-HifRelativePath $summaryMarkdown
        $text = [System.IO.File]::ReadAllText($summaryMarkdown).TrimStart([char]0xFEFF)
        $resultMatch = [regex]::Match($text, '(?m)^\*\*(?<label>[^*]+)\*\*\s*\(`(?<result>[^`]+)`\)')
        $buildSection = Get-HifMarkdownSection $text 'ビルド'
        $testSection = Get-HifMarkdownSection $text 'テスト'
        $buildExitMatch = if ($buildSection) { [regex]::Match($buildSection, '(?m)^- 終了コード:\s*(?<code>-?\d+)') } else { $null }
        $testExitMatch = if ($testSection) { [regex]::Match($testSection, '(?m)^- 終了コード:\s*(?<code>-?\d+)') } else { $null }
        $buildExitCode = if ($buildExitMatch -and $buildExitMatch.Success) { [int]$buildExitMatch.Groups['code'].Value } else { $null }
        $testExitCode = if ($testExitMatch -and $testExitMatch.Success) { [int]$testExitMatch.Groups['code'].Value } else { $null }
        foreach ($key in @('discovered', 'run', 'passed', 'failed', 'skipped')) {
            $label = switch ($key) { discovered { '検出' } run { '実行' } passed { '成功' } failed { '失敗' } skipped { 'スキップ' } }
            $match = [regex]::Match($text, ('(?m)^- ' + [regex]::Escape($label) + ':\s*(?<count>\d+) 件'))
            if ($match.Success) { $counts[$key] = [int]$match.Groups['count'].Value }
        }
        foreach ($match in [regex]::Matches($text, '(?im)^- (?:ログ|Logs):\s*(?<path>.+)$')) { $logPaths += $match.Groups['path'].Value.Trim() }
        $isBuildOnly = ($resultMatch.Success -and $resultMatch.Groups['result'].Value -eq 'BuildSucceeded')
        $knownSuccessful = ($resultMatch.Success -and $resultMatch.Groups['result'].Value -in @('Passed', 'BuildSucceeded'))
        if ($null -ne $buildExitCode -and $buildExitCode -ne 0) { $exitCode = $buildExitCode }
        elseif ($null -ne $testExitCode) { $exitCode = $testExitCode }
        elseif ($isBuildOnly -and $buildExitCode -eq 0) { $exitCode = 0 }
        elseif ($knownSuccessful) { $exitCode = 0 }
        if ($null -ne $counts.failed -and $counts.failed -eq 0 -and $knownSuccessful -and $buildExitCode -eq 0 -and (($null -ne $testExitCode -and $testExitCode -eq 0 -and $counts.run -gt 0) -or $isBuildOnly)) {
            $data = [pscustomobject]@{
                runDirectory = Get-HifRelativePath $directory
                source = $source
                result = if ($isBuildOnly) { 'build_only' } else { 'passed' }
                exitCode = 0
                counts = [pscustomobject]$counts
                failures = @()
                logs = @()
                failureFileRead = $false
            }
            Write-HifResult 'SummarizeVerification' 'passed' 0 $data
        }
        if (($null -ne $counts.failed -and $counts.failed -gt 0) -or ($null -ne $exitCode -and $exitCode -ne 0)) {
            if (Test-Path -LiteralPath $failurePath -PathType Leaf) {
            $failureText = [System.IO.File]::ReadAllText($failurePath).TrimStart([char]0xFEFF)
            $failureDetails = @($failureText -split "`r?`n" | Where-Object { $_.Trim() -and $_ -notmatch '^#{1,6}\s' -and $_ -notmatch '^\|\s*(項目|---)' } | Select-Object -First 40)
            }
        }
    }

    $invokeSummary = Join-Path $directory 'verification-summary.json'
    if (($null -eq $exitCode -or $null -eq $counts.failed) -and (Test-Path -LiteralPath $invokeSummary -PathType Leaf)) {
        $record = Get-Content -LiteralPath $invokeSummary -Raw -Encoding UTF8 | ConvertFrom-Json -ErrorAction Stop
        if (Test-HifProperty $record 'exitCode') { $exitCode = [int]$record.exitCode }
        if (Test-HifProperty $record 'steps') {
            foreach ($step in @($record.steps)) {
                if ((Test-HifProperty $step 'exitCode') -and [int]$step.exitCode -ne 0) { $failureDetails += ('{0}: exit {1}' -f $step.kind, $step.exitCode) }
                foreach ($name in @('stdout', 'stderr')) { if ((Test-HifProperty $step $name) -and $step.$name) { $logPaths += [string]$step.$name } }
                if (Test-HifProperty $step 'nestedRunDirectories') { $logPaths += @($step.nestedRunDirectories) }
            }
        }
        $source = Get-HifRelativePath $invokeSummary
    }

    if (($null -eq $exitCode -or $null -eq $counts.failed) -and (Test-Path -LiteralPath $summaryJson -PathType Leaf)) {
        $record = Get-Content -LiteralPath $summaryJson -Raw -Encoding UTF8 | ConvertFrom-Json -ErrorAction Stop
        if ($null -eq $exitCode -and (Test-HifProperty $record 'testExitCode')) { $exitCode = $record.testExitCode }
        if ($null -eq $exitCode -and (Test-HifProperty $record 'ExitCode')) { $exitCode = $record.ExitCode }
        foreach ($field in @('testsDiscovered', 'testsRun', 'testsPassed', 'testsFailed', 'testsSkipped')) {
            $key = switch ($field) { testsDiscovered { 'discovered' } testsRun { 'run' } testsPassed { 'passed' } testsFailed { 'failed' } testsSkipped { 'skipped' } }
            if ($null -eq $counts[$key] -and (Test-HifProperty $record $field)) { $counts[$key] = [int]$record.$field }
        }
        if ((Test-HifProperty $record 'logs') -and $record.logs) { $logPaths += @($record.logs.PSObject.Properties | ForEach-Object { [string]$_.Value }) }
        if ((Test-HifProperty $record 'Results') -and $null -ne $record.Results) {
            $bad = @($record.Results | Where-Object { (Test-HifProperty $_ 'ExitCode') -and [int]$_.ExitCode -ne 0 })
            if ($null -eq $exitCode) { $exitCode = if ($bad.Count) { 1 } else { 0 } }
            if ($bad.Count) { $failureDetails += @($bad | ForEach-Object { '{0}: exit {1}' -f $_.Name, $_.ExitCode }) }
        }
    }
    if (($null -eq $exitCode -or $null -eq $counts.failed) -and (Test-Path -LiteralPath $verifyJson -PathType Leaf)) {
        $record = Get-Content -LiteralPath $verifyJson -Raw -Encoding UTF8 | ConvertFrom-Json -ErrorAction Stop
        if ($null -eq $exitCode -and (Test-HifProperty $record 'ExitCode')) { $exitCode = [int]$record.ExitCode }
        if ((Test-HifProperty $record 'Results') -and $null -ne $record.Results) {
            $bad = @($record.Results | Where-Object { (Test-HifProperty $_ 'ExitCode') -and [int]$_.ExitCode -ne 0 })
            if ($null -eq $exitCode) { $exitCode = if ($bad.Count) { 1 } else { 0 } }
            if ($bad.Count) { $failureDetails += @($bad | ForEach-Object { '{0}: exit {1}' -f $_.Name, $_.ExitCode }) }
        }
    }

    if (($null -eq $exitCode -or $null -eq $counts.failed) -and $xmlFiles.Count -gt 0) {
        $xml = [System.Xml.XmlDocument]::new()
        $xml.Load($xmlFiles[0].FullName)
        $suite = $xml.SelectSingleNode('/testsuites')
        if ($suite) {
            $counts.discovered = [int]$suite.GetAttribute('tests')
            $counts.failed = [int]$suite.GetAttribute('failures') + [int]$suite.GetAttribute('errors')
            $counts.skipped = [int]$suite.GetAttribute('skipped')
            $counts.run = $counts.discovered - [int]$suite.GetAttribute('disabled')
            $counts.passed = $counts.run - $counts.failed - $counts.skipped
            if ($null -eq $exitCode) { $exitCode = if ($counts.failed -gt 0) { 1 } else { 0 } }
            if ($counts.failed -gt 0) {
                foreach ($case in @($xml.SelectNodes('/testsuites/testsuite/testcase[failure or error]'))) {
                    $failureDetails += ('{0}.{1}' -f $case.GetAttribute('classname'), $case.GetAttribute('name'))
                }
            }
        }
    }

    if ($counts.failed -gt 0 -and $failureDetails.Count -eq 0 -and (Test-Path -LiteralPath $failurePath -PathType Leaf)) {
        $failureText = [System.IO.File]::ReadAllText($failurePath).TrimStart([char]0xFEFF)
        $failureDetails = @($failureText -split "`r?`n" | Where-Object { $_.Trim() -and $_ -notmatch '^#{1,6}\s' } | Select-Object -First 40)
    }
    if ($logPaths.Count -eq 0) { $logPaths = @(Get-ChildItem -LiteralPath $directory -File -Recurse -ErrorAction SilentlyContinue | Where-Object { $_.Extension -in @('.log', '.binlog', '.xml') } | Select-Object -First 30 | ForEach-Object { Get-HifRelativePath $_.FullName }) }
    $complete = ($null -ne $exitCode)
    $failed = (($null -ne $counts.failed -and $counts.failed -gt 0) -or ($null -ne $exitCode -and $exitCode -ne 0))
    $status = if (-not $complete) { 'incomplete' } elseif ($failed) { 'failed' } else { 'passed' }
    $data = [pscustomobject]@{
        runDirectory = Get-HifRelativePath $directory
        source = $source
        result = $status
        exitCode = $exitCode
        buildExitCode = $buildExitCode
        testExitCode = $testExitCode
        counts = [pscustomobject]$counts
        failures = @($failureDetails | Select-Object -First 40)
        logs = @($logPaths | Select-Object -First 30)
        failureFileRead = ($null -ne $counts.failed -and $counts.failed -gt 0 -and (Test-Path -LiteralPath $failurePath -PathType Leaf))
    }
    $code = if (-not $complete -or $failed) { 1 } else { 0 }
    $warnings = if (-not $complete) { @('Summary、構造化 JSON、XML から終了結果を確定できませんでした。') } else { @() }
    Write-HifResult 'SummarizeVerification' $status $code $data $warnings
}
catch {
    Write-HifResult 'SummarizeVerification' 'error' 2 $null @() @($_.Exception.Message)
}
