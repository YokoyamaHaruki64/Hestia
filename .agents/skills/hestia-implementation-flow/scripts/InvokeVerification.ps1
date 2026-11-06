param(
    [Parameter(Mandatory = $true)][string]$TaskPath,
    [string]$RepositoryRoot = '',
    [ValidateSet('research', 'header', 'test', 'implementation', 'review', 'complete')][string]$CurrentPhase
)

. (Join-Path $PSScriptRoot 'Common.ps1')

function ConvertTo-HifCommandLineArgument {
    param([AllowEmptyString()][string]$Value)

    $builder = [System.Text.StringBuilder]::new()
    [void]$builder.Append('"')
    $slashes = 0
    foreach ($character in $Value.ToCharArray()) {
        if ($character -eq '\') { $slashes++; continue }
        if ($character -eq '"') {
            [void]$builder.Append(('\' * (2 * $slashes + 1)))
            [void]$builder.Append('"')
            $slashes = 0
            continue
        }
        if ($slashes -gt 0) { [void]$builder.Append(('\' * $slashes)); $slashes = 0 }
        [void]$builder.Append($character)
    }
    if ($slashes -gt 0) { [void]$builder.Append(('\' * (2 * $slashes))) }
    [void]$builder.Append('"')
    return $builder.ToString()
}

function Test-HifOnlyKeys {
    param([object]$Object, [string[]]$Allowed, [string]$Label)
    if ($null -eq $Object -or $Object -isnot [System.Management.Automation.PSCustomObject]) { throw "$Label は object である必要があります。" }
    $unknown = @($Object.PSObject.Properties.Name | Where-Object { $_ -notin $Allowed })
    if ($unknown.Count -gt 0) { throw "$Label に未対応の項目があります: $($unknown -join ', ')" }
}

function Get-HifPathArgument {
    param([object]$Value, [string]$Name)
    if ($null -eq $Value) { return '' }
    if ($Value -is [string]) { $items = @($Value -split ';' | Where-Object { $_.Trim() } | ForEach-Object { $_.Trim() }) }
    elseif ($Value -is [System.Array]) {
        foreach ($item in $Value) { if ($item -isnot [string]) { throw "$Name の各パスは文字列である必要があります。" } }
        $items = @($Value)
    }
    else { throw "$Name はパス文字列または文字列配列である必要があります。" }
    if ($items.Count -eq 0) { throw "$Name は空にできません。" }
    foreach ($item in $items) { [void](Get-HifRepositoryPath $item -AllowMissing) }
    return ($items -join ';')
}

function Invoke-HifChildPowerShell {
    param([string[]]$Tokens, [string]$WorkingDirectory, [string]$StdoutPath, [string]$StderrPath)

    $executable = if ($PSVersionTable.PSEdition -eq 'Core') { Join-Path $PSHOME 'pwsh.exe' } else { Join-Path $PSHOME 'powershell.exe' }
    if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) { throw "PowerShell 実行ファイルがありません: $executable" }
    $startInfo = [System.Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $executable
    $startInfo.Arguments = (($Tokens | ForEach-Object { ConvertTo-HifCommandLineArgument ([string]$_) }) -join ' ')
    $startInfo.WorkingDirectory = $WorkingDirectory
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    $process = [System.Diagnostics.Process]::new()
    $process.StartInfo = $startInfo
    if (-not $process.Start()) { throw '検証プロセスを開始できませんでした。' }
    $stdoutTask = $process.StandardOutput.ReadToEndAsync()
    $stderrTask = $process.StandardError.ReadToEndAsync()
    $process.WaitForExit()
    $stdout = $stdoutTask.Result
    $stderr = $stderrTask.Result
    Write-HifUtf8File $StdoutPath $stdout -NoBom
    Write-HifUtf8File $StderrPath $stderr -NoBom
    return [pscustomobject]@{ exitCode = $process.ExitCode; stdout = $stdout; stderr = $stderr }
}

try {
    Set-HifRepositoryRoot $RepositoryRoot
    $taskDirectory = Get-HifTaskPath $TaskPath
    $metadata = (Get-HifFirstJsonMetadata (Join-Path $taskDirectory 'Task.md')).Metadata
    $decision = if ((Test-HifProperty $metadata 'verification') -and (Test-HifProperty $metadata.verification 'decision')) { [string]$metadata.verification.decision } else { 'pending' }
    $reason = if ((Test-HifProperty $metadata 'verification') -and (Test-HifProperty $metadata.verification 'reason')) { [string]$metadata.verification.reason } else { '' }
    if ($decision -eq 'pending') { Write-HifResult 'InvokeVerification' 'pending' 0 ([pscustomobject]@{ taskId = $metadata.taskId; executed = $false; decision = 'pending' }) @('検証判断が pending のため実行していません。') }
    if ($decision -eq 'skip') {
        if (-not $reason.Trim()) { throw 'verification=skip には理由が必要です。' }
        if ((Test-HifProperty $metadata 'verification') -and (Test-HifProperty $metadata.verification 'steps') -and @($metadata.verification.steps).Count -gt 0) { throw 'verification=skip では steps を空にしてください。' }
        Write-HifResult 'InvokeVerification' 'skipped' 0 ([pscustomobject]@{ taskId = $metadata.taskId; executed = $false; decision = 'skip'; reason = $reason })
    }
    if ($decision -ne 'run') { throw "verification.decision が不正です: $decision" }
    if (-not $reason.Trim()) { throw 'verification=run には理由が必要です。' }
    $phase = if ($CurrentPhase) { $CurrentPhase } else { [string]$metadata.phase }
    if ($CurrentPhase -and $CurrentPhase -ne [string]$metadata.phase) { throw "CurrentPhase ($CurrentPhase) と Task.phase ($($metadata.phase)) が異なります。Task を更新してください。" }
    if ($phase -notin @('test', 'implementation', 'review', 'complete')) { throw "この工程では検証を実行できません: $phase" }
    if ([string]$metadata.status -notin @('ready', 'in_progress', 'awaiting_review', 'complete')) { throw "Task.status が実行可能ではありません: $($metadata.status)" }
    if (-not (Test-HifProperty $metadata 'verification') -or -not (Test-HifProperty $metadata.verification 'steps') -or $metadata.verification.steps -isnot [System.Array]) { throw 'verification.steps は JSON 配列である必要があります。' }
    $steps = @($metadata.verification.steps)
    if ($steps.Count -eq 0) { throw 'verification=run には1件以上の step が必要です。' }

    $plannedSteps = [System.Collections.Generic.List[object]]::new()
    $stepIndex = 0
    foreach ($step in $steps) {
        $stepIndex++
        if ($step -isnot [System.Management.Automation.PSCustomObject] -or -not (Test-HifProperty $step 'kind') -or -not (Test-HifProperty $step 'arguments')) { throw "verification.steps[$($stepIndex - 1)] は kind と arguments を持つ object が必要です。" }
        $kind = [string]$step.kind
        $argumentsObject = $step.arguments
        $scriptPath = $null
        $tokens = @('-NoProfile', '-NonInteractive', '-ExecutionPolicy', 'Bypass', '-File')
        $commandLabel = $null
        switch ($kind) {
            'RunTests' {
                Test-HifOnlyKeys $argumentsObject @('BuildOnly', 'Filter') 'RunTests.arguments'
                $scriptPath = Join-Path $script:HifRepositoryRoot 'Test\RunTests.ps1'
                $commandLabel = 'Test/RunTests.ps1'
                $tokens += $scriptPath
                if (Test-HifProperty $argumentsObject 'BuildOnly') {
                    if ($argumentsObject.BuildOnly -isnot [bool]) { throw 'RunTests.BuildOnly は boolean が必要です。' }
                    if ($argumentsObject.BuildOnly) { $tokens += '-BuildOnly' }
                }
                if (Test-HifProperty $argumentsObject 'Filter') {
                    if ($argumentsObject.Filter -isnot [string] -or $argumentsObject.Filter.Contains("`n") -or $argumentsObject.Filter.Contains("`r")) { throw 'RunTests.Filter は改行を含まない文字列が必要です。' }
                    $tokens += @('-Filter', [string]$argumentsObject.Filter)
                }
            }
            'VerifyDocs' {
                Test-HifOnlyKeys $argumentsObject @('paths') 'VerifyDocs.arguments'
                $scriptPath = Join-Path $script:HifRepositoryRoot 'Tools\Verify\Verify.ps1'
                $commandLabel = 'Tools/Verify/Verify.ps1 -Mode Docs'
                $tokens += @($scriptPath, '-Mode', 'Docs')
                if (Test-HifProperty $argumentsObject 'paths') { $tokens += @('-Paths', (Get-HifPathArgument $argumentsObject.paths 'VerifyDocs.paths')) }
            }
            'VerifyProject' {
                Test-HifOnlyKeys $argumentsObject @('project', 'configuration', 'paths', 'rebuild') 'VerifyProject.arguments'
                if (-not (Test-HifProperty $argumentsObject 'project') -or $argumentsObject.project -isnot [string] -or [string]::IsNullOrWhiteSpace($argumentsObject.project)) { throw 'VerifyProject.project は空でない文字列が必要です。' }
                $project = [string]$argumentsObject.project
                if ($project.EndsWith('.vcxproj', [StringComparison]::OrdinalIgnoreCase)) { [void](Get-HifRepositoryPath $project) }
                elseif ($project -notmatch '^[A-Za-z0-9_.-]+$') { throw 'VerifyProject.project は名前または .vcxproj のリポジトリ相対パスが必要です。' }
                $scriptPath = Join-Path $script:HifRepositoryRoot 'Tools\Verify\Verify.ps1'
                $commandLabel = 'Tools/Verify/Verify.ps1 -Mode Project'
                $tokens += @($scriptPath, '-Mode', 'Project', '-Project', $project)
                if (Test-HifProperty $argumentsObject 'configuration') {
                    $configuration = [string]$argumentsObject.configuration
                    if ($configuration -notin @('Debug_Editor', 'Debug_Game', 'Release_Editor', 'Release_Game')) { throw "VerifyProject.configuration が不正です: $configuration" }
                    $tokens += @('-Configuration', $configuration)
                }
                if (Test-HifProperty $argumentsObject 'paths') { $tokens += @('-Paths', (Get-HifPathArgument $argumentsObject.paths 'VerifyProject.paths')) }
                if (Test-HifProperty $argumentsObject 'rebuild') {
                    if ($argumentsObject.rebuild -isnot [bool]) { throw 'VerifyProject.rebuild は boolean が必要です。' }
                    if ($argumentsObject.rebuild) { $tokens += '-Rebuild' }
                }
            }
            'BuildSolution' {
                Test-HifOnlyKeys $argumentsObject @('configuration', 'rebuild') 'BuildSolution.arguments'
                $scriptPath = Join-Path $script:HifRepositoryRoot 'Tools\Verify\Verify.ps1'
                $commandLabel = 'Tools/Verify/Verify.ps1 -Mode Solution'
                $tokens += @($scriptPath, '-Mode', 'Solution')
                if (Test-HifProperty $argumentsObject 'configuration') {
                    $configuration = [string]$argumentsObject.configuration
                    if ($configuration -notin @('All', 'Debug_Editor', 'Debug_Game', 'Release_Editor', 'Release_Game')) { throw "BuildSolution.configuration が不正です: $configuration" }
                    $tokens += @('-Configuration', $configuration)
                }
                if (Test-HifProperty $argumentsObject 'rebuild') {
                    if ($argumentsObject.rebuild -isnot [bool]) { throw 'BuildSolution.rebuild は boolean が必要です。' }
                    if ($argumentsObject.rebuild) { $tokens += '-Rebuild' }
                }
            }
            default { throw "許可されていない検証 step です: $kind" }
        }
        if (-not (Test-Path -LiteralPath $scriptPath -PathType Leaf)) { throw "検証スクリプトがありません: $commandLabel" }
        $plannedSteps.Add([pscustomobject]@{ index = $stepIndex; kind = $kind; command = $commandLabel; tokens = [string[]]$tokens })
    }

    $runId = (Get-Date).ToUniversalTime().ToString('yyyyMMddTHHmmssfffZ') + '-' + [Guid]::NewGuid().ToString('N').Substring(0, 8)
    $runDirectory = Join-Path $script:HifRepositoryRoot "Build\Verification\Skill\$runId"
    [void][System.IO.Directory]::CreateDirectory($runDirectory)
    $stepResults = [System.Collections.Generic.List[object]]::new()
    foreach ($plan in $plannedSteps) {
        $stdoutPath = Join-Path $runDirectory ("step-{0:D2}-stdout.log" -f $plan.index)
        $stderrPath = Join-Path $runDirectory ("step-{0:D2}-stderr.log" -f $plan.index)
        try {
            $result = Invoke-HifChildPowerShell $plan.tokens $script:HifRepositoryRoot $stdoutPath $stderrPath
            $nestedDirectories = @([regex]::Matches($result.stdout, '(?im)^(?:Logs|ログ):\s*(?<path>.+)$') | ForEach-Object { $_.Groups['path'].Value.Trim() } | Select-Object -First 5)
            $stepResults.Add([pscustomobject]@{ index = $plan.index; kind = $plan.kind; command = $plan.command; exitCode = $result.exitCode; stdout = Get-HifRelativePath $stdoutPath; stderr = Get-HifRelativePath $stderrPath; nestedRunDirectories = $nestedDirectories })
        }
        catch {
            $stepResults.Add([pscustomobject]@{ index = $plan.index; kind = $plan.kind; command = $plan.command; exitCode = 2; stdout = if (Test-Path -LiteralPath $stdoutPath) { Get-HifRelativePath $stdoutPath } else { $null }; stderr = if (Test-Path -LiteralPath $stderrPath) { Get-HifRelativePath $stderrPath } else { $null }; error = $_.Exception.Message })
        }
    }
    $finalCode = if (@($stepResults | Where-Object { $_.exitCode -eq 2 }).Count -gt 0) { 2 } elseif (@($stepResults | Where-Object { $_.exitCode -ne 0 }).Count -gt 0) { 1 } else { 0 }
    $status = if ($finalCode -eq 0) { 'passed' } elseif ($finalCode -eq 1) { 'failed' } else { 'error' }
    $summary = [ordered]@{ schemaVersion = 1; taskId = $metadata.taskId; decision = $decision; phase = $phase; reason = $reason; exitCode = $finalCode; steps = @($stepResults.ToArray()) }
    $summaryFile = Join-Path $runDirectory 'verification-summary.json'
    Write-HifUtf8File $summaryFile ($summary | ConvertTo-Json -Depth 12) -NoBom
    $data = [pscustomobject]@{ taskId = $metadata.taskId; decision = $decision; phase = $phase; exitCode = $finalCode; steps = @($stepResults.ToArray()); runDirectory = Get-HifRelativePath $runDirectory; summary = Get-HifRelativePath $summaryFile }
    Write-HifResult 'InvokeVerification' $status $finalCode $data @() @() @($data.summary)
}
catch {
    Write-HifResult 'InvokeVerification' 'error' 2 $null @() @($_.Exception.Message)
}
