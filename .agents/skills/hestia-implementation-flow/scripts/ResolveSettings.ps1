param(
    [string]$RepositoryRoot = '',
    [string]$SettingsPath = '.agents/skills/hestia-implementation-flow/settings.jsonc',
    [string]$UpperModel,
    [string]$UpperEffort,
    [string]$LowerModel,
    [string]$LowerEffort
)

. (Join-Path $PSScriptRoot 'Common.ps1')
try {
    Set-HifRepositoryRoot $RepositoryRoot
    $settingsFile = Get-HifRepositoryPath $SettingsPath
    $jsonText = ConvertFrom-HifJsonc -Text ([System.IO.File]::ReadAllText($settingsFile))
    $settings = ConvertFrom-Json -InputObject $jsonText -ErrorAction Stop
    $problems = [System.Collections.Generic.List[string]]::new()
    $knownEfforts = @('low', 'medium', 'high', 'xhigh', 'max', 'ultra')
    if (-not (Test-HifProperty $settings 'schemaVersion') -or $settings.schemaVersion -isnot [int] -or $settings.schemaVersion -ne 1) { $problems.Add('schemaVersion は整数 1 が必要です。') }
    foreach ($required in @('modelSelection', 'candidateSequence', 'fixedModels')) {
        if (-not (Test-HifProperty $settings $required)) { $problems.Add("$required がありません。"); continue }
    }
    if ((Test-HifProperty $settings 'candidateSequence') -and $settings.candidateSequence -isnot [System.Array]) { $problems.Add('candidateSequence は配列である必要があります。') }
    if ((Test-HifProperty $settings 'modelSelection')) {
        foreach ($role in @('upper', 'lower')) {
            if (-not (Test-HifProperty $settings.modelSelection $role)) { $problems.Add("modelSelection.$role がありません。") }
        }
    }
    if ((Test-HifProperty $settings 'fixedModels')) {
        foreach ($role in @('upper', 'lower')) {
            if (-not (Test-HifProperty $settings.fixedModels $role)) { $problems.Add("fixedModels.$role がありません。"); continue }
            $fixed = $settings.fixedModels.$role
            if (-not (Test-HifProperty $fixed 'model') -or -not (Test-HifProperty $fixed 'effort')) { $problems.Add("fixedModels.$role は model と effort が必要です。"); continue }
            if (($null -eq $fixed.model) -xor ($null -eq $fixed.effort)) { $problems.Add("fixedModels.$role は model と effort の両方を設定するか、両方を null にしてください."); continue }
            if ($null -ne $fixed.model -and ($fixed.model -isnot [string] -or [string]::IsNullOrWhiteSpace($fixed.model))) { $problems.Add("fixedModels.$role.model は空でない文字列が必要です。") }
            if ($null -ne $fixed.effort -and ($fixed.effort -isnot [string] -or $fixed.effort -notin $knownEfforts)) { $problems.Add("fixedModels.$role.effort が不正です。許可値: $($knownEfforts -join ', ')") }
        }
    }

    $resolvedRoles = [ordered]@{}
    $unconfigured = [System.Collections.Generic.List[string]]::new()
    foreach ($role in @('upper', 'lower')) {
        $modelArg = if ($role -eq 'upper') { $UpperModel } else { $LowerModel }
        $effortArg = if ($role -eq 'upper') { $UpperEffort } else { $LowerEffort }
        if (($modelArg -and -not $effortArg) -or ($effortArg -and -not $modelArg)) {
            $problems.Add("$role の明示指定は model と effort の両方が必要です。")
            continue
        }

        if ($modelArg) {
            if ($effortArg -notin $knownEfforts) { $problems.Add("$role の effort が不正です: $effortArg"); continue }
            $resolvedRoles[$role] = [pscustomobject]@{ selection = 'explicit'; value = [pscustomobject]@{ model = $modelArg; effort = $effortArg }; runtimeSupport = 'unknown' }
            continue
        }
        if (-not (Test-HifProperty $settings 'modelSelection') -or -not (Test-HifProperty $settings.modelSelection $role)) { continue }
        $selection = [string]$settings.modelSelection.$role
        if ($selection -notin @('inherit', 'fixed', 'select') -or ($role -eq 'lower' -and $selection -eq 'inherit')) {
            $problems.Add("modelSelection.$role の値が不正です: $selection")
            continue
        }
        if ($selection -eq 'inherit') {
            $resolvedRoles[$role] = [pscustomobject]@{ selection = $selection; value = $null; runtimeSupport = 'unknown'; note = '現在の実行モデルをスクリプトから照合できません。' }
            continue
        }
        if ($selection -eq 'fixed') {
            if (-not (Test-HifProperty $settings 'fixedModels') -or -not (Test-HifProperty $settings.fixedModels $role)) {
                $problems.Add("fixedModels.$role がありません。")
                continue
            }
            $value = $settings.fixedModels.$role
            $model = if (Test-HifProperty $value 'model') { $value.model } else { $null }
            $effort = if (Test-HifProperty $value 'effort') { $value.effort } else { $null }
            if (($null -eq $model) -xor ($null -eq $effort)) { $problems.Add("fixedModels.$role は model と effort の両方を設定するか、両方を null にしてください。"); continue }
            if ($null -eq $model) { $unconfigured.Add("fixedModels.$role は未設定です。"); $resolvedRoles[$role] = [pscustomobject]@{ selection = $selection; value = $null; runtimeSupport = 'unknown' } }
            elseif ($model -isnot [string] -or [string]::IsNullOrWhiteSpace($model) -or $effort -isnot [string] -or $effort -notin $knownEfforts) { $problems.Add("fixedModels.$role の model / effort が不正です。") }
            else { $resolvedRoles[$role] = [pscustomobject]@{ selection = $selection; value = [pscustomobject]@{ model = $model; effort = $effort }; runtimeSupport = 'unknown' } }
            continue
        }

        $candidates = @($settings.candidateSequence)
        $candidateValues = @()
        foreach ($candidate in $candidates) {
            if (-not (Test-HifProperty $candidate 'model') -or -not (Test-HifProperty $candidate 'effort') -or $candidate.model -isnot [string] -or [string]::IsNullOrWhiteSpace($candidate.model) -or $candidate.effort -isnot [string] -or $candidate.effort -notin $knownEfforts) {
                $problems.Add('candidateSequence の各項目には空でない model と effort が必要です。')
                continue
            }
            $candidateValues += [pscustomobject]@{ model = $candidate.model; effort = $candidate.effort; runtimeSupport = 'unknown' }
        }
        if ($candidateValues.Count -eq 0) { $unconfigured.Add('candidateSequence が空のため select を解決できません。') }
        $resolvedRoles[$role] = [pscustomobject]@{ selection = $selection; candidates = $candidateValues; runtimeSupport = 'unknown' }
    }

    $data = [pscustomobject]@{ roles = [pscustomobject]$resolvedRoles; runtimeSupport = 'unknown' }
    if ($problems.Count -gt 0) { Write-HifResult 'ResolveSettings' 'invalid' 2 $data @() $problems.ToArray() }
    $warnings = @('モデルと effort の実行時対応可否は、このスクリプトから確認できません。')
    if ($unconfigured.Count -gt 0) { $warnings += $unconfigured.ToArray() }
    $status = if ($unconfigured.Count -gt 0) { 'unconfigured' } else { 'resolved' }
    Write-HifResult 'ResolveSettings' $status 0 $data $warnings
}
catch {
    Write-HifResult 'ResolveSettings' 'error' 2 $null @() @($_.Exception.Message)
}
