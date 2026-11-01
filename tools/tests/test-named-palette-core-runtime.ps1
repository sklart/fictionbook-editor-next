[CmdletBinding()]
param([string]$FbeExe = (Join-Path $PSScriptRoot '../../out/Release/FBE.exe'),
    [ValidateSet('portable','installed')][string]$Mode = 'portable',
    [ValidateSet('ru-RU','en-US')][string]$Locale = 'ru-RU',
    [switch]$CoreOnly, [switch]$WarningOnly, [switch]$ManageOnly)
$ErrorActionPreference = 'Stop'
if(@(@($CoreOnly, $WarningOnly, $ManageOnly) | Where-Object { $_ }).Count -gt 1) { throw 'Choose only one focused mode.' }
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$iso = New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'named-palette-core'
$names = @('FBE_NEXT_TEST_MODE','FBE_NEXT_TEST_SCENARIO','FBE_NEXT_TEST_SETTINGS_DIRECTORY')
$saved = @{}; foreach ($name in $names) { $saved[$name] = [Environment]::GetEnvironmentVariable($name, 'Process') }
$passed = $false
try {
    $seed = New-IsolatedFbeSettingsSeed -Isolation $iso
    $languageId = if($Locale -eq 'ru-RU') { 1049 } else { 1033 }
    $expectedLanguageId = if($Locale -eq 'ru-RU') { 0xFBE002 } else { 0xFBE001 }
    $settingsXml = [IO.File]::ReadAllText($seed).Replace('</Settings>', "<IntefaceLangID>$languageId</IntefaceLangID></Settings>")
    if($WarningOnly) { $settingsXml = $settingsXml.Replace('</Settings>', '<InterfacePalette>custom:restart-test</InterfacePalette></Settings>') }
    [IO.File]::WriteAllText($seed, $settingsXml, [Text.UTF8Encoding]::new($false))
    $env:FBE_NEXT_TEST_MODE = '1'
    $settingsRoot = Join-Path $iso.DataPath 'Settings'
    $env:FBE_NEXT_TEST_SETTINGS_DIRECTORY = if($Mode -eq 'installed') { $settingsRoot } else { $null }
    $diagnosticsRoot = if($Mode -eq 'installed') { $settingsRoot } else { $iso.DataPath }
    $report = Join-Path $diagnosticsRoot 'Diagnostics/portable-state-report.txt'
    $missingRuns = 0
    $scenarios = if($CoreOnly) { @('named-palette-core-runtime') } elseif($WarningOnly) { @('custom-palette-settings-missing','custom-palette-settings-missing') } elseif($ManageOnly) { @('custom-palette-settings-write','custom-palette-manage') } else { @('named-palette-core-runtime','custom-palette-copy','named-palette-settings-write','named-palette-settings-read','popular-palette-write','popular-palette-read','custom-palette-settings-write','custom-palette-manage','custom-palette-edit','custom-palette-settings-read','custom-palette-settings-missing','custom-palette-settings-missing') }
    foreach($scenario in $scenarios) {
        if($scenario -eq 'custom-palette-settings-missing' -and ++$missingRuns -eq 2) {
            $themeDirectory = Join-Path $settingsRoot 'InterfaceThemes'
            New-Item -ItemType Directory -Path $themeDirectory -Force | Out-Null
            [IO.File]::WriteAllText((Join-Path $themeDirectory 'restart-test.json'), '{broken', [Text.UTF8Encoding]::new($false))
        }
        $env:FBE_NEXT_TEST_SCENARIO = $scenario
        Remove-Item -LiteralPath $report -Force -ErrorAction SilentlyContinue
        $windowStyle = if($scenario -in @('popular-palette-write','custom-palette-settings-missing')) { 'Normal' } else { 'Hidden' }
        $process = Start-Process $iso.Exe -ArgumentList "--$Mode" -WorkingDirectory $iso.Root -WindowStyle $windowStyle -PassThru
        $code = Wait-IsolatedFbeProcess -Process $process -Isolation $iso -Scenario $scenario -Report $report -TimeoutSeconds 120
        $text = if(Test-Path -LiteralPath $report) { Get-Content -LiteralPath $report -Raw } else { '<missing report>' }
        if($code -ne 0 -or $text -notmatch '(?m)^result=pass\r?$') { throw "Named palette core failed ($Mode/$scenario): $text" }
        if($scenario -eq 'custom-palette-settings-missing' -and $text -notmatch ("(?m)^interface-language=" + $expectedLanguageId + "\r?$")) { throw "Requested interface language not active ($Mode/$scenario): $text" }
    }
    $passed = $true
    if($WarningOnly) { Write-Host "Missing and invalid palette warnings: PASS ($Mode/$Locale)." }
    elseif($CoreOnly) { Write-Host "Named palette core and system control colours: PASS ($Mode)." }
    elseif($ManageOnly) { Write-Host "Palette management and cancellation: PASS ($Mode/$Locale)." }
    else { Write-Host "Named palette roles, classic restoration, persistence and resource lifetime: PASS ($Mode)." }
} finally {
    foreach ($name in $names) { [Environment]::SetEnvironmentVariable($name, $saved[$name], 'Process') }
    Complete-IsolatedFbeRuntime -Isolation $iso -Passed $passed
}
