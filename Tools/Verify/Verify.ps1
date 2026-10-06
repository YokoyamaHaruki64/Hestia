param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('Docs', 'Project', 'Solution')]
    [string]$Mode,
    [string]$Paths = '',
    [string]$Project = '',
    [string]$Configuration = '',
    [switch]$Rebuild,
    [switch]$Help
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$utf8 = [System.Text.UTF8Encoding]::new($false)
[Console]::OutputEncoding = $utf8
$OutputEncoding = $utf8
$root = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$results = [System.Collections.Generic.List[object]]::new()
$logDirectory = $null
$allowedConfigurations = @('Debug_Editor', 'Debug_Game', 'Release_Editor', 'Release_Game')

function Invoke-LoggedCommand {
    param([string]$Executable, [string[]]$Arguments, [string]$LogName)

    $writer = [System.IO.StreamWriter]::new((Join-Path $logDirectory $LogName), $false, $utf8)
    $previousPreference = $ErrorActionPreference
    try {
        # Windows PowerShell 5.1 のネイティブ stderr をログへ取り込み、終了コードで判定する。
        $ErrorActionPreference = 'Continue'
        & $Executable @Arguments 2>&1 | ForEach-Object {
            $line = $_.ToString()
            $writer.WriteLine($line)
        }
        $exitCode = $LASTEXITCODE
        if ($exitCode -ne 0) {
            $writer.Flush()
            Write-Host "ビルドに失敗しました。ログ: $(Join-Path $logDirectory $LogName)"
            Get-Content -LiteralPath (Join-Path $logDirectory $LogName) -Encoding UTF8 | ForEach-Object { Write-Host $_ }
        }
        return $exitCode
    }
    finally {
        $ErrorActionPreference = $previousPreference
        $writer.Dispose()
    }
}

