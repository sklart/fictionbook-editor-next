[CmdletBinding()]
param([string]$FbeExe, [int]$TimeoutSeconds = 30)
$ErrorActionPreference = 'Stop'
if (-not $FbeExe) { $FbeExe = Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe' }
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$isolation = New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'spell-layout'
$passed = $false
$fixture = Join-Path $isolation.Root 'spell.fb2'
$report = Join-Path $isolation.Root 'report.txt'
try {
    '<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>Spell layout</book-title><lang>en</lang></title-info><document-info><id>spell-layout-smoke</id><version>1.0</version></document-info></description><body><section><p>Spell layout smoke.</p></section></body></FictionBook>' |
        Set-Content -LiteralPath $fixture -Encoding utf8
    $oldMode, $oldScenario = $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO
    try {
        $env:FBE_NEXT_TEST_MODE = '1'
        $env:FBE_NEXT_TEST_SCENARIO = 'spell-dialog-layout-runtime'
        $process = Start-Process -FilePath $isolation.Exe -WorkingDirectory $isolation.Runtime -ArgumentList @('--portable', '-b', ('"' + $report + '"'), ('"' + $fixture + '"')) -WindowStyle Hidden -PassThru
        $exitCode = Wait-IsolatedFbeProcess -Process $process -Isolation $isolation -Scenario 'spell-dialog-layout-runtime' -Report $report -TimeoutSeconds $TimeoutSeconds
        if ($exitCode -ne 0) { throw "Spell layout smoke exited $exitCode`: $(if(Test-Path $report){Get-Content $report -Raw})" }
    }
    finally {
        $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO = $oldMode, $oldScenario
    }
    $text = Get-Content -Raw -LiteralPath $report
    foreach ($expected in @('ru-RU=1', 'en-US=1', 'de-DE=1', 'uk-UA=1', 'result=pass')) {
        if (-not $text.Contains($expected)) { throw "Spell layout smoke missing $expected`: $text" }
    }
    Write-Host 'Spell dialog layout passed for ru/en/de/uk.'
    $passed = $true
}
finally {
    Complete-IsolatedFbeRuntime -Isolation $isolation -Passed $passed
}
