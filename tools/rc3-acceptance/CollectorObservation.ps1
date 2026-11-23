function Read-Rc3CollectorLog {
    param([Parameter(Mandatory)][string]$Path)
    $bytes = [IO.File]::ReadAllBytes($Path)
    if ($bytes.Length -eq 0) { return '' }
    if ($bytes.Length -ge 2 -and $bytes[0] -eq 0xff -and $bytes[1] -eq 0xfe) { return [Text.Encoding]::Unicode.GetString($bytes, 2, $bytes.Length - 2) }
    $oddZeros = 0; $pairs = [Math]::Min([int][Math]::Floor($bytes.Length / 2), 64)
    for ($i = 0; $i -lt $pairs; $i++) { if ($bytes[$i * 2 + 1] -eq 0) { $oddZeros++ } }
    if ($pairs -ge 4 -and $oddZeros -gt ($pairs / 2)) { return [Text.Encoding]::Unicode.GetString($bytes) }
    return [Text.Encoding]::UTF8.GetString($bytes).TrimStart([char]0xfeff)
}

function Get-Rc3CollectorObservation {
    param(
        [Parameter(Mandatory)][bool]$ForcedStop,
        [Parameter(Mandatory)][int]$ExitCode,
        [AllowEmptyString()][string]$Log = '',
        [Parameter(Mandatory)][int]$DumpCount,
        [Parameter(Mandatory)][string]$ExpectedTempRoot
    )
    $reasons = @()
    if ($ForcedStop) { $reasons += 'collector did not exit after the case' }
    if ($ExitCode -ne 0) { $reasons += "collector exit code $ExitCode" }
    $target = [regex]::Match($Log, '(?im)^\s*Process:\s*FBE\.exe\s*\((?<pid>\d+)\)')
    if (-not $target.Success) { $reasons += 'attachment to FBE.exe is unconfirmed' }
    else {
        $pidText = [regex]::Escape($target.Groups['pid'].Value)
        if ($Log -notmatch ("(?im)Process Exit:\s*PID\s+${pidText},\s*Exit Code\s+0x0{8}\b")) {
            $reasons += 'clean exit of the observed FBE.exe is unconfirmed'
        }
    }
    $image = [regex]::Match($Log, '(?im)^\s*Process image:\s*(?<path>[^\r\n]+)')
    try {
        if (-not $image.Success) { throw 'image missing' }
        $imagePath = [IO.Path]::GetFullPath($image.Groups['path'].Value.Trim().Trim('"'))
        $runtimePath = [IO.Path]::GetDirectoryName($imagePath)
        $tempPath = [IO.Path]::GetFullPath($ExpectedTempRoot).TrimEnd('\')
        if ([IO.Path]::GetFileName($imagePath) -ine 'FBE.exe' -or
            [IO.Path]::GetFileName($runtimePath) -notmatch '^fr-[0-9a-f]{32}$' -or
            [IO.Path]::GetDirectoryName($runtimePath).TrimEnd('\') -ine $tempPath) { throw 'image outside isolated runtime' }
    } catch { $reasons += 'observed image is not in the isolated runtime' }
    if ($DumpCount -gt 0) { $reasons += "collector produced $DumpCount crash dump(s)" }
    [pscustomobject]@{ status = if ($reasons.Count) { 'FAIL' } else { 'PASS' }; reasons = $reasons }
}
