<# Exercises window.external document-path methods in a live FBE MSHTML host. #>
[CmdletBinding()]
param(
    [string]$FbeExe = (Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe'),
    [int]$TimeoutSeconds = 120
)

$ErrorActionPreference = 'Stop'
$FbeExe = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($FbeExe)
if (-not (Test-Path -LiteralPath $FbeExe -PathType Leaf)) { throw "FBE.exe was not found: $FbeExe" }
$root = Join-Path ([IO.Path]::GetTempPath()) ('fbe-api-путь-' + [guid]::NewGuid().ToString('N'))
try {
    New-Item -ItemType Directory -Path $root -Force | Out-Null
    $firstDirectory = Join-Path $root 'путь-первый'; $secondDirectory = Join-Path $root 'путь-второй'; $saveDirectory = Join-Path $root 'путь-сохранение'
    New-Item -ItemType Directory -Path $firstDirectory,$secondDirectory,$saveDirectory -Force | Out-Null
    $first = Join-Path $firstDirectory 'книга-первая.fb2'; $second = Join-Path $secondDirectory 'книга-вторая.fb2'; $saveAs = Join-Path $saveDirectory 'книга-сохранённая.fb2'; $report = Join-Path $root 'report.txt'
    $fixture = '<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>Runtime</book-title><lang>en</lang></title-info><document-info><program-used>test</program-used><id>runtime-document-path</id><version>1.0</version></document-info></description><body><section><p>runtime</p></section></body></FictionBook>'
    [IO.File]::WriteAllText($first, $fixture, [Text.UTF8Encoding]::new($false)); [IO.File]::WriteAllText($second, $fixture, [Text.UTF8Encoding]::new($false))
    $oldMode,$oldScenario,$oldFirst,$oldSecond,$oldSave = $env:FBE_NEXT_TEST_MODE,$env:FBE_NEXT_TEST_SCENARIO,$env:FBE_NEXT_TEST_DOCUMENT_PATH_FIRST,$env:FBE_NEXT_TEST_DOCUMENT_PATH_SECOND,$env:FBE_NEXT_TEST_SAVE_PATH
    try {
        $env:FBE_NEXT_TEST_MODE = '1'; $env:FBE_NEXT_TEST_SCENARIO = 'script-document-path-runtime'; $env:FBE_NEXT_TEST_DOCUMENT_PATH_FIRST = $first; $env:FBE_NEXT_TEST_DOCUMENT_PATH_SECOND = $second; $env:FBE_NEXT_TEST_SAVE_PATH = $saveAs
        $process = Start-Process -FilePath $FbeExe -ArgumentList @('--portable', '-b', $report) -WorkingDirectory (Split-Path $FbeExe) -PassThru
        if (-not $process.WaitForExit($TimeoutSeconds * 1000)) { Stop-Process -Id $process.Id -Force; throw 'FBE timed out during window.external document-path runtime test.' }
        if ($process.ExitCode -ne 0) { $detail = if(Test-Path -LiteralPath $report) { Get-Content -LiteralPath $report -Raw } else { '<report unavailable>' }; throw "FBE document-path runtime test failed: exit $($process.ExitCode). Report: $detail" }
    }
    finally { $env:FBE_NEXT_TEST_MODE,$env:FBE_NEXT_TEST_SCENARIO,$env:FBE_NEXT_TEST_DOCUMENT_PATH_FIRST,$env:FBE_NEXT_TEST_DOCUMENT_PATH_SECOND,$env:FBE_NEXT_TEST_SAVE_PATH = $oldMode,$oldScenario,$oldFirst,$oldSecond,$oldSave }
    $values = @{}; foreach($line in Get-Content -LiteralPath $report) { $pair = $line -split '=',2; if($pair.Count -eq 2) { $values[$pair[0]] = $pair[1] } }
    foreach($key in @('unsaved','opened','save_as','other_opened','unicode')) { if($values[$key] -ne '1') { throw "window.external document-path runtime check failed: $key (value '$($values[$key])')." } }
    if($values.result -ne 'pass') { throw "window.external document-path runtime result failed: $($values.result)" }
    Write-Host 'window.external document-path runtime passed.'
}
finally { Remove-Item -LiteralPath $root -Recurse -Force -ErrorAction SilentlyContinue }
