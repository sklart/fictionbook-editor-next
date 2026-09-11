<# Exercises BODY <-> SOURCE transitions through the real FBE lifecycle. #>
[CmdletBinding()]
param([string]$FbeExe = (Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe'), [int]$TimeoutSeconds = 90)

$ErrorActionPreference = 'Stop'
$FbeExe = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($FbeExe)
if(-not (Test-Path -LiteralPath $FbeExe -PathType Leaf)) { throw "Не найден FBE: $FbeExe" }
$directory = Join-Path ([IO.Path]::GetTempPath()) ('fbe-body-source-transition-' + [guid]::NewGuid().ToString('N'))
[void](New-Item -ItemType Directory -Path $directory)
try {
    $fixture = Join-Path $directory 'transition.fb2'; $report = Join-Path $directory 'transition.txt'
    '<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>transition</book-title><lang>en</lang></title-info><document-info><id>transition-test</id><version>1.0</version></document-info></description><body><section><p>BODY_SOURCE_ORIGINAL</p></section></body></FictionBook>' | Set-Content -LiteralPath $fixture -Encoding utf8
    $oldMode, $oldScenario = $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO
    try {
        $env:FBE_NEXT_TEST_MODE = '1'; $env:FBE_NEXT_TEST_SCENARIO = 'body-source-transition-runtime'
        $process = Start-Process -FilePath $FbeExe -ArgumentList @('-b', $report, $fixture) -PassThru
        if(-not $process.WaitForExit($TimeoutSeconds * 1000)) { Stop-Process -Id $process.Id -Force; throw 'FBE не завершил BODY/SOURCE transition scenario.' }
        if($process.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $report)) { throw "FBE BODY/SOURCE transition scenario failed: exit $($process.ExitCode)." }
    } finally { $env:FBE_NEXT_TEST_MODE=$oldMode; $env:FBE_NEXT_TEST_SCENARIO=$oldScenario }
    $rows = @{}; foreach($line in Get-Content -LiteralPath $report) { $parts = $line -split '='; if($parts.Count -eq 2) { $rows[$parts[0]] = $parts[1] } }
    foreach($key in @('source_active','source_current','body_without_change','valid_edit','edited_source','cycles','invalid_rejected','invalid_preserved','selection_saved')) { if($rows[$key] -ne '1') { throw "BODY/SOURCE transition check failed: $key (value '$($rows[$key])')." } }
    Write-Host 'FBE BODY/SOURCE transition runtime passed.'
} finally { Remove-Item -LiteralPath $directory -Recurse -Force -ErrorAction SilentlyContinue }
