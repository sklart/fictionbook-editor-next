[CmdletBinding()]
param([string]$FbeExe = (Join-Path $PSScriptRoot '../../out/Release/FBE.exe'))
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$iso = New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'description-custom-undo'
$passed = $false
$oldMode, $oldScenario = $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO
try {
    $null = New-IsolatedFbeSettingsSeed $iso
    $fixture = Join-Path $iso.Root 'custom-undo.fb2'
    [IO.File]::WriteAllText($fixture, '<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><nickname>Tester</nickname></author><book-title>Title</book-title><lang>en</lang></title-info><document-info><id>original-id</id><version>1.0</version></document-info></description><body><section><p>Body</p></section></body></FictionBook>', [Text.UTF8Encoding]::new($false))
    $report = Join-Path $iso.Root 'custom-undo-report.txt'
    $env:FBE_NEXT_TEST_MODE = '1'; $env:FBE_NEXT_TEST_SCENARIO = 'description-custom-undo-probe'
    $process = Start-Process -FilePath $iso.Exe -WorkingDirectory $iso.Root -ArgumentList @('--portable','-b',$report,$fixture) -WindowStyle Hidden -PassThru
    $code = Wait-IsolatedFbeProcess -Process $process -Isolation $iso -Scenario $env:FBE_NEXT_TEST_SCENARIO -Report $report -TimeoutSeconds 90
    $text = if (Test-Path -LiteralPath $report) { Get-Content -LiteralPath $report -Raw } else { '<missing report>' }
    Write-Host $text
    $evidence = Join-Path $PSScriptRoot '../../build/evidence-takir-id-undo/custom-unit'
    New-Item -ItemType Directory -Force -Path $evidence | Out-Null
    [IO.File]::WriteAllText((Join-Path $evidence 'report.txt'), $text, [Text.UTF8Encoding]::new($false))
    foreach ($key in @('undo-enabled','undo-restored','redo-enabled','redo-restored','manager-received')) {
        if ($text -notmatch "(?m)^$key=1\r?$") { throw "Custom Description undo probe failed ($key):`n$text" }
    }
    if ($code -ne 0 -or $text -notmatch '(?m)^calls=2\r?$') { throw "Custom Description undo probe failed: $code" }
    $passed = $true
} finally {
    $env:FBE_NEXT_TEST_MODE = $oldMode; $env:FBE_NEXT_TEST_SCENARIO = $oldScenario
    Complete-IsolatedFbeRuntime -Isolation $iso -Passed $passed
}
