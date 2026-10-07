Set-StrictMode -Version Latest

$script:HifScriptsDirectory = $PSScriptRoot
$script:HifSkillDirectory = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$script:HifRepositoryRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\..\..'))
$script:HifUtf8Bom = [System.Text.UTF8Encoding]::new($true)
$script:HifUtf8NoBom = [System.Text.UTF8Encoding]::new($false)

function Write-HifResult {
    param(
        [Parameter(Mandatory = $true)][string]$Command,
        [Parameter(Mandatory = $true)][string]$Status,
        [Parameter(Mandatory = $true)][int]$ExitCode,
        [object]$Data = $null,
        [string[]]$Warnings = @(),
        [string[]]$Errors = @(),
        [string[]]$Artifacts = @()
    )

    $warningList = [System.Collections.Generic.List[string]]::new()
    $errorList = [System.Collections.Generic.List[string]]::new()
    $artifactList = [System.Collections.Generic.List[string]]::new()
    foreach ($warning in $Warnings) { if ($null -ne $warning) { $warningList.Add([string]$warning) } }
    foreach ($errorText in $Errors) { if ($null -ne $errorText) { $errorList.Add([string]$errorText) } }
    foreach ($artifact in $Artifacts) { if ($null -ne $artifact) { $artifactList.Add([string]$artifact) } }
    $result = [ordered]@{
        schemaVersion = 1
        command = $Command
        status = $Status
        exitCode = $ExitCode
        data = $Data
        warnings = $warningList.ToArray()
        errors = $errorList.ToArray()
        artifacts = $artifactList.ToArray()
    }
    [Console]::OutputEncoding = [System.Text.UTF8Encoding]::new($false)
    $OutputEncoding = [System.Text.UTF8Encoding]::new($false)
    [Console]::Out.WriteLine(($result | ConvertTo-Json -Depth 16 -Compress))
    exit $ExitCode
}

