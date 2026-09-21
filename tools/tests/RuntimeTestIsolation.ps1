Set-StrictMode -Version Latest

function New-IsolatedFbeRuntime {
    param([Parameter(Mandatory)][string]$FbeExe, [Parameter(Mandatory)][string]$Name)

    $sourceExe = (Resolve-Path -LiteralPath $FbeExe).Path
    # Keep this deliberately shallow: bundled user-script names are long and
    # the legacy script host still observes MAX_PATH.
    $root = Join-Path ([IO.Path]::GetTempPath()) ('fr-' + [guid]::NewGuid().ToString('N'))
    $runtime = $root
    New-Item -ItemType Directory -Path $root -Force | Out-Null
    Get-ChildItem -LiteralPath (Split-Path -Parent $sourceExe) -Force | Copy-Item -Destination $runtime -Recurse -Force
    $dataName = "$Name-" + [guid]::NewGuid().ToString('N')
    $dataPath = Join-Path $runtime $dataName
    if (Test-Path -LiteralPath $dataPath) { Remove-Item -LiteralPath $dataPath -Recurse -Force }
    New-Item -ItemType Directory -Path $dataPath | Out-Null
    [IO.File]::WriteAllText((Join-Path $runtime 'portable.ini'), "[Portable]`r`nDataPath=$dataName`r`n", [Text.UTF8Encoding]::new($false))
    return [pscustomobject]@{ Root = $root; Runtime = $runtime; Exe = Join-Path $runtime 'FBE.exe'; DataPath = $dataPath; StagePath = Join-Path $root 'test-stage.txt' }
}

function Set-IsolatedRuntimeStage {
    param([Parameter(Mandatory)]$Isolation, [Parameter(Mandatory)][string]$Stage)
    [IO.File]::WriteAllText($Isolation.StagePath, "stage=$Stage`r`nutc=$([DateTime]::UtcNow.ToString('o'))`r`n", [Text.UTF8Encoding]::new($false))
}

function Get-IsolatedRuntimeProcesses {
    param([Parameter(Mandatory)]$Isolation)
    $needle = $Isolation.Exe
    return @(Get-CimInstance Win32_Process -Filter "Name = 'FBE.exe'" -ErrorAction SilentlyContinue | Where-Object {
        $_.CommandLine -and $_.CommandLine.IndexOf($needle, [StringComparison]::OrdinalIgnoreCase) -ge 0
    })
}

function Get-IsolatedRuntimeDiagnostics {
    param([Parameter(Mandatory)]$Isolation, [string]$Report)
    $reportExists = $Report -and (Test-Path -LiteralPath $Report)
    $reportSize = if ($reportExists) { (Get-Item -LiteralPath $Report).Length } else { 0 }
    $recovery = Join-Path $Isolation.DataPath 'Recovery'
    $recoveryFiles = if (Test-Path -LiteralPath $recovery) { (Get-ChildItem -LiteralPath $recovery -File -Recurse | ForEach-Object { "$($_.FullName) ($($_.Length) bytes, $($_.LastWriteTimeUtc.ToString('o')))" }) -join '; ' } else { '<missing>' }
    return "report_path=$Report; report_exists=$reportExists; report_size=$reportSize; recovery=$recovery; recovery_files=$recoveryFiles; stage=$($Isolation.StagePath)"
}

function Wait-IsolatedFbeProcess {
    param([Parameter(Mandatory)][Diagnostics.Process]$Process, [Parameter(Mandatory)]$Isolation, [Parameter(Mandatory)][string]$Scenario, [Parameter(Mandatory)][string]$Report, [ValidateRange(1, 600)][int]$TimeoutSeconds)
    if (-not $Process.WaitForExit($TimeoutSeconds * 1000)) {
        Stop-Process -Id $Process.Id -Force -ErrorAction SilentlyContinue
        $Process.WaitForExit()
        throw "FBE runtime scenario '$Scenario' timed out after $TimeoutSeconds seconds. PID=$($Process.Id); command_line=$($Process.StartInfo.FileName) $($Process.StartInfo.Arguments); $(Get-IsolatedRuntimeDiagnostics $Isolation $Report)"
    }
    $Process.WaitForExit()
    $Process.Refresh()
    if (-not $Process.HasExited) { throw "FBE runtime scenario '$Scenario': child process status unavailable. PID=$($Process.Id)" }
    return $Process.ExitCode
}

function Complete-IsolatedFbeRuntime {
    param([Parameter(Mandatory)]$Isolation, [Parameter(Mandatory)][bool]$Passed)
    $leftovers = @(Get-IsolatedRuntimeProcesses $Isolation)
    if ($leftovers.Count -gt 0) {
        foreach ($leftover in $leftovers) { Stop-Process -Id $leftover.ProcessId -Force -ErrorAction SilentlyContinue }
        throw "Disposable runtime leaked FBE.exe process(es): $($leftovers.ProcessId -join ', '). Runtime failure artifacts: $($Isolation.Root)"
    }
    if ($Passed) { Remove-Item -LiteralPath $Isolation.Root -Recurse -Force }
    else { Write-Host "Runtime failure artifacts: $($Isolation.Root)" }
}
