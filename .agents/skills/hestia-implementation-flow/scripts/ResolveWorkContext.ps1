param(
    [Parameter(Mandatory = $true)][ValidateSet('NewSession', 'NewRun', 'Resume')][string]$Mode,
    [string]$RepositoryRoot = '',
    [string]$SessionId,
    [string]$RunId
)

. (Join-Path $PSScriptRoot 'Common.ps1')
try {
    Set-HifRepositoryRoot $RepositoryRoot
    $working = Get-HifWorkingRoot
    $issued = [ordered]@{ sessionId = $null; runId = $null }
    $sessionDirectory = $null
    $runDirectory = $null
    switch ($Mode) {
        'NewSession' {
            if ($SessionId -or $RunId) { throw 'NewSession では既存 ID を指定できません。' }
            [void][System.IO.Directory]::CreateDirectory($working)
            do {
                $SessionId = 's-{0}-{1}' -f (Get-Date).ToUniversalTime().ToString('yyyyMMddTHHmmssfffZ'), [Guid]::NewGuid().ToString('N').Substring(0, 8)
                $sessionDirectory = Join-Path $working "session-$SessionId"
            } while (Test-Path -LiteralPath $sessionDirectory)
            [void][System.IO.Directory]::CreateDirectory($sessionDirectory)
            $issued.sessionId = $SessionId
        }
        'NewRun' {
            if (-not $SessionId -or -not (Test-HifSafeId $SessionId)) { throw 'NewRun には安全な sessionId が必要です。' }
            if ($RunId) { throw 'NewRun では runId を指定できません。' }
            $sessionDirectory = Join-Path $working "session-$SessionId"
            if (-not (Test-Path -LiteralPath $sessionDirectory -PathType Container)) { throw "既存 Session がありません: $SessionId" }
            do {
                $RunId = 'r-{0}-{1}' -f (Get-Date).ToUniversalTime().ToString('yyyyMMddTHHmmssfffZ'), [Guid]::NewGuid().ToString('N').Substring(0, 8)
                $runDirectory = Join-Path $sessionDirectory "run-$RunId"
            } while (Test-Path -LiteralPath $runDirectory)
            [void][System.IO.Directory]::CreateDirectory($runDirectory)
            $issued.runId = $RunId
        }
        'Resume' {
            if (-not $SessionId -or -not (Test-HifSafeId $SessionId)) { throw 'Resume には既存の安全な sessionId が必要です。' }
            if (-not $RunId -or -not (Test-HifSafeId $RunId)) { throw 'Resume には既存の安全な runId が必要です。' }
            $sessionDirectory = Join-Path $working "session-$SessionId"
            $runDirectory = Join-Path $sessionDirectory "run-$RunId"
            if (-not (Test-Path -LiteralPath $sessionDirectory -PathType Container)) { throw "再開する Session がありません: $SessionId" }
            if (-not (Test-Path -LiteralPath $runDirectory -PathType Container)) { throw "再開する Run がありません: $RunId" }
        }
    }

    $data = [pscustomobject]@{
        mode = $Mode
        sessionId = $SessionId
        runId = $RunId
        sessionPath = Get-HifRelativePath $sessionDirectory
        runPath = if ($runDirectory) { Get-HifRelativePath $runDirectory } else { $null }
        issuedIds = [pscustomobject]$issued
    }
    Write-HifResult 'ResolveWorkContext' 'resolved' 0 $data
}
catch {
    Write-HifResult 'ResolveWorkContext' 'error' 2 $null @() @($_.Exception.Message)
}
