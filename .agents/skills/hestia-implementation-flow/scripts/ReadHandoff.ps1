param(
    [Parameter(Mandatory = $true)][string]$TaskPath,
    [Parameter(Mandatory = $true)][ValidateSet('Task', 'Report')][string]$Source,
    [Parameter(Mandatory = $true)][string[]]$Sections,
    [string]$RepositoryRoot = ''
)

. (Join-Path $PSScriptRoot 'Common.ps1')
try {
    Set-HifRepositoryRoot $RepositoryRoot
    $taskDirectory = Get-HifTaskPath $TaskPath
    $fileName = if ($Source -eq 'Task') { 'Task.md' } else { 'Report.md' }
    $path = Join-Path $taskDirectory $fileName
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "$fileName がありません。" }
    $document = Get-HifFirstJsonMetadata $path
    $requested = [ordered]@{}
    foreach ($section in $Sections) {
        if ([string]::IsNullOrWhiteSpace($section)) { throw '空の section 名は指定できません。' }
        if ($section -eq 'metadata') { $requested.metadata = $document.Metadata; continue }
        $text = Get-HifMarkdownSection $document.Body $section
        if ($null -eq $text) { $requested[$section] = $null }
        else { $requested[$section] = $text }
    }
    $data = [pscustomobject]@{
        source = $Source
        taskPath = Get-HifRelativePath $taskDirectory
        file = Get-HifRelativePath $path
        sections = [pscustomobject]$requested
    }
    $missing = @($requested.Keys | Where-Object { $null -eq $requested[$_] })
    $warnings = if ($missing.Count -gt 0) { @('指定された見出しが見つかりません: ' + ($missing -join ', ')) } else { @() }
    Write-HifResult 'ReadHandoff' 'read' 0 $data $warnings
}
catch {
    Write-HifResult 'ReadHandoff' 'error' 2 $null @() @($_.Exception.Message)
}