function Get-RepositoryPath {
    param([string]$Path)

    $absolute = [System.IO.Path]::GetFullPath((Join-Path $root $Path))
    if ($absolute -ne $root -and -not $absolute.StartsWith($root + '\', [StringComparison]::OrdinalIgnoreCase)) {
        throw "対象はリポジトリ内で指定してください: $Path"
    }
    if (-not (Test-Path -LiteralPath $absolute)) { throw "対象がありません: $Path" }
    if ($absolute -eq $root) { return '.' }
    return $absolute.Substring($root.Length + 1).Replace('\', '/')
}

function Get-ScopeFiles {
    param([string[]]$Scope)

    $raw = (& git -c "safe.directory=$root" -c core.quotepath=false ls-files -z --cached --others --exclude-standard -- @Scope) -join "`n"
    if ($LASTEXITCODE -ne 0) { throw 'Git のファイル一覧取得に失敗しました。' }
    return @($raw.Split([char]0) | Where-Object { $_ -and (Test-Path -LiteralPath $_ -PathType Leaf) } | Sort-Object -Unique)
}

function Show-Changes {
    param([string[]]$Scope)

    $code = Invoke-LoggedCommand 'git' @('-c', "safe.directory=$root", '-c', 'core.quotepath=false', 'status', '--short') 'git-status.log'
    if ($code -ne 0) { throw 'Git の変更状態取得に失敗しました。' }
    $code = Invoke-LoggedCommand 'git' (@('-c', "safe.directory=$root", 'diff', '--stat', 'HEAD', '--') + $Scope) 'diff-stat.log'
    if ($code -ne 0) { throw 'Git の変更量取得に失敗しました。' }
}

function Find-Conflicts {
    param([string[]]$Files)

    $findings = [System.Collections.Generic.List[string]]::new()
    foreach ($file in $Files) {
        $bytes = [System.IO.File]::ReadAllBytes((Join-Path $root $file))
        $isUtf16 = $bytes.Length -ge 2 -and (($bytes[0] -eq 255 -and $bytes[1] -eq 254) -or ($bytes[0] -eq 254 -and $bytes[1] -eq 255))
        if (-not $isUtf16 -and ($bytes | Select-Object -First 8192) -contains 0) { continue }
        $lineNumber = 0
        foreach ($line in [System.IO.File]::ReadAllLines((Join-Path $root $file))) {
            $lineNumber++
            # Markdown の Setext 見出しと衝突する ======= 単独行はエラーにしない。
            if ($line -match '^(<{7}|>{7}|\|{7})(?:\s|$)') {
                $findings.Add("${file}:${lineNumber}: 競合マーカー")
            }
        }
    }
    return $findings.ToArray()
}

function Find-BrokenLinks {
    param([string[]]$Files)

    $findings = [System.Collections.Generic.List[string]]::new()
    $script:linkCount = 0
    foreach ($file in $Files) {
        $lineNumber = 0
        $fenceCharacter = ''
        $fenceLength = 0
        foreach ($line in [System.IO.File]::ReadAllLines((Join-Path $root $file))) {
            $lineNumber++
            $fence = [regex]::Match($line, '^\s{0,3}(?<fence>`{3,}|~{3,})(?<rest>.*)$')
            if ($fence.Success) {
                $marker = $fence.Groups['fence'].Value
                if (-not $fenceCharacter) {
                    $fenceCharacter = $marker.Substring(0, 1)
                    $fenceLength = $marker.Length
                }
                elseif ($marker.StartsWith($fenceCharacter) -and $marker.Length -ge $fenceLength -and -not $fence.Groups['rest'].Value.Trim()) {
                    $fenceCharacter = ''
                }
                continue
            }
            if ($fenceCharacter) { continue }
            $visible = [regex]::Replace($line, '(`+).*?\1', '')
            $destinations = [System.Collections.Generic.List[string]]::new()
            $definition = [regex]::Match($visible, '^\s{0,3}\[[^\]]+\]:\s*(?<path><[^>]+>|\S+)')
            if ($definition.Success) { $destinations.Add($definition.Groups['path'].Value) }

            foreach ($start in [regex]::Matches($visible, '!?\[[^\]]*\]\(')) {
                $tail = $visible.Substring($start.Index + $start.Length).TrimStart()
                if ($tail.StartsWith('<')) {
                    $end = $tail.IndexOf('>')
                    if ($end -gt 0) { $destinations.Add($tail.Substring(0, $end + 1)) }
                    continue
                }
                $depth = 0
                $length = 0
                while ($length -lt $tail.Length) {
                    $character = $tail[$length]
                    if ($character -eq '\' -and $length + 1 -lt $tail.Length) { $length += 2; continue }
                    if ($character -eq '(') { $depth++ }
                    elseif ($character -eq ')') {
                        if ($depth -eq 0) { break }
                        $depth--
                    }
                    elseif ([char]::IsWhiteSpace($character) -and $depth -eq 0) { break }
                    $length++
                }
                if ($length -gt 0) { $destinations.Add($tail.Substring(0, $length)) }
            }

            foreach ($destination in $destinations) {
                $destination = $destination.Trim('<', '>')
                if ($destination -match '^(?:#|//|[a-zA-Z][a-zA-Z0-9+.-]*:)') { continue }
                $pathPart = ($destination -split '[?#]', 2)[0]
                if (-not $pathPart) { continue }
                $script:linkCount++
                $pathPart = [Uri]::UnescapeDataString($pathPart)
                $pathPart = [regex]::Replace($pathPart, '\\([\\ ()\[\]])', '$1')
                $target = Join-Path (Split-Path (Join-Path $root $file) -Parent) $pathPart
                if (-not (Test-Path -LiteralPath $target)) {
                    $findings.Add("${file}:${lineNumber}: リンク先がありません: $destination")
                }
            }
        }
    }
    return $findings.ToArray()
}

function Get-MSBuild {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path -LiteralPath $vswhere)) { throw 'VS 2022 Installer の vswhere.exe がありません。' }
    $found = @(& $vswhere -latest -products '*' -version '[17.0,18.0)' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -find 'MSBuild\Current\Bin\MSBuild.exe')
    if ($LASTEXITCODE -ne 0 -or $found.Count -eq 0) {
        throw 'VS 2022 の C++ ビルドツールが見つかりません。Desktop development with C++ を確認してください。'
    }
    return $found[0]
}

function Invoke-Build {
    param([string]$Target, [string]$BuildConfiguration, [string]$Label)

    $buildTarget = if ($Rebuild) { 'Rebuild' } else { 'Build' }
    $arguments = @($Target, '/nologo', '/m', "/t:$buildTarget", "/p:Configuration=$BuildConfiguration", '/p:Platform=x64', '/p:BuildProjectReferences=true', "/p:SolutionDir=$root/", '/verbosity:minimal', "/bl:$(Join-Path $logDirectory ($Label + '.binlog'))", "/flp:LogFile=$(Join-Path $logDirectory ($Label + '.log'));Verbosity=normal;Encoding=UTF-8")
    Write-Host "Build: $Target ($BuildConfiguration|x64, $buildTarget)"
    $code = Invoke-LoggedCommand $script:msbuild $arguments ($Label + '-console.log')
    $results.Add([pscustomobject]@{ Name = $Label; ExitCode = $code; Status = $(if ($code -eq 0) { 'PASS' } else { 'FAIL' }) })
}

if ($Help) {
    Write-Host 'VerifyDocs.cmd [-Paths "Docs;AGENTS.md"]'
    Write-Host 'VerifyProject.cmd -Project App [-Configuration Debug_Editor] [-Rebuild] [-Paths "additional/path"]'
    Write-Host 'BuildSolution.cmd [-Configuration All|Debug_Editor|Debug_Game|Release_Editor|Release_Game] [-Rebuild]'
    Write-Host 'Project accepts a unique vcxproj filename stem or a repository-relative vcxproj path. See Tools/Verify/README.md.'
    exit 0
}

$exitCode = 2
Push-Location $root
try {
    if ($Mode -eq 'Docs' -and ($Project -or $Configuration -or $Rebuild)) { throw 'Docs にはビルド用の引数を指定できません。' }
    if ($Mode -eq 'Solution' -and ($Project -or $Paths)) { throw 'Solution は Hestia.sln のビルドだけを行います。' }
    if ($Mode -eq 'Project' -and -not $Project) { throw '対象を -Project App または -Project Engine の形式で指定してください。' }
    if ($Mode -ne 'Docs') {
        if (-not $Configuration) { $Configuration = if ($Mode -eq 'Solution') { 'All' } else { 'Debug_Editor' } }
        if ($Configuration -notin ($allowedConfigurations + 'All') -or ($Mode -eq 'Project' -and $Configuration -eq 'All')) { throw "構成が不正です: $Configuration" }
    }
    $stamp = Get-Date -Format 'yyyyMMdd-HHmmss-fff'
    $logDirectory = Join-Path $root "Build/Verification/$stamp-$Mode-$([Guid]::NewGuid().ToString('N').Substring(0, 8))"
    New-Item -ItemType Directory -Path $logDirectory -Force | Out-Null
    Write-Host "Logs: $logDirectory"

    if ($Mode -eq 'Docs') {
        if (-not $Paths) { $Paths = 'Docs;AGENTS.md' }
        $scope = @($Paths.Split(';') | Where-Object { $_.Trim() } | ForEach-Object { Get-RepositoryPath $_.Trim() })
        if ($scope.Count -eq 0) { throw '対象パスが空です。' }
        Show-Changes $scope
        $files = @(Get-ScopeFiles $scope | Where-Object { [IO.Path]::GetExtension($_) -eq '.md' })
        if ($files.Count -eq 0) { throw '対象に Markdown ファイルがありません。' }
        [IO.File]::WriteAllLines((Join-Path $logDirectory 'checked-files.log'), [string[]]$files, $utf8)
        $findings = @(Find-Conflicts $files) + @(Find-BrokenLinks $files)
        foreach ($finding in $findings) { Write-Host $finding }
        [IO.File]::WriteAllLines((Join-Path $logDirectory 'docs-findings.log'), [string[]]$findings, $utf8)
        $results.Add([pscustomobject]@{ Name = 'Docs'; Files = $files.Count; Links = $script:linkCount; Findings = $findings.Count; ExitCode = $(if ($findings.Count) { 1 } else { 0 }); Status = $(if ($findings.Count) { 'FAIL' } else { 'PASS' }) })
    }
    elseif ($Mode -eq 'Project') {
        if ($Project.EndsWith('.vcxproj', [StringComparison]::OrdinalIgnoreCase)) {
            $target = Get-RepositoryPath $Project
        }
        else {
            $candidates = @(Get-ScopeFiles @('.') | Where-Object { [IO.Path]::GetExtension($_) -eq '.vcxproj' -and [IO.Path]::GetFileNameWithoutExtension($_) -eq $Project })
            if ($candidates.Count -ne 1) { throw "プロジェクト名が一意に解決できません: $Project。vcxproj のパスを指定してください。" }
            $target = $candidates[0]
        }
        $scope = @((Split-Path $target -Parent))
        if (-not $scope[0]) { $scope[0] = '.' }
        if ($Paths) { $scope += @($Paths.Split(';') | Where-Object { $_.Trim() } | ForEach-Object { Get-RepositoryPath $_.Trim() }) }
        Show-Changes $scope
        $files = @(Get-ScopeFiles $scope)
        [IO.File]::WriteAllLines((Join-Path $logDirectory 'checked-files.log'), [string[]]$files, $utf8)
        $findings = @(Find-Conflicts $files)
        [IO.File]::WriteAllLines((Join-Path $logDirectory 'project-findings.log'), [string[]]$findings, $utf8)
        if ($findings.Count) {
            foreach ($finding in $findings) { Write-Host $finding }
            $results.Add([pscustomobject]@{ Name = 'Project check'; ExitCode = 1; Status = 'FAIL'; Build = 'NOT RUN' })
        }
        else {
            $script:msbuild = Get-MSBuild
            Invoke-Build $target $Configuration ([IO.Path]::GetFileNameWithoutExtension($target) + '-' + $Configuration)
        }
    }
    else {
        $target = Get-RepositoryPath 'Hestia.sln'
        $script:msbuild = Get-MSBuild
        $configurations = if ($Configuration -eq 'All') { $allowedConfigurations } else { @($Configuration) }
        foreach ($buildConfiguration in $configurations) { Invoke-Build $target $buildConfiguration ('Solution-' + $buildConfiguration) }
    }
    $exitCode = if (@($results | Where-Object { $_.ExitCode -ne 0 }).Count) { 1 } else { 0 }
}
catch {
    Write-Host "ERROR: $($_.Exception.Message)"
    $results.Add([pscustomobject]@{ Name = 'Setup/verification'; ExitCode = 2; Status = 'ERROR'; Message = $_.Exception.Message })
    $exitCode = 2
}
finally {
    if ($logDirectory) {
        $summary = [pscustomobject]@{ Mode = $Mode; Paths = $Paths; Project = $Project; Configuration = $Configuration; Rebuild = [bool]$Rebuild; ExitCode = $exitCode; Results = @($results.ToArray()); ManualRuntimeCheck = 'NOT RUN' }
        [IO.File]::WriteAllText((Join-Path $logDirectory 'summary.json'), ($summary | ConvertTo-Json -Depth 5), $utf8)
        foreach ($result in $results) { Write-Host "$($result.Status): $($result.Name) (exit $($result.ExitCode))" }
        Write-Host "Logs: $logDirectory"
    }
    Pop-Location
}
exit $exitCode
