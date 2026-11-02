[CmdletBinding()]
param([string]$FbeExe = (Join-Path $PSScriptRoot '../../out/Release/FBE.exe'),
    [ValidateSet('portable','installed')][string]$Mode = 'portable',
    [ValidateSet('ru-RU','en-US','de-DE','uk-UA','fr-FR','es-ES','it-IT','pl-PL','pt-PT','nl-NL','cs-CZ','bg-BG')][string]$Locale = 'ru-RU',
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
    $languageId = @{ 'ru-RU' = 1049; 'en-US' = 1033; 'de-DE' = 1031; 'uk-UA' = 1058; 'fr-FR' = 1036; 'es-ES' = 3082; 'it-IT' = 1040; 'pl-PL' = 1045; 'pt-PT' = 2070; 'nl-NL' = 1043; 'cs-CZ' = 1029; 'bg-BG' = 1026 }[$Locale]
    $expectedLanguageId = @{ 'ru-RU' = 0xFBE002; 'en-US' = 0xFBE001; 'de-DE' = 0xFBE004; 'uk-UA' = 0xFBE003; 'fr-FR' = 0xFBE005; 'es-ES' = 0xFBE006; 'it-IT' = 0xFBE007; 'pl-PL' = 0xFBE008; 'pt-PT' = 0xFBE009; 'nl-NL' = 0xFBE00A; 'cs-CZ' = 0xFBE00B; 'bg-BG' = 0xFBE00C }[$Locale]
    $settingsXml = [IO.File]::ReadAllText($seed).Replace('</Settings>', "<IntefaceLangID>$languageId</IntefaceLangID></Settings>")
    if($WarningOnly) { $settingsXml = $settingsXml.Replace('</Settings>', '<InterfacePalette>custom:restart-test</InterfacePalette></Settings>') }
    [IO.File]::WriteAllText($seed, $settingsXml, [Text.UTF8Encoding]::new($false))
    $env:FBE_NEXT_TEST_MODE = '1'
    $settingsRoot = Join-Path $iso.DataPath 'Settings'
    $env:FBE_NEXT_TEST_SETTINGS_DIRECTORY = if($Mode -eq 'installed') { $settingsRoot } else { $null }
    $diagnosticsRoot = if($Mode -eq 'installed') { $settingsRoot } else { $iso.DataPath }
    $report = Join-Path $diagnosticsRoot 'Diagnostics/portable-state-report.txt'
    $fixture = Join-Path $iso.Root 'palette-document.fb2'
    [IO.File]::WriteAllText($fixture, '<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>Palette test</book-title><lang>en</lang></title-info><document-info><id>palette-test</id><version>1.0</version></document-info></description><body><section><title><p>Heading</p></title><p>Palette <emphasis>document</emphasis> test.</p></section></body></FictionBook>', [Text.UTF8Encoding]::new($false))
    $fixtureHash = (Get-FileHash -LiteralPath $fixture -Algorithm SHA256).Hash
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
        $arguments = if($scenario -eq 'custom-palette-manage') { @("--$Mode", '-b', ('"' + (Join-Path $iso.Root 'palette-dispatch.txt') + '"'), ('"' + $fixture + '"')) } else { @("--$Mode") }
        $process = Start-Process $iso.Exe -ArgumentList $arguments -WorkingDirectory $iso.Root -WindowStyle $windowStyle -PassThru
        $code = Wait-IsolatedFbeProcess -Process $process -Isolation $iso -Scenario $scenario -Report $report -TimeoutSeconds 120
        $text = if(Test-Path -LiteralPath $report) { Get-Content -LiteralPath $report -Raw } else { '<missing report>' }
        if($code -ne 0 -or $text -notmatch '(?m)^result=pass\r?$') { throw "Named palette core failed ($Mode/$scenario): $text" }
        if($scenario -eq 'custom-palette-manage' -and ($text -notmatch '(?m)^document-preserved=1\r?$' -or $text -notmatch '(?m)^hex-persisted=1\r?$' -or (Get-FileHash -LiteralPath $fixture -Algorithm SHA256).Hash -ne $fixtureHash)) { throw 'Palette management failed to persist HEX or changed the test document.' }
        if($scenario -eq 'custom-palette-manage') {
            if($text -notmatch '(?m)^dpi-failure=0\r?$') { throw "Palette manager DPI simulation failed: $text" }
            if($text -notmatch ("(?m)^interface-language=" + $expectedLanguageId + "\r?$")) { throw "Requested interface language not active ($Mode/$scenario): $text" }
            Write-Host "Palette manager simulated DPI 96/120/144/192, 1366x728 work area and scaled captions: PASS ($Mode/$Locale)."
            foreach($line in ($text -split '\r?\n' | Where-Object { $_ -match '^dpi-(gdi|user)=' })) { Write-Host $line }
        }
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
