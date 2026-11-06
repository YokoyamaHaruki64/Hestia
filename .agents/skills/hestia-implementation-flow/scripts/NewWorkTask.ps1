param(
    [Parameter(Mandatory = $true)][string]$SessionId,
    [Parameter(Mandatory = $true)][string]$RunId,
    [Parameter(Mandatory = $true)][string]$TaskName,
    [Parameter(Mandatory = $true)][string]$Purpose,
    [string]$RepositoryRoot = '',
    [string]$TaskId,
    [ValidateSet('research', 'implementation')][string]$Kind = 'implementation',
    [ValidateSet('research', 'header', 'test', 'implementation', 'review', 'complete')][string]$Phase = 'header',
    [ValidateSet('ready', 'in_progress', 'awaiting_review', 'awaiting_upper', 'awaiting_user', 'complete')][string]$Status = 'ready',
    [string]$AssignedModel,
    [string]$AssignedEffort,
    [string[]]$EditPaths = @(),
    [ValidateSet('pending', 'run', 'skip')][string]$VerificationDecision = 'pending',
    [string]$VerificationReason = '',
    [string]$VerificationStepsJson = '[]'
)

. (Join-Path $PSScriptRoot 'Common.ps1')
try {
    Set-HifRepositoryRoot $RepositoryRoot
    if (-not (Test-HifSafeId $SessionId) -or -not (Test-HifSafeId $RunId)) { throw 'sessionId と runId は安全な ID 形式で指定してください。' }
    $assignedModelValue = $null
    $assignedEffortValue = $null
    if (-not [string]::IsNullOrWhiteSpace($AssignedModel)) { $assignedModelValue = $AssignedModel }
    if (-not [string]::IsNullOrWhiteSpace($AssignedEffort)) { $assignedEffortValue = $AssignedEffort }
    if (($AssignedModel -and -not $AssignedEffort) -or ($AssignedEffort -and -not $AssignedModel)) { throw 'AssignedModel と AssignedEffort は両方を指定してください。' }
    if ($Kind -eq 'research' -and $EditPaths.Count -gt 0) { throw 'research Task の editPaths は空にしてください。' }
    if ($VerificationDecision -eq 'run' -and -not $VerificationReason.Trim()) { throw 'verification=run では理由が必要です。' }
    if ($VerificationDecision -eq 'skip' -and -not $VerificationReason.Trim()) { throw 'verification=skip では理由が必要です。' }
    $stepsText = $VerificationStepsJson.TrimStart()
    if (-not $stepsText.StartsWith('[')) { throw 'VerificationStepsJson は JSON 配列である必要があります。' }
    if ($stepsText -eq '[]') { $steps = [object[]]@() }
    else { $steps = @(ConvertFrom-Json -InputObject $VerificationStepsJson -ErrorAction Stop) }
    if ($VerificationDecision -eq 'run' -and $steps.Count -eq 0) { throw 'verification=run では1件以上の step が必要です。' }
    if ($VerificationDecision -eq 'skip' -and $steps.Count -gt 0) { throw 'verification=skip では steps を空にしてください。' }
    foreach ($path in $EditPaths) { [void](Get-HifRepositoryPath $path -AllowMissing) }

    $sessionDirectory = Join-Path (Get-HifWorkingRoot) "session-$SessionId"
    $runDirectory = Join-Path $sessionDirectory "run-$RunId"
    if (-not (Test-Path -LiteralPath $sessionDirectory -PathType Container)) { throw "親 Session がありません: $SessionId" }
    if (-not (Test-Path -LiteralPath $runDirectory -PathType Container)) { throw "親 Run がありません: $RunId" }

    if (-not $TaskId) {
        $slug = [regex]::Replace($TaskName.ToLowerInvariant(), '[^a-z0-9]+', '-').Trim('-')
        if (-not $slug) { $slug = 'task' }
        if ($slug.Length -gt 32) { $slug = $slug.Substring(0, 32).TrimEnd('-') }
        do { $TaskId = '{0}-{1}' -f $slug, [Guid]::NewGuid().ToString('N').Substring(0, 8) }
        while (Test-Path -LiteralPath (Join-Path $runDirectory "Task-$TaskId"))
    }
    if (-not (Test-HifSafeId $TaskId)) { throw "taskId が安全な形式ではありません: $TaskId" }
    $taskDirectory = Join-Path $runDirectory "Task-$TaskId"
    if (Test-Path -LiteralPath $taskDirectory) { throw "Task の上書きを拒否しました: $TaskId" }

    $assetDirectory = Join-Path $script:HifSkillDirectory 'assets'
    $workingTemplateDirectory = Join-Path (Get-HifWorkingRoot) '_Template'
    $assetTaskTemplate = Join-Path $assetDirectory 'Task.md'
    $assetReportTemplate = Join-Path $assetDirectory 'Report.md'
    if (-not (Test-Path -LiteralPath $assetTaskTemplate -PathType Leaf) -or -not (Test-Path -LiteralPath $assetReportTemplate -PathType Leaf)) { throw 'assets/Task.md と assets/Report.md が必要です。' }
    [void][System.IO.Directory]::CreateDirectory($workingTemplateDirectory)
    foreach ($template in @(
        [pscustomobject]@{ source = $assetTaskTemplate; destination = (Join-Path $workingTemplateDirectory 'Task.md') },
        [pscustomobject]@{ source = $assetReportTemplate; destination = (Join-Path $workingTemplateDirectory 'Report.md') }
    )) {
        if (-not (Test-Path -LiteralPath $template.destination -PathType Leaf)) {
            try { [System.IO.File]::Copy($template.source, $template.destination, $false) }
            catch {
                if (-not (Test-Path -LiteralPath $template.destination -PathType Leaf)) { throw }
            }
        }
    }
    $taskTemplate = Join-Path $workingTemplateDirectory 'Task.md'
    $reportTemplate = Join-Path $workingTemplateDirectory 'Report.md'
    $taskTemplateDocument = Get-HifFirstJsonMetadata $taskTemplate
    $reportTemplateDocument = Get-HifFirstJsonMetadata $reportTemplate
    $taskMetadata = $taskTemplateDocument.Metadata
    $reportMetadata = $reportTemplateDocument.Metadata
    foreach ($metadata in @($taskMetadata, $reportMetadata)) {
        $metadata.sessionId = $SessionId
        $metadata.runId = $RunId
        $metadata.taskId = $TaskId
        $metadata.phase = $Phase
        $metadata.status = $Status
    }
    $taskMetadata.kind = $Kind
    $taskMetadata.assignedModel = [pscustomobject]@{ model = $assignedModelValue; effort = $assignedEffortValue }
    $taskMetadata.editPaths = @($EditPaths)
    $taskMetadata.verification = [pscustomobject]@{ decision = $VerificationDecision; reason = $VerificationReason; steps = @($steps) }

    $taskBody = $taskTemplateDocument.Body
    $workerGuideLink = '(../../../../../.agents/skills/hestia-implementation-flow/references/WorkerGuide.md)'
    $taskBody = $taskBody.Replace('(../references/WorkerGuide.md)', $workerGuideLink)
    $taskBody = $taskBody.Replace('(../../../.agents/skills/hestia-implementation-flow/references/WorkerGuide.md)', $workerGuideLink)
    $taskBody = [regex]::Replace($taskBody, '(?m)^# Task.*$', [System.Text.RegularExpressions.MatchEvaluator]{ param($match) '# Task: ' + $TaskName }, 1)
    $purposeText = $Purpose.Trim()
    if ($EditPaths.Count -gt 0) { $purposeText += "`n`nTarget paths: " + ($EditPaths -join ', ') }
    $purposePattern = '(?ms)(^## Purpose and Scope\s*\r?\n).*?(?=^##\s+|\z)'
    if ([regex]::IsMatch($taskBody, $purposePattern)) {
        $taskBody = [regex]::Replace($taskBody, $purposePattern, [System.Text.RegularExpressions.MatchEvaluator]{ param($match) $match.Groups[1].Value + $purposeText + "`n`n" }, 1)
    }
    else { $taskBody += "`n`n## Purpose and Scope`n`n$purposeText`n" }

    $baseline = Get-HifGitSnapshot
    [void][System.IO.Directory]::CreateDirectory($taskDirectory)
    $lockPath = Join-Path $taskDirectory '.creating'
    $lockStream = $null
    try { $lockStream = [System.IO.File]::Open($lockPath, [System.IO.FileMode]::CreateNew, [System.IO.FileAccess]::Write, [System.IO.FileShare]::None) }
    catch { throw "Task の作成競合または既存フォルダーを検出しました: $TaskId" }
    try {
        $taskPath = Join-Path $taskDirectory 'Task.md'
        $reportPath = Join-Path $taskDirectory 'Report.md'
        if ((Test-Path -LiteralPath $taskPath) -or (Test-Path -LiteralPath $reportPath)) { throw "Task / Report の上書きを拒否しました: $TaskId" }
        $taskJson = $taskMetadata | ConvertTo-Json -Depth 16
        $reportJson = $reportMetadata | ConvertTo-Json -Depth 16
        $taskContent = '```json' + "`n" + $taskJson + "`n" + '```' + "`n`n" + $taskBody.Trim() + "`n"
        $reportContent = '```json' + "`n" + $reportJson + "`n" + '```' + "`n`n" + $reportTemplateDocument.Body.Trim() + "`n"
        Write-HifUtf8File $taskPath $taskContent
        Write-HifUtf8File $reportPath $reportContent
        $baselineDocument = [ordered]@{ schemaVersion = 1; capturedAt = (Get-Date).ToUniversalTime().ToString('o'); sessionId = $SessionId; runId = $RunId; taskId = $TaskId; gitChanges = @($baseline) }
        Write-HifUtf8File (Join-Path $taskDirectory 'Baseline.json') ($baselineDocument | ConvertTo-Json -Depth 12) -NoBom
    }
    finally {
        if ($lockStream) { $lockStream.Dispose() }
        if (Test-Path -LiteralPath $lockPath) { Remove-Item -LiteralPath $lockPath -Force }
    }

    $data = [pscustomobject]@{
        sessionId = $SessionId
        runId = $RunId
        taskId = $TaskId
        taskPath = Get-HifRelativePath $taskDirectory
        taskFile = Get-HifRelativePath $taskPath
        reportFile = Get-HifRelativePath $reportPath
        baselineFile = Get-HifRelativePath (Join-Path $taskDirectory 'Baseline.json')
        assignedModel = $taskMetadata.assignedModel
        editPaths = @($EditPaths)
        verification = $taskMetadata.verification
    }
    Write-HifResult 'NewWorkTask' 'created' 0 $data @() @() @($data.taskFile, $data.reportFile, $data.baselineFile)
}
catch {
    Write-HifResult 'NewWorkTask' 'error' 2 $null @() @($_.Exception.Message)
}