function ConvertFrom-HifJsonc {
    param([Parameter(Mandatory = $true)][string]$Text)

    $builder = [System.Text.StringBuilder]::new($Text.Length)
    $inString = $false
    $escaped = $false
    $inLineComment = $false
    for ($index = 0; $index -lt $Text.Length; $index++) {
        $character = $Text[$index]
        if ($inLineComment) {
            if ($character -eq "`r" -or $character -eq "`n") {
                $inLineComment = $false
                [void]$builder.Append($character)
            }
            else {
                [void]$builder.Append(' ')
            }
            continue
        }

        if ($inString) {
            [void]$builder.Append($character)
            if ($escaped) { $escaped = $false; continue }
            if ($character -eq '\') { $escaped = $true; continue }
            if ($character -eq '"') { $inString = $false }
            continue
        }

        if ($character -eq '"') {
            $inString = $true
            [void]$builder.Append($character)
            continue
        }
        if ($character -eq '/' -and $index + 1 -lt $Text.Length -and $Text[$index + 1] -eq '/') {
            $inLineComment = $true
            [void]$builder.Append('  ')
            $index++
            continue
        }
        [void]$builder.Append($character)
    }
    return $builder.ToString()
}

function Get-HifRepositoryPath {
    param(
        [Parameter(Mandatory = $true)][string]$RelativePath,
        [switch]$AllowMissing
    )

    if ([string]::IsNullOrWhiteSpace($RelativePath) -or [System.IO.Path]::IsPathRooted($RelativePath)) {
        throw "リポジトリ相対パスを指定してください: $RelativePath"
    }
    $normalized = $RelativePath.Replace('/', '\')
    if ($normalized -match '(^|\\)\.\.(\\|$)' -or $normalized -match '(^|\\)\.($|\\)') {
        throw "上位・カレントディレクトリを含むパスは指定できません: $RelativePath"
    }
    $absolute = [System.IO.Path]::GetFullPath((Join-Path $script:HifRepositoryRoot $normalized))
    $root = $script:HifRepositoryRoot.TrimEnd('\')
    if ($absolute -ne $root -and -not $absolute.StartsWith($root + '\', [StringComparison]::OrdinalIgnoreCase)) {
        throw "リポジトリ外のパスは指定できません: $RelativePath"
    }
    if (-not $AllowMissing -and -not (Test-Path -LiteralPath $absolute)) {
        throw "指定したパスがありません: $RelativePath"
    }
    return $absolute
}

function Set-HifRepositoryRoot {
    param([Parameter(Mandatory = $true)][AllowEmptyString()][string]$Path)

    if ([string]::IsNullOrWhiteSpace($Path)) { return }
    $resolved = [System.IO.Path]::GetFullPath($Path)
    if (-not (Test-Path -LiteralPath (Join-Path $resolved '.git')) -and -not (Test-Path -LiteralPath (Join-Path $resolved '.git') -PathType Leaf)) {
        throw "RepositoryRoot に Git リポジトリを指定してください: $Path"
    }
    $script:HifRepositoryRoot = $resolved
}

function Get-HifRelativePath {
    param([Parameter(Mandatory = $true)][string]$AbsolutePath)

    $absolute = [System.IO.Path]::GetFullPath($AbsolutePath)
    $root = $script:HifRepositoryRoot.TrimEnd('\')
    if ($absolute -eq $root) { return '.' }
    if (-not $absolute.StartsWith($root + '\', [StringComparison]::OrdinalIgnoreCase)) {
        throw "リポジトリ外のパスです: $AbsolutePath"
    }
    return $absolute.Substring($root.Length + 1).Replace('\', '/')
}

function Test-HifSafeId {
    param([Parameter(Mandatory = $true)][string]$Id)
    return ($Id -match '^[A-Za-z0-9][A-Za-z0-9._-]{0,63}$' -and $Id -ne '.' -and $Id -ne '..')
}

function Test-HifProperty {
    param([object]$InputObject, [Parameter(Mandatory = $true)][string]$Name)
    return ($null -ne $InputObject -and $null -ne $InputObject.PSObject.Properties[$Name])
}

function Get-HifWorkingRoot {
    return [System.IO.Path]::GetFullPath((Join-Path $script:HifRepositoryRoot '.codex\working'))
}

function Get-HifFirstJsonMetadata {
    param([Parameter(Mandatory = $true)][string]$Path)

    $content = [System.IO.File]::ReadAllText($Path).TrimStart([char]0xFEFF)
    $match = [regex]::Match($content, '(?s)```(?<language>[A-Za-z0-9_-]*)[ \t]*\r?\n(?<json>.*?)\r?\n```')
    if (-not $match.Success) { throw "JSON メタデータのコードブロックがありません: $Path" }
    if ($match.Groups['language'].Value -and $match.Groups['language'].Value -ne 'json') { throw "最初のコードブロックは JSON である必要があります: $Path" }
    $metadata = $match.Groups['json'].Value | ConvertFrom-Json -ErrorAction Stop
    $body = $content.Substring(0, $match.Index) + $content.Substring($match.Index + $match.Length)
    $body = $body.Trim("`r", "`n")
    return [pscustomobject]@{ Content = $content; Metadata = $metadata; Body = $body; Match = $match }
}

function Set-HifFirstJsonMetadata {
    param([Parameter(Mandatory = $true)][string]$Path, [Parameter(Mandatory = $true)]$Metadata)

    $document = Get-HifFirstJsonMetadata $Path
    $json = $Metadata | ConvertTo-Json -Depth 16
    $replacement = '```json' + "`n" + $json + "`n" + '```'
    $content = $document.Content.Substring(0, $document.Match.Index) + $replacement + $document.Content.Substring($document.Match.Index + $document.Match.Length)
    Write-HifUtf8File $Path $content
}

function Write-HifUtf8File {
    param([Parameter(Mandatory = $true)][string]$Path, [Parameter(Mandatory = $true)][AllowEmptyString()][string]$Content, [switch]$NoBom)

    $encoding = if ($NoBom) { $script:HifUtf8NoBom } else { $script:HifUtf8Bom }
    $normalized = $Content.Replace("`r`n", "`n").Replace("`r", "`n")
    [System.IO.File]::WriteAllText($Path, $normalized, $encoding)
}

function Get-HifTaskPath {
    param([Parameter(Mandatory = $true)][string]$TaskPath)

    $absolute = Get-HifRepositoryPath $TaskPath
    $working = (Get-HifWorkingRoot).TrimEnd('\')
    if (-not $absolute.StartsWith($working + '\', [StringComparison]::OrdinalIgnoreCase)) {
        throw "Task は .codex/working 配下を指定してください: $TaskPath"
    }
    if (-not (Test-Path -LiteralPath (Join-Path $absolute 'Task.md') -PathType Leaf)) {
        throw "Task.md がありません: $TaskPath"
    }
    return $absolute
}

function Get-HifMarkdownSection {
    param([Parameter(Mandatory = $true)][string]$Markdown, [Parameter(Mandatory = $true)][string]$Heading)

    $escaped = [regex]::Escape($Heading)
    $pattern = '(?ms)^#{1,6}\s+' + $escaped + '\s*\r?\n(?<body>.*?)(?=^#{1,6}\s+|\z)'
    $match = [regex]::Match($Markdown, $pattern)
    if (-not $match.Success) { return $null }
    return $match.Groups['body'].Value.Trim()
}

function Get-HifGitSnapshot {
    $root = $script:HifRepositoryRoot
    $statusLines = @(& git -c "safe.directory=$root" -c core.quotepath=false -C $root status --porcelain=v1 --untracked-files=all 2>$null)
    if ($LASTEXITCODE -ne 0) { throw 'Git status の取得に失敗しました。' }
    $items = @()
    foreach ($line in $statusLines) {
        if ($line.Length -lt 4) { continue }
        $relative = $line.Substring(3).Trim('"')
        if ($relative -match ' -> ') { $relative = ($relative -split ' -> ', 2)[1].Trim('"') }
        $absolute = [System.IO.Path]::GetFullPath((Join-Path $root $relative))
        $hash = $null
        if (Test-Path -LiteralPath $absolute -PathType Leaf) {
            $hash = (Get-FileHash -LiteralPath $absolute -Algorithm SHA256).Hash
        }
        $items += [pscustomobject]@{ path = $relative.Replace('\', '/'); status = $line.Substring(0, 2); hash = $hash }
    }
    return @($items | Sort-Object path -Unique)
}

function Get-HifTaskMetadataCollection {
    $working = Get-HifWorkingRoot
    $script:HifInvalidTaskMetadata = @()
    if (-not (Test-Path -LiteralPath $working -PathType Container)) { return @() }
    $tasks = @()
    foreach ($file in @(Get-ChildItem -LiteralPath $working -Filter Task.md -File -Recurse -ErrorAction SilentlyContinue)) {
        try {
            $doc = Get-HifFirstJsonMetadata $file.FullName
            $m = $doc.Metadata
            if ($null -eq $m.sessionId -or $null -eq $m.runId -or $null -eq $m.taskId) {
                if ($file.Directory.Name -notmatch '^_(?:Template|Draft)$') { $script:HifInvalidTaskMetadata += Get-HifRelativePath $file.FullName }
                continue
            }
            $tasks += [pscustomobject]@{
                path = $file.Directory.FullName
                sessionId = [string]$m.sessionId
                runId = [string]$m.runId
                taskId = [string]$m.taskId
                status = [string]$m.status
                kind = [string]$m.kind
                editPaths = @($m.editPaths)
            }
        }
        catch { $script:HifInvalidTaskMetadata += Get-HifRelativePath $file.FullName }
    }
    return $tasks
}

function Test-HifPathsOverlap {
    param([Parameter(Mandatory = $true)][string]$First, [Parameter(Mandatory = $true)][string]$Second)

    $a = $First.TrimEnd('/', '\')
    $b = $Second.TrimEnd('/', '\')
    return ($a.Equals($b, [StringComparison]::OrdinalIgnoreCase) -or
        $a.StartsWith($b + '/', [StringComparison]::OrdinalIgnoreCase) -or
        $a.StartsWith($b + '\', [StringComparison]::OrdinalIgnoreCase) -or
        $b.StartsWith($a + '/', [StringComparison]::OrdinalIgnoreCase) -or
        $b.StartsWith($a + '\', [StringComparison]::OrdinalIgnoreCase))
}
