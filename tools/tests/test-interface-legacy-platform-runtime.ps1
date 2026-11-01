<# Exercises light palettes and dark-mode restrictions with a simulated legacy Windows build. #>
[CmdletBinding()]
param([string]$FbeExe = (Join-Path $PSScriptRoot '../../out/Release/FBE.exe'),
      [ValidateSet('portable','installed')][string]$Mode = 'portable')
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$iso = New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'legacy-platform-theme'
$passed = $false
$names = @('FBE_NEXT_TEST_MODE','FBE_NEXT_TEST_SCENARIO','FBE_NEXT_TEST_SETTINGS_DIRECTORY','FBE_NEXT_TEST_FORCE_HIGH_CONTRAST')
$saved = @{}; foreach($name in $names) { $saved[$name] = [Environment]::GetEnvironmentVariable($name,'Process') }
try {
    [void](New-IsolatedFbeSettingsSeed -Isolation $iso)
    $env:FBE_NEXT_TEST_MODE = '1'
    $env:FBE_NEXT_TEST_SCENARIO = 'interface-legacy-platform-runtime'
    $env:FBE_NEXT_TEST_FORCE_HIGH_CONTRAST = $null
    $settingsRoot = Join-Path $iso.DataPath 'Settings'
    $env:FBE_NEXT_TEST_SETTINGS_DIRECTORY = if($Mode -eq 'installed') { $settingsRoot } else { $null }
    $diagnosticsRoot = if($Mode -eq 'installed') { $settingsRoot } else { $iso.DataPath }
    $report = Join-Path $diagnosticsRoot 'Diagnostics/portable-state-report.txt'
    $process = Start-Process $iso.Exe -ArgumentList "--$Mode" -WorkingDirectory $iso.Root -PassThru
    $code = Wait-IsolatedFbeProcess -Process $process -Isolation $iso -Scenario 'interface-legacy-platform-runtime' -Report $report -TimeoutSeconds 90
    $text = if(Test-Path -LiteralPath $report) { Get-Content -Raw -LiteralPath $report } else { '<missing report>' }
    Write-Host $text
    if($code -ne 0 -or $text -notmatch '(?m)^result=pass\r?$') { throw "Legacy platform theme policy failed ($Mode): $text" }
    foreach($field in @('unsupported','no-dark','latte','sepia','custom','actual-pixel','policy')) {
        if($text -notmatch ("(?m)^" + [regex]::Escape($field) + "=1\r?$")) { throw "Missing legacy platform assertion: $field" }
    }
    $passed = $true
    Write-Host "Simulated legacy platform theme policy: PASS ($Mode; not a Windows 7 machine)."
} finally {
    foreach($name in $names) { [Environment]::SetEnvironmentVariable($name, $saved[$name], 'Process') }
    Complete-IsolatedFbeRuntime -Isolation $iso -Passed $passed
}
