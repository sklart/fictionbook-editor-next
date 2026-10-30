[CmdletBinding()]
param([string]$FbeExe = (Join-Path $PSScriptRoot '../../out/Release/FBE.exe'))
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$iso = New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'generated-id-undo'
$passed = $false
$oldMode, $oldScenario, $oldExpected = $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO, $env:FBE_NEXT_TEST_EXPECTED_ID
try {
    $null = New-IsolatedFbeSettingsSeed $iso
    $fixture = Join-Path $iso.Root 'id.fb2'
    [IO.File]::WriteAllText($fixture, '<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><nickname>Tester</nickname></author><book-title>Title</book-title><lang>en</lang></title-info><document-info><id>original-id</id><version>1.0</version></document-info></description><body><section><p>Body</p></section></body></FictionBook>', [Text.UTF8Encoding]::new($false))
    $report = Join-Path $iso.Root 'id-undo-report.txt'
    $env:FBE_NEXT_TEST_MODE = '1'; $env:FBE_NEXT_TEST_SCENARIO = 'generated-id-runtime'
    $process = Start-Process -FilePath $iso.Exe -WorkingDirectory $iso.Root -ArgumentList @('--portable','-b',$report,$fixture) -WindowStyle Hidden -PassThru
    $code = Wait-IsolatedFbeProcess -Process $process -Isolation $iso -Scenario $env:FBE_NEXT_TEST_SCENARIO -Report $report -TimeoutSeconds 90
    $text = if (Test-Path -LiteralPath $report) { Get-Content -LiteralPath $report -Raw } else { '<missing report>' }
    Write-Host $text
    $evidence = Join-Path $PSScriptRoot '../../build/evidence-takir-id-undo/generated'
    New-Item -ItemType Directory -Force -Path $evidence | Out-Null
    [IO.File]::WriteAllText((Join-Path $evidence 'report.txt'), $text, [Text.UTF8Encoding]::new($false))
    foreach ($key in @('initially-clean','canceled-clean','generated','dirty','undo-after','undo-restored','undo-source-current','redo-enabled','redo-restored','second-generated','second-undone','second-redone','first-retained','source-current','saved','clean-after-save','persisted')) {
        if ($text -notmatch "(?m)^$key=1\r?$") { throw "Generated ID undo failed ($key):`n$text" }
    }
    if ($code -ne 0) { throw "Generated ID undo runtime failed: $code" }
    $xml = [xml]([IO.File]::ReadAllText($fixture, [Text.Encoding]::UTF8))
    $savedId = $xml.SelectSingleNode("/*[local-name()='FictionBook']/*[local-name()='description']/*[local-name()='document-info']/*[local-name()='id']").InnerText
    if (!$savedId -or $savedId -eq 'original-id') { throw 'Saved generated ID is missing.' }
    $env:FBE_NEXT_TEST_EXPECTED_ID = $savedId; $env:FBE_NEXT_TEST_SCENARIO = 'generated-id-reopen-runtime'
    $process = Start-Process -FilePath $iso.Exe -WorkingDirectory $iso.Root -ArgumentList @('--portable','-b',$report,$fixture) -WindowStyle Hidden -PassThru
    $code = Wait-IsolatedFbeProcess -Process $process -Isolation $iso -Scenario $env:FBE_NEXT_TEST_SCENARIO -Report $report -TimeoutSeconds 90
    $reopened = if (Test-Path -LiteralPath $report) { Get-Content -LiteralPath $report -Raw } else { '<missing report>' }
    [IO.File]::WriteAllText((Join-Path $evidence 'reopen.txt'), $reopened, [Text.UTF8Encoding]::new($false))
    if ($code -ne 0 -or $reopened -notmatch '(?m)^description-current=1\r?$' -or $reopened -notmatch '(?m)^source-current=1\r?$') { throw "Generated ID reopen failed: $code`n$reopened" }
    $passed = $true
} finally {
    $env:FBE_NEXT_TEST_MODE = $oldMode; $env:FBE_NEXT_TEST_SCENARIO = $oldScenario; $env:FBE_NEXT_TEST_EXPECTED_ID = $oldExpected
    Complete-IsolatedFbeRuntime -Isolation $iso -Passed $passed
}
