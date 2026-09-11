<# Exercises SourceEditorControl through an unattended FBE process. #>
[CmdletBinding()]
param([string]$FbeExe = (Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe'), [int]$TimeoutSeconds = 90)

$ErrorActionPreference = 'Stop'
$FbeExe = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($FbeExe)
if(-not (Test-Path -LiteralPath $FbeExe -PathType Leaf)) { throw "Не найден FBE: $FbeExe" }
$directory = Join-Path ([IO.Path]::GetTempPath()) ('fbe-source-editor-ui-' + [guid]::NewGuid().ToString('N'))
[void](New-Item -ItemType Directory -Path $directory)
try {
    $fixture = Join-Path $directory 'source.fb2'; $report = Join-Path $directory 'source.tsv'
    '<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>source</book-title><lang>en</lang></title-info><document-info><id>source-test</id><version>1.0</version></document-info></description><body><section><p>source</p></section></body></FictionBook>' | Set-Content -LiteralPath $fixture -Encoding utf8
    $oldMode, $oldScenario = $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO
    try {
        $env:FBE_NEXT_TEST_MODE = '1'; $env:FBE_NEXT_TEST_SCENARIO = 'source-editor-ui-runtime'
        $process = Start-Process -FilePath $FbeExe -ArgumentList @('-b', $report, $fixture) -PassThru
        if(-not $process.WaitForExit($TimeoutSeconds * 1000)) { Stop-Process -Id $process.Id -Force; throw 'FBE не завершил SourceEditorControl scenario.' }
        if($process.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $report)) { throw "FBE SourceEditorControl scenario failed: exit $($process.ExitCode)." }
    } finally { $env:FBE_NEXT_TEST_MODE=$oldMode; $env:FBE_NEXT_TEST_SCENARIO=$oldScenario }
    $rows = @{}; foreach($line in Get-Content -LiteralPath $report) { $parts = $line -split "`t"; if($parts.Count -eq 2) { $rows[$parts[0]] = $parts[1] } }
    foreach($key in @('created', 'utf8', 'eol', 'eol_visibility', 'wrapping', 'whitespace', 'line_numbers', 'folding', 'styles', 'tag_state', 'metrics', 'reapply')) { if($rows[$key] -ne '1') { throw "SourceEditorControl runtime check failed: $key (value '$($rows[$key])')." } }
    Write-Host 'FBE SourceEditorControl runtime passed.'
} finally { Remove-Item -LiteralPath $directory -Recurse -Force -ErrorAction SilentlyContinue }
