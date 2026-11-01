[CmdletBinding()]
param(
    [string]$FbeExe = (Join-Path $PSScriptRoot '../../out/Release/FBE.exe'),
    [ValidateSet('portable','installed')][string]$Mode = 'portable',
    [ValidateSet('ru-RU','en-US')][string]$Locale = 'ru-RU'
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$iso = New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'theme-policy'
$passed = $false
$names = @('FBE_NEXT_TEST_MODE','FBE_NEXT_TEST_SCENARIO','FBE_NEXT_TEST_SETTINGS_DIRECTORY')
$saved = @{}; foreach($name in $names) { $saved[$name] = [Environment]::GetEnvironmentVariable($name,'Process') }
try {
    $seed = New-IsolatedFbeSettingsSeed -Isolation $iso
    $languageId = if($Locale -eq 'ru-RU') { 1049 } else { 1033 }
    $expectedLanguageId = if($Locale -eq 'ru-RU') { 0xFBE002 } else { 0xFBE001 }
    $settingsXml = [IO.File]::ReadAllText($seed).Replace('</Settings>', "<IntefaceLangID>$languageId</IntefaceLangID></Settings>")
    [IO.File]::WriteAllText($seed, $settingsXml, [Text.UTF8Encoding]::new($false))
    $env:FBE_NEXT_TEST_MODE = '1'
    $settingsRoot = Join-Path $iso.DataPath 'Settings'
    $env:FBE_NEXT_TEST_SETTINGS_DIRECTORY = if($Mode -eq 'installed') { $settingsRoot } else { $null }
    $diagnosticsRoot = if($Mode -eq 'installed') { $settingsRoot } else { $iso.DataPath }
    $report = Join-Path $diagnosticsRoot 'Diagnostics/portable-state-report.txt'
    foreach($scenario in @('interface-theme-policy-runtime','interface-theme-policy-read')) {
        $env:FBE_NEXT_TEST_SCENARIO = $scenario
        Set-IsolatedRuntimeStage $iso "$scenario/$Mode"
        Remove-Item -LiteralPath $report -Force -ErrorAction SilentlyContinue
        $windowStyle = if($scenario -eq 'interface-theme-policy-runtime') { 'Normal' } else { 'Hidden' }
        $process = Start-Process $iso.Exe -ArgumentList "--$Mode" -WorkingDirectory $iso.Root -WindowStyle $windowStyle -PassThru
        $exitCode = Wait-IsolatedFbeProcess -Process $process -Isolation $iso -Scenario $scenario -Report $report -TimeoutSeconds 90
        $text = if(Test-Path -LiteralPath $report) { Get-Content -LiteralPath $report -Raw } else { '<missing report>' }
        Write-Host $text
        if($exitCode -ne 0 -or $text -notmatch '(?m)^result=pass\r?$') { throw "Theme policy runtime failed ($Mode/$scenario), exit=$exitCode" }
        if($text -notmatch ("(?m)^interface-language=" + $expectedLanguageId + "\r?$")) { throw "Requested interface language not active ($Mode/$scenario): $text" }
    }
    Write-Host "Theme policy runtime: PASS ($Mode/$Locale; controlled platform availability, not a Windows 7 run)."
    $passed = $true
} finally {
    foreach($name in $names) { [Environment]::SetEnvironmentVariable($name,$saved[$name],'Process') }
    Complete-IsolatedFbeRuntime -Isolation $iso -Passed $passed
}
