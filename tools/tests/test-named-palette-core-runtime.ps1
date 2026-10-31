[CmdletBinding()]
param([string]$FbeExe = (Join-Path $PSScriptRoot '../../out/Release/FBE.exe'))
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$iso = New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'named-palette-core'
$names = @('FBE_NEXT_TEST_MODE','FBE_NEXT_TEST_SCENARIO','FBE_NEXT_TEST_SETTINGS_DIRECTORY')
$saved = @{}; foreach ($name in $names) { $saved[$name] = [Environment]::GetEnvironmentVariable($name, 'Process') }
$passed = $false
try {
    New-IsolatedFbeSettingsSeed -Isolation $iso | Out-Null
    $env:FBE_NEXT_TEST_MODE = '1'; $env:FBE_NEXT_TEST_SCENARIO = 'named-palette-core-runtime'
    $env:FBE_NEXT_TEST_SETTINGS_DIRECTORY = $null
    $report = Join-Path $iso.DataPath 'Diagnostics/portable-state-report.txt'
    $process = Start-Process $iso.Exe -ArgumentList '--portable' -WorkingDirectory $iso.Root -WindowStyle Hidden -PassThru
    $code = Wait-IsolatedFbeProcess -Process $process -Isolation $iso -Scenario 'named-palette-core-runtime' -Report $report -TimeoutSeconds 120
    $text = if(Test-Path -LiteralPath $report) { Get-Content -LiteralPath $report -Raw } else { '<missing report>' }
    if($code -ne 0 -or $text -notmatch '(?m)^result=pass\r?$') { throw "Named palette core failed: $text" }
    $passed = $true
    Write-Host 'Named palette roles, classic restoration and resource lifetime: PASS.'
} finally {
    foreach ($name in $names) { [Environment]::SetEnvironmentVariable($name, $saved[$name], 'Process') }
    Complete-IsolatedFbeRuntime -Isolation $iso -Passed $passed
}
