<# Records the actual native history behavior for a typed Description edit. #>
[CmdletBinding()]
param(
    [string]$FbeExe = (Join-Path $PSScriptRoot '../../out/Release/FBE.exe'),
    [string]$EvidenceDirectory = (Join-Path $PSScriptRoot '../../build/evidence-takir-id-undo/input-probe'),
    [ValidateSet('portable','installed')][string]$Mode = 'portable',
    [ValidateSet('modern','classic')][string]$DescriptionLayout = 'modern',
    [ValidateRange(30,300)][int]$TimeoutSeconds = 90
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$iso = New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'description-input-undo'
$passed = $false
$names = @('FBE_NEXT_TEST_MODE','FBE_NEXT_TEST_SCENARIO','FBE_NEXT_TEST_SETTINGS_DIRECTORY','FBE_NEXT_TEST_DESCRIPTION_LAYOUT','FBE_NEXT_TEST_DESCRIPTION_MODE')
$saved = @{}; foreach ($name in $names) { $saved[$name] = [Environment]::GetEnvironmentVariable($name, 'Process') }
try {
    $settings = New-IsolatedFbeSettingsSeed $iso
    $settingsXml = [IO.File]::ReadAllText($settings, [Text.Encoding]::UTF8).Replace('</Settings>', "<DescriptionLayout>$DescriptionLayout</DescriptionLayout></Settings>")
    [IO.File]::WriteAllText($settings, $settingsXml, [Text.UTF8Encoding]::new($false))
    $fixture = Join-Path $iso.Root 'description-input.fb2'
    [IO.File]::WriteAllText($fixture, '<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><nickname>Tester</nickname></author><book-title>Original title</book-title><lang>en</lang></title-info><document-info><id>original-id</id><version>1.0</version></document-info></description><body><section><p>Body</p></section></body><binary id="attachment" content-type="application/octet-stream">AQIDBA==</binary></FictionBook>', [Text.UTF8Encoding]::new($false))
    $report = Join-Path $iso.Root 'description-input-report.txt'
    $env:FBE_NEXT_TEST_MODE = '1'; $env:FBE_NEXT_TEST_SCENARIO = 'description-input-undo-probe'; $env:FBE_NEXT_TEST_DESCRIPTION_LAYOUT = $DescriptionLayout; $env:FBE_NEXT_TEST_DESCRIPTION_MODE = $Mode
    $env:FBE_NEXT_TEST_SETTINGS_DIRECTORY = if ($Mode -eq 'installed') { Join-Path $iso.DataPath 'Settings' } else { $null }
    $process = Start-Process -FilePath $iso.Exe -WorkingDirectory $iso.Root -ArgumentList @("--$Mode",'-b',$report,$fixture) -WindowStyle Hidden -PassThru
    $code = Wait-IsolatedFbeProcess -Process $process -Isolation $iso -Scenario $env:FBE_NEXT_TEST_SCENARIO -Report $report -TimeoutSeconds $TimeoutSeconds
    if (!(Test-Path -LiteralPath $report)) { throw 'Description input report missing.' }
    New-Item -ItemType Directory -Force -Path $EvidenceDirectory | Out-Null
    Copy-Item -LiteralPath $report -Destination (Join-Path $EvidenceDirectory "report-$Mode-$DescriptionLayout.txt") -Force
    $text = Get-Content -LiteralPath $report -Raw
    Write-Host $text
    if ($code -ne 0 -or $text -notmatch '(?m)^mode-applied=1\r?$' -or $text -notmatch '(?m)^layout-applied=1\r?$' -or $text -notmatch '(?m)^clean-before-description=1\r?$' -or $text -notmatch '(?m)^input-edited=1\r?$' -or $text -notmatch '(?m)^input-dirty=1\r?$' -or
        $text -notmatch '(?m)^snapshot-before-present=1\r?$' -or $text -notmatch '(?m)^snapshot-changed=1\r?$' -or
        $text -notmatch '(?m)^snapshot-undo-restored=1\r?$' -or $text -notmatch '(?m)^language-changed=1\r?$' -or
        $text -notmatch '(?m)^snapshot-language-changed=1\r?$' -or $text -notmatch '(?m)^saved-clean=1\r?$' -or
        $text -notmatch '(?m)^undo-dirty=0\r?$' -or $text -notmatch '(?m)^saved-undo-restored=1\r?$' -or $text -notmatch '(?m)^saved-undo-dirty=0\r?$' -or
        $text -notmatch '(?m)^initial-content-preserved=1\r?$' -or $text -notmatch '(?m)^independent-field-preserved=1\r?$' -or
        $text -notmatch '(?m)^independent-binary-preserved=1\r?$' -or $text -notmatch '(?m)^independent-body-preserved=1\r?$' -or
        $text -notmatch '(?m)^independent-language-preserved=1\r?$') { throw "Description input probe failed: $code" }
    $passed = $true
} finally {
    foreach ($name in $names) { [Environment]::SetEnvironmentVariable($name, $saved[$name], 'Process') }
    Complete-IsolatedFbeRuntime -Isolation $iso -Passed $passed
}
