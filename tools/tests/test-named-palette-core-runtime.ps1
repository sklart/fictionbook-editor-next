[CmdletBinding()]
param([string]$FbeExe = (Join-Path $PSScriptRoot '../../out/Release/FBE.exe'),
    [ValidateSet('portable','installed')][string]$Mode = 'portable')
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$iso = New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'named-palette-core'
$names = @('FBE_NEXT_TEST_MODE','FBE_NEXT_TEST_SCENARIO','FBE_NEXT_TEST_SETTINGS_DIRECTORY')
$saved = @{}; foreach ($name in $names) { $saved[$name] = [Environment]::GetEnvironmentVariable($name, 'Process') }
$passed = $false
try {
    New-IsolatedFbeSettingsSeed -Isolation $iso | Out-Null
    $env:FBE_NEXT_TEST_MODE = '1'
    $env:FBE_NEXT_TEST_SETTINGS_DIRECTORY = if($Mode -eq 'installed') { $iso.DataPath } else { $null }
    $report = Join-Path $iso.DataPath 'Diagnostics/portable-state-report.txt'
    $missingRuns = 0
    foreach($scenario in @('named-palette-core-runtime','custom-palette-copy','named-palette-settings-write','named-palette-settings-read','popular-palette-write','popular-palette-read','custom-palette-settings-write','custom-palette-manage','custom-palette-edit','custom-palette-settings-read','custom-palette-settings-missing','custom-palette-settings-missing')) {
        if($scenario -eq 'custom-palette-settings-missing' -and ++$missingRuns -eq 2) {
            $settingsRoot = if($Mode -eq 'portable') { Join-Path $iso.DataPath 'Settings' } else { $iso.DataPath }
            $themeDirectory = Join-Path $settingsRoot 'InterfaceThemes'
            New-Item -ItemType Directory -Path $themeDirectory -Force | Out-Null
            [IO.File]::WriteAllText((Join-Path $themeDirectory 'restart-test.json'), '{broken', [Text.UTF8Encoding]::new($false))
        }
        $env:FBE_NEXT_TEST_SCENARIO = $scenario
        Remove-Item -LiteralPath $report -Force -ErrorAction SilentlyContinue
        $windowStyle = if($scenario -eq 'popular-palette-write') { 'Normal' } else { 'Hidden' }
        $process = Start-Process $iso.Exe -ArgumentList "--$Mode" -WorkingDirectory $iso.Root -WindowStyle $windowStyle -PassThru
        $code = Wait-IsolatedFbeProcess -Process $process -Isolation $iso -Scenario $scenario -Report $report -TimeoutSeconds 120
        $text = if(Test-Path -LiteralPath $report) { Get-Content -LiteralPath $report -Raw } else { '<missing report>' }
        if($code -ne 0 -or $text -notmatch '(?m)^result=pass\r?$') { throw "Named palette core failed ($Mode/$scenario): $text" }
    }
    $passed = $true
    Write-Host "Named palette roles, classic restoration, persistence and resource lifetime: PASS ($Mode)."
} finally {
    foreach ($name in $names) { [Environment]::SetEnvironmentVariable($name, $saved[$name], 'Process') }
    Complete-IsolatedFbeRuntime -Isolation $iso -Passed $passed
}
