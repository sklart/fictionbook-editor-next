[CmdletBinding()]
param(
    [string]$FbeExe = (Join-Path $PSScriptRoot '../../out/Release/FBE.exe'),
    [ValidateSet('portable','installed')][string]$Mode = 'portable',
    [ValidateSet('ru-RU','en-US')][string]$Locale = 'ru-RU'
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$iso = New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'main-toolbar-size-settings'
$names = @('FBE_NEXT_TEST_MODE','FBE_NEXT_TEST_SCENARIO','FBE_NEXT_TEST_SETTINGS_DIRECTORY')
$saved = @{}; foreach ($name in $names) { $saved[$name] = [Environment]::GetEnvironmentVariable($name, 'Process') }
$passed = $false
try {
    $seed = New-IsolatedFbeSettingsSeed -Isolation $iso
    $languageId = if ($Locale -eq 'ru-RU') { 1049 } else { 1033 }
    $expectedLanguageId = if ($Locale -eq 'ru-RU') { 0xFBE002 } else { 0xFBE001 }
    $settingsXml = [IO.File]::ReadAllText($seed).Replace('</Settings>', "<IntefaceLangID>$languageId</IntefaceLangID></Settings>")
    [IO.File]::WriteAllText($seed, $settingsXml, [Text.UTF8Encoding]::new($false))
    $env:FBE_NEXT_TEST_MODE = '1'
    $env:FBE_NEXT_TEST_SETTINGS_DIRECTORY = if ($Mode -eq 'installed') { Join-Path $iso.DataPath 'Settings' } else { $null }
    $diagnosticsRoot = if ($Mode -eq 'installed') { Join-Path $iso.DataPath 'Settings' } else { $iso.DataPath }
    $report = Join-Path $diagnosticsRoot 'Diagnostics/portable-state-report.txt'
    foreach ($scenario in @('main-toolbar-size-setting-write','main-toolbar-size-setting-read','main-toolbar-size-dialog-runtime','main-toolbar-size-dialog-read')) {
        $env:FBE_NEXT_TEST_SCENARIO = $scenario
        Remove-Item -LiteralPath $report -Force -ErrorAction SilentlyContinue
        $process = if ($scenario -eq 'main-toolbar-size-dialog-runtime') {
            Start-Process $iso.Exe -ArgumentList "--$Mode" -WorkingDirectory $iso.Root -PassThru
        } else {
            Start-Process $iso.Exe -ArgumentList "--$Mode" -WorkingDirectory $iso.Root -WindowStyle Hidden -PassThru
        }
        $code = Wait-IsolatedFbeProcess -Process $process -Isolation $iso -Scenario $scenario -Report $report -TimeoutSeconds 120
        $text = if (Test-Path -LiteralPath $report) { Get-Content -LiteralPath $report -Raw } else { '<missing report>' }
        if ($code -ne 0 -or $text -notmatch '(?m)^result=pass\r?$') { throw "Main toolbar size settings failed ($Mode/$scenario): $text" }
        if ($text -notmatch ("(?m)^interface-language=" + $expectedLanguageId + "\r?$")) { throw "Requested interface language not active ($Mode/$scenario): $text" }
    }
    $passed = $true
    Write-Host "Main toolbar size default, normalization, independence and restart: PASS ($Mode/$Locale)."
} finally {
    foreach ($name in $names) { [Environment]::SetEnvironmentVariable($name, $saved[$name], 'Process') }
    Complete-IsolatedFbeRuntime -Isolation $iso -Passed $passed
}
