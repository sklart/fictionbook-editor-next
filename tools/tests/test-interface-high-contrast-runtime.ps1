<# Exercises the high-contrast palette transition without changing Windows settings. #>
[CmdletBinding()]
param([string]$FbeExe = (Join-Path $PSScriptRoot '../../out/Release/FBE.exe'),
      [ValidateSet('portable','installed')][string]$Mode = 'portable')
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$iso = New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'high-contrast-theme'
$passed = $false
$names = @('FBE_NEXT_TEST_MODE','FBE_NEXT_TEST_SCENARIO','FBE_NEXT_TEST_SETTINGS_DIRECTORY','FBE_NEXT_TEST_FORCE_HIGH_CONTRAST')
$saved = @{}; foreach($name in $names) { $saved[$name] = [Environment]::GetEnvironmentVariable($name,'Process') }
try {
    [void](New-IsolatedFbeSettingsSeed -Isolation $iso)
    $env:FBE_NEXT_TEST_MODE = '1'
    $env:FBE_NEXT_TEST_SCENARIO = 'interface-high-contrast-runtime'
    $env:FBE_NEXT_TEST_FORCE_HIGH_CONTRAST = $null
    $settingsRoot = Join-Path $iso.DataPath 'Settings'
    $env:FBE_NEXT_TEST_SETTINGS_DIRECTORY = if($Mode -eq 'installed') { $settingsRoot } else { $null }
    $diagnosticsRoot = if($Mode -eq 'installed') { $settingsRoot } else { $iso.DataPath }
    $report = Join-Path $diagnosticsRoot 'Diagnostics/portable-state-report.txt'
    $process = Start-Process $iso.Exe -ArgumentList "--$Mode" -WorkingDirectory $iso.Root -PassThru
    $code = Wait-IsolatedFbeProcess -Process $process -Isolation $iso -Scenario 'interface-high-contrast-runtime' -Report $report -TimeoutSeconds 90
    $text = if(Test-Path -LiteralPath $report) { Get-Content -Raw -LiteralPath $report } else { '<missing report>' }
    Write-Host $text
    if($code -ne 0 -or $text -notmatch '(?m)^result=pass\r?$') { throw "High contrast theme policy failed ($Mode): $text" }
    foreach($field in @('custom-before','entered','system-colors','tree-system','retained','exited','restored','menu-width')) {
        if($text -notmatch ("(?m)^" + [regex]::Escape($field) + "=1\r?$")) { throw "Missing high-contrast assertion: $field" }
    }
    $passed = $true
    Write-Host "Controlled high-contrast transition: PASS ($Mode; system setting was not changed)."
} finally {
    foreach($name in $names) { [Environment]::SetEnvironmentVariable($name, $saved[$name], 'Process') }
    Complete-IsolatedFbeRuntime -Isolation $iso -Passed $passed
}
