param(
    [Parameter(Mandatory = $true)][string]$TaskPath,
    [string]$RepositoryRoot = '',
    [string]$BaselinePath
)

. (Join-Path $PSScriptRoot 'Common.ps1')
try {
    Set-HifRepositoryRoot $RepositoryRoot
    $taskDirectory = Get-HifTaskPath $TaskPath
    $task = (Get-HifFirstJsonMetadata (Join-Path $taskDirectory 'Task.md')).Metadata
    if (-not $BaselinePath) { $baselineFile = Join-Path $taskDirectory 'Baseline.json' }
    else { $baselineFile = Get-HifRepositoryPath $BaselinePath }
    if (-not (Test-Path -LiteralPath $baselineFile -PathType Leaf)) { throw "Baseline.json がありません: $baselineFile" }
    $baseline = Get-Content -LiteralPath $baselineFile -Raw -Encoding UTF8 | ConvertFrom-Json -ErrorAction Stop
    foreach ($idName in @('sessionId', 'runId', 'taskId')) {
        if ([string]$baseline.$idName -ne [string]$task.$idName) { throw "Baseline と Task の $idName が一致しません。" }
    }

    $before = @{}
    foreach ($item in @($baseline.gitChanges)) { $before[[string]$item.path] = $item }
    $after = @{}
    foreach ($item in @(Get-HifGitSnapshot)) { $after[[string]$item.path] = $item }
    $changes = [System.Collections.Generic.List[object]]::new()
    foreach ($path in @($before.Keys + $after.Keys | Sort-Object -Unique)) {
        $old = $before[$path]
        $new = $after[$path]
        if ($null -eq $old) { $kind = 'added' }
        elseif ($null -eq $new) { $kind = 'resolved' }
        elseif ([string]$old.status -eq [string]$new.status -and [string]$old.hash -eq [string]$new.hash) { continue }
        else { $kind = 'changed' }
        $isTaskArtifact = Test-HifPathsOverlap ([string]$path) (Get-HifRelativePath $taskDirectory)
        $inScope = $false
        foreach ($scopePath in @($task.editPaths)) {
            if (Test-HifPathsOverlap ([string]$path) ([string]$scopePath)) { $inScope = $true; break }
        }
        $changes.Add([pscustomobject]@{
            path = [string]$path
            change = $kind
            baselineStatus = if ($old) { [string]$old.status } else { $null }
            currentStatus = if ($new) { [string]$new.status } else { $null }
            outsideScope = (-not $inScope -and -not $isTaskArtifact)
        })
    }
    $outside = @($changes | Where-Object { $_.outsideScope })
    $verificationRoot = Join-Path $script:HifRepositoryRoot 'Build\SkillVerification'
    [void][System.IO.Directory]::CreateDirectory($verificationRoot)
    $reportPath = Join-Path $verificationRoot ('{0}-changes.md' -f $task.taskId)
    $rows = @($changes | ForEach-Object { '| `{0}` | {1} | {2} |' -f $_.path.Replace('|', '\|'), $_.change, $(if ($_.outsideScope) { '範囲外候補' } else { '範囲内' }) })
    if ($rows.Count -eq 0) { $rows = @('| なし | 変更なし | - |') }
    $report = @(
        '# 作業差分の集計',
        '',
        "- Task: $($task.taskId)",
        "- Baseline: $(Get-HifRelativePath $baselineFile)",
        "- 比較日時: $((Get-Date).ToUniversalTime().ToString('o'))",
        "- 変更件数: $($changes.Count)",
        "- 範囲外候補: $($outside.Count)",
        '',
        '| パス | 差分 | 範囲 |',
        '|---|---|---|'
    ) + $rows
    Write-HifUtf8File $reportPath ($report -join "`n")
    $data = [pscustomobject]@{
        taskId = $task.taskId
        baseline = Get-HifRelativePath $baselineFile
        changeCount = $changes.Count
        added = @($changes | Where-Object { $_.change -eq 'added' }).Count
        changed = @($changes | Where-Object { $_.change -eq 'changed' }).Count
        resolved = @($changes | Where-Object { $_.change -eq 'resolved' }).Count
        outsideScopeCount = $outside.Count
        changes = @($changes | Select-Object -First 100)
        pathsTruncated = ($changes.Count -gt 100)
        report = Get-HifRelativePath $reportPath
    }
    $warnings = if ($outside.Count -gt 0) { @('担当範囲外候補の変更があります。既存変更を自動変更・削除していません。') } else { @() }
    Write-HifResult 'SummarizeChanges' 'summarized' 0 $data $warnings @() @($data.report)
}
catch {
    Write-HifResult 'SummarizeChanges' 'error' 2 $null @() @($_.Exception.Message)
}
