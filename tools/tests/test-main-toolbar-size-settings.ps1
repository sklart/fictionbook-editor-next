[CmdletBinding()]
param(
    [string]$FbeExe = (Join-Path $PSScriptRoot '../../out/Release/FBE.exe'),
    [ValidateSet('portable','installed')][string]$Mode = 'portable'
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$iso = New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'main-toolbar-size-settings'
$names = @('FBE_NEXT_TEST_MODE','FBE_NEXT_TEST_SCENARIO','FBE_NEXT_TEST_SETTINGS_DIRECTORY')
$saved = @{}; foreach ($name in $names) { $saved[$name] = [Environment]::GetEnvironmentVariable($name, 'Process') }
$passed = $false
try {
    $env:FBE_NEXT_TEST_MODE = '1'
    $env:FBE_NEXT_TEST_SETTINGS_DIRECTORY = if ($Mode -eq 'installed') { $iso.DataPath } else { $null }
    $report = Join-Path $iso.DataPath 'Diagnostics/portable-state-report.txt'
    foreach ($scenario in @('main-toolbar-size-setting-write','main-toolbar-size-setting-read')) {
        $env:FBE_NEXT_TEST_SCENARIO = $scenario
        Remove-Item -LiteralPath $report -Force -ErrorAction SilentlyContinue
        $process = Start-Process $iso.Exe -ArgumentList "--$Mode" -WorkingDirectory $iso.Root -WindowStyle Hidden -PassThru
        $code = Wait-IsolatedFbeProcess -Process $process -Isolation $iso -Scenario $scenario -Report $report -TimeoutSeconds 120
        $text = if (Test-Path -LiteralPath $report) { Get-Content -LiteralPath $report -Raw } else { '<missing report>' }
        if ($code -ne 0 -or $text -notmatch '(?m)^result=pass\r?$') { throw "Main toolbar size settings failed ($Mode/$scenario): $text" }
    }
    $passed = $true
    Write-Host "Main toolbar size default, normalization, independence and restart: PASS ($Mode)."
} finally {
    foreach ($name in $names) { [Environment]::SetEnvironmentVariable($name, $saved[$name], 'Process') }
    Complete-IsolatedFbeRuntime -Isolation $iso -Passed $passed
}
