<# Records the actual native history behavior for a typed Description edit. #>
[CmdletBinding()]
param(
    [string]$FbeExe = (Join-Path $PSScriptRoot '../../out/Release/FBE.exe'),
    [string]$EvidenceDirectory = (Join-Path $PSScriptRoot '../../build/evidence-takir-id-undo/input-probe'),
    [ValidateRange(30,300)][int]$TimeoutSeconds = 90
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$iso = New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'description-input-undo'
$passed = $false
$oldMode, $oldScenario = $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO
try {
    $null = New-IsolatedFbeSettingsSeed $iso
    $fixture = Join-Path $iso.Root 'description-input.fb2'
    [IO.File]::WriteAllText($fixture, '<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><nickname>Tester</nickname></author><book-title>Original title</book-title><lang>en</lang></title-info><document-info><id>original-id</id><version>1.0</version></document-info></description><body><section><p>Body</p></section></body></FictionBook>', [Text.UTF8Encoding]::new($false))
    $report = Join-Path $iso.Root 'description-input-report.txt'
    $env:FBE_NEXT_TEST_MODE = '1'; $env:FBE_NEXT_TEST_SCENARIO = 'description-input-undo-probe'
    $process = Start-Process -FilePath $iso.Exe -WorkingDirectory $iso.Root -ArgumentList @('--portable','-b',$report,$fixture) -WindowStyle Hidden -PassThru
    $code = Wait-IsolatedFbeProcess -Process $process -Isolation $iso -Scenario $env:FBE_NEXT_TEST_SCENARIO -Report $report -TimeoutSeconds $TimeoutSeconds
    if (!(Test-Path -LiteralPath $report)) { throw 'Description input report missing.' }
    New-Item -ItemType Directory -Force -Path $EvidenceDirectory | Out-Null
    Copy-Item -LiteralPath $report -Destination (Join-Path $EvidenceDirectory 'report.txt') -Force
    $text = Get-Content -LiteralPath $report -Raw
    Write-Host $text
    if ($code -ne 0 -or $text -notmatch '(?m)^input-edited=1\r?$' -or $text -notmatch '(?m)^input-dirty=1\r?$' -or
        $text -notmatch '(?m)^snapshot-before-present=1\r?$' -or $text -notmatch '(?m)^snapshot-changed=1\r?$' -or
        $text -notmatch '(?m)^snapshot-undo-restored=1\r?$' -or $text -notmatch '(?m)^language-changed=1\r?$' -or
        $text -notmatch '(?m)^snapshot-language-changed=1\r?$') { throw "Description input probe failed: $code" }
    $passed = $true
} finally {
    $env:FBE_NEXT_TEST_MODE = $oldMode; $env:FBE_NEXT_TEST_SCENARIO = $oldScenario
    Complete-IsolatedFbeRuntime -Isolation $iso -Passed $passed
}
