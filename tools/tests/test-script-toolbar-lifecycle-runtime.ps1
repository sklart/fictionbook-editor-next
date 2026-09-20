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
$portableData = [IO.Path]::GetFullPath((Join-Path $exeDirectory 'ScriptToolbarLifecyclePortable'))
if((Split-Path -Parent $portableData) -ne [IO.Path]::GetFullPath($exeDirectory)) { throw 'Unsafe portable lifecycle test directory.' }
if($IncludeInstalled -and $env:FBE_CI_ISOLATED_PROFILE -ne '1') {
    throw 'Installed lifecycle test requires FBE_CI_ISOLATED_PROFILE=1 and must not run against a developer profile.'
}
function Reset-PortableLifecycleState {
    # `$portableData` is validated as a direct child of `$exeDirectory` above.
    if(Test-Path -LiteralPath $portableData) { Remove-Item -LiteralPath $portableData -Recurse -Force }
    New-Item -ItemType Directory -Path $portableData -Force | Out-Null
}
$installedProfile = Join-Path ([IO.Path]::GetTempPath()) ('fbe-script-toolbar-lifecycle-' + [guid]::NewGuid().ToString('N'))
$savedTestSettingsDirectory = $env:FBE_NEXT_TEST_SETTINGS_DIRECTORY
try {
    [IO.File]::WriteAllText($portableIni, "[Portable]`r`nDataPath=ScriptToolbarLifecyclePortable`r`n", [Text.UTF8Encoding]::new($false))
    Reset-PortableLifecycleState
    Invoke-Lifecycle '--portable' 'script-toolbar-rollback-no-main-runtime' (Join-Path $portableData 'Diagnostics')
    Reset-PortableLifecycleState
    Invoke-Lifecycle '--portable' 'script-toolbar-rollback-persisted-runtime' (Join-Path $portableData 'Diagnostics')
    Reset-PortableLifecycleState
    Invoke-Lifecycle '--portable' 'script-toolbar-rollback-partial-runtime' (Join-Path $portableData 'Diagnostics')
    Reset-PortableLifecycleState
    Invoke-Lifecycle '--portable' 'script-toolbar-lifecycle-runtime' (Join-Path $portableData 'Diagnostics')
    Invoke-Lifecycle '--portable' 'script-toolbar-lifecycle-reload-runtime' (Join-Path $portableData 'Diagnostics')
    if($IncludeInstalled) {
        # Test mode redirects the installed data directory without touching the
        # caller's LocalAppData profile.
        New-Item -ItemType Directory -Path $installedProfile -Force | Out-Null
        $env:FBE_NEXT_TEST_SETTINGS_DIRECTORY = $installedProfile
        $installedDiagnostics = Join-Path $installedProfile 'Diagnostics'
        Invoke-Lifecycle '--installed' 'script-toolbar-rollback-no-main-runtime' $installedDiagnostics
        Invoke-Lifecycle '--installed' 'script-toolbar-lifecycle-runtime' $installedDiagnostics
        Invoke-Lifecycle '--installed' 'script-toolbar-lifecycle-reload-runtime' $installedDiagnostics
        Invoke-Lifecycle '--installed' 'script-toolbar-rollback-persisted-runtime' $installedDiagnostics
        Invoke-Lifecycle '--installed' 'script-toolbar-rollback-partial-runtime' $installedDiagnostics
    }
    Write-Host ('Script toolbar lifecycle runtime regression passed ({0}).' -f $(if($IncludeInstalled) { 'portable + installed' } else { 'portable' }))
} finally {
    $env:FBE_NEXT_TEST_SETTINGS_DIRECTORY = $savedTestSettingsDirectory
    if($hadIni) { [IO.File]::WriteAllText($portableIni, $oldIni, [Text.UTF8Encoding]::new($false)) } else { Remove-Item -LiteralPath $portableIni -Force -ErrorAction SilentlyContinue }
    if(Test-Path -LiteralPath $portableData) { Remove-Item -LiteralPath $portableData -Recurse -Force -ErrorAction SilentlyContinue }
    if(Test-Path -LiteralPath $installedProfile) { Remove-Item -LiteralPath $installedProfile -Recurse -Force -ErrorAction SilentlyContinue }
}
