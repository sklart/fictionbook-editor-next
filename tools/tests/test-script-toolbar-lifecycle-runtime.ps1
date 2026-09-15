<# Exercises real script-toolbar HWND/rebar lifecycle.  CI opts into installed mode on an isolated worker. #>
[CmdletBinding()]
param([Parameter(Mandatory)][string]$FbeExe, [switch]$IncludeInstalled, [ValidateRange(30, 300)][int]$TimeoutSeconds = 120)

$ErrorActionPreference = 'Stop'
$FbeExe = (Resolve-Path -LiteralPath $FbeExe).Path
$exeDirectory = Split-Path -Parent $FbeExe

function Invoke-Lifecycle([string]$Mode, [string]$Scenario, [string]$ReportDirectory) {
    $savedMode = $env:FBE_NEXT_TEST_MODE; $savedScenario = $env:FBE_NEXT_TEST_SCENARIO
    try {
        $env:FBE_NEXT_TEST_MODE = '1'; $env:FBE_NEXT_TEST_SCENARIO = $Scenario
        $process = Start-Process -FilePath $FbeExe -WorkingDirectory $exeDirectory -ArgumentList @($Mode) -PassThru
        if(-not $process.WaitForExit($TimeoutSeconds * 1000)) { Stop-Process -Id $process.Id -Force; throw "$Scenario timed out." }
        $report = Join-Path $ReportDirectory 'portable-state-report.txt'
        $text = if(Test-Path -LiteralPath $report) { Get-Content -LiteralPath $report -Raw } else { '' }
        if($process.ExitCode -ne 0 -or $text -notmatch '(?m)^result=pass$') { throw "$Mode $Scenario failed:`n$text" }
    } finally { $env:FBE_NEXT_TEST_MODE = $savedMode; $env:FBE_NEXT_TEST_SCENARIO = $savedScenario }
}

$portableIni = Join-Path $exeDirectory 'portable.ini'; $hadIni = Test-Path -LiteralPath $portableIni
$oldIni = if($hadIni) { Get-Content -LiteralPath $portableIni -Raw } else { $null }
$portableData = Join-Path $exeDirectory 'ScriptToolbarLifecyclePortable'
try {
    [IO.File]::WriteAllText($portableIni, "[Portable]`r`nDataPath=ScriptToolbarLifecyclePortable`r`n", [Text.UTF8Encoding]::new($false))
    New-Item -ItemType Directory -Path $portableData -Force | Out-Null
    Invoke-Lifecycle '--portable' 'script-toolbar-lifecycle-runtime' (Join-Path $portableData 'Diagnostics')
    Invoke-Lifecycle '--portable' 'script-toolbar-lifecycle-reload-runtime' (Join-Path $portableData 'Diagnostics')
    if($IncludeInstalled) {
        # Installed persistence is intentionally opt-in: it is safe on an
        # ephemeral CI worker and never deletes or moves a developer profile.
        Invoke-Lifecycle '--installed' 'script-toolbar-lifecycle-runtime' (Join-Path $env:LOCALAPPDATA 'FBE Next\Diagnostics')
        Invoke-Lifecycle '--installed' 'script-toolbar-lifecycle-reload-runtime' (Join-Path $env:LOCALAPPDATA 'FBE Next\Diagnostics')
    }
    Write-Host ('Script toolbar lifecycle runtime regression passed ({0}).' -f $(if($IncludeInstalled) { 'portable + installed' } else { 'portable' }))
} finally {
    if($hadIni) { [IO.File]::WriteAllText($portableIni, $oldIni, [Text.UTF8Encoding]::new($false)) } else { Remove-Item -LiteralPath $portableIni -Force -ErrorAction SilentlyContinue }
}
