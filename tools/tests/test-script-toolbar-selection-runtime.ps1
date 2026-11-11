[CmdletBinding()]
param([string]$FbeExe = (Join-Path $PSScriptRoot '../../out/Release/FBE.exe'), [int]$TimeoutSeconds = 120)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$iso = New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'toolbar-selection'
$names = @('FBE_NEXT_TEST_MODE','FBE_NEXT_TEST_SCENARIO','FBE_NEXT_TEST_SETTINGS_DIRECTORY')
$old = @{}; foreach($name in $names) { $old[$name] = [Environment]::GetEnvironmentVariable($name, 'Process') }
$passed = $false
try {
    $null = New-IsolatedFbeSettingsSeed $iso
    $env:FBE_NEXT_TEST_MODE='1'; $env:FBE_NEXT_TEST_SCENARIO='scripts-toolbar-customize-ctrl-f-runtime'
    $env:FBE_NEXT_TEST_SETTINGS_DIRECTORY = Join-Path $iso.DataPath 'Settings'
    $report = Join-Path $iso.DataPath 'Diagnostics/portable-state-report.txt'
    $process = Start-Process $iso.Exe -WorkingDirectory $iso.Runtime -ArgumentList '--portable' -WindowStyle Hidden -PassThru
    $code = Wait-IsolatedFbeProcess -Process $process -Isolation $iso -Scenario 'toolbar-selection' -Report $report -TimeoutSeconds $TimeoutSeconds
    if($code -ne 0 -or -not (Test-Path -LiteralPath $report)) { throw 'Toolbar selection runtime did not complete.' }
    $text = Get-Content -LiteralPath $report -Raw
    foreach($key in @('result=pass','catalog-missing-paths=0','catalog-order=1','independent-selection=1','insert-before=1','multi-insert-remove=1','drag-insert=1','drag-insert-mask=63','keyboard-and-double-click-insert=1')) {
        if(-not $text.Contains($key)) { throw "Toolbar selection runtime failed ($key): $text" }
    }
    if($text.Contains('keyboard-move=NOT_RUN')) { Write-Warning 'Ctrl+Up/Down was not exercised: the isolated input queue did not retain the Ctrl state.' }
    $passed = $true; Write-Host $text
} finally {
    foreach($name in $names) { [Environment]::SetEnvironmentVariable($name, $old[$name], 'Process') }
    Complete-IsolatedFbeRuntime -Isolation $iso -Passed $passed
}
