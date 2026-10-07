param(
    [Parameter(Mandatory = $true)][string]$TaskPath,
    [string]$RepositoryRoot = '',
    [string]$BaselinePath
)

. (Join-Path $PSScriptRoot 'Common.ps1')
try {
    Set-HifRepositoryRoot $RepositoryRoot
    $taskDirectory = Get-HifTaskPath $TaskPath
    $taskFile = Join-Path $taskDirectory 'Task.md'
    $document = Get-HifFirstJsonMetadata $taskFile
    $metadata = $document.Metadata
    if (-not (Test-HifSafeId ([string]$metadata.sessionId)) -or -not (Test-HifSafeId ([string]$metadata.runId)) -or -not (Test-HifSafeId ([string]$metadata.taskId))) { throw 'Task の sessionId / runId / taskId が不正です。' }
    if (-not $BaselinePath) { $baselineFile = Join-Path $taskDirectory 'Baseline.json' }
    else { $baselineFile = Get-HifRepositoryPath $BaselinePath }
    $baselineData = $null
    if (Test-Path -LiteralPath $baselineFile -PathType Leaf) { $baselineData = Get-Content -LiteralPath $baselineFile -Raw -Encoding UTF8 | ConvertFrom-Json -ErrorAction Stop }

    $changed = @(Get-HifGitSnapshot)
    $activeStatuses = @('ready', 'in_progress', 'awaiting_review', 'awaiting_upper', 'awaiting_user')
    $overlaps = @()
    $ownPaths = @($metadata.editPaths | ForEach-Object { ([string]$_).Replace('\', '/') })
    $allTasks = @(Get-HifTaskMetadataCollection)
    foreach ($other in $allTasks) {
        if ($other.path -eq $taskDirectory -or $other.status -notin $activeStatuses) { continue }
        foreach ($otherPath in @($other.editPaths)) {
            foreach ($ownPath in $ownPaths) {
                if (Test-HifPathsOverlap ([string]$ownPath) ([string]$otherPath)) {
                    $overlaps += [pscustomobject]@{ taskId = $other.taskId; sessionId = $other.sessionId; runId = $other.runId; ownPath = $ownPath; otherPath = $otherPath; taskPath = Get-HifRelativePath $other.path }
                }
            }
        }
    }
    $data = [pscustomobject]@{
        ids = [pscustomobject]@{ sessionId = $metadata.sessionId; runId = $metadata.runId; taskId = $metadata.taskId }
        taskPath = Get-HifRelativePath $taskDirectory
        kind = $metadata.kind
        phase = $metadata.phase
        status = $metadata.status
        editPaths = $ownPaths
        gitChangeCount = $changed.Count
        gitChangedPaths = @($changed | Select-Object -First 100 | ForEach-Object { [pscustomobject]@{ path = $_.path; status = $_.status } })
        gitPathsTruncated = ($changed.Count -gt 100)
        baseline = if ($baselineData) { [pscustomobject]@{ available = $true; path = Get-HifRelativePath $baselineFile; capturedAt = $baselineData.capturedAt; recordedChanges = @($baselineData.gitChanges).Count } } else { [pscustomobject]@{ available = $false; path = $null; capturedAt = $null; recordedChanges = $null } }
        overlappingTasks = @($overlaps | Sort-Object taskId, ownPath, otherPath -Unique)
        invalidTaskMetadata = @($script:HifInvalidTaskMetadata)
    }
    $warnings = @()
    if (-not $baselineData) { $warnings += '開始時 Git 差分 Baseline がありません。SummarizeChanges では開始時との比較ができません。' }
    if ($overlaps.Count -gt 0) { $warnings += '編集対象が進行中 Task と重複しています。自動変更は行っていません。' }
    if (@($script:HifInvalidTaskMetadata).Count -gt 0) { $warnings += 'メタデータを読めず競合確認に含められない Task があります。' }
    $status = if ($overlaps.Count -gt 0) { 'conflict' } else { 'inspected' }
    Write-HifResult 'InspectWorkScope' $status 0 $data $warnings
}
catch {
    Write-HifResult 'InspectWorkScope' 'error' 2 $null @() @($_.Exception.Message)
}
