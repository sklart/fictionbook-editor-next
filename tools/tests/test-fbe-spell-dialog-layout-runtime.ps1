[CmdletBinding()]
param([string]$FbeExe, [int]$TimeoutSeconds = 30)
$ErrorActionPreference = 'Stop'
if (-not $FbeExe) { $FbeExe = Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe' }
$FbeExe = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($FbeExe)
if (-not (Test-Path -LiteralPath $FbeExe)) { throw "FBE not found: $FbeExe" }
$directory = Join-Path ([IO.Path]::GetTempPath()) ('fbe-spell-layout-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $directory | Out-Null
$fixture = Join-Path $directory 'spell.fb2'
$report = Join-Path $directory 'report.txt'
try {
    '<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>Spell layout</book-title><lang>en</lang></title-info><document-info><id>spell-layout-smoke</id><version>1.0</version></document-info></description><body><section><p>Spell layout smoke.</p></section></body></FictionBook>' |
        Set-Content -LiteralPath $fixture -Encoding utf8
    $oldMode, $oldScenario = $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO
    try {
        $env:FBE_NEXT_TEST_MODE = '1'
        $env:FBE_NEXT_TEST_SCENARIO = 'spell-dialog-layout-runtime'
        $process = Start-Process -FilePath $FbeExe -ArgumentList @('-b', ('"' + $report + '"'), ('"' + $fixture + '"')) -WindowStyle Hidden -PassThru
        if (-not $process.WaitForExit($TimeoutSeconds * 1000)) { Stop-Process -Id $process.Id -Force; throw 'Spell layout smoke timed out.' }
        if ($process.ExitCode -ne 0) { throw "Spell layout smoke exited $($process.ExitCode): $(if(Test-Path $report){Get-Content $report -Raw})" }
    }
    finally {
        $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO = $oldMode, $oldScenario
    }
    $text = Get-Content -Raw -LiteralPath $report
    foreach ($expected in @('ru-RU=1', 'en-US=1', 'de-DE=1', 'uk-UA=1', 'result=pass')) {
        if (-not $text.Contains($expected)) { throw "Spell layout smoke missing $expected`: $text" }
    }
    Write-Host 'Spell dialog layout passed for ru/en/de/uk.'
}
finally {
    Remove-Item -LiteralPath $fixture, $report -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $directory -Force -ErrorAction SilentlyContinue
}
