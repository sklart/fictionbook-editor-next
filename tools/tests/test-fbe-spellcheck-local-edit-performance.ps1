<# Exercises the real body edit handler at the final paragraph of a long section. #>
[CmdletBinding()]
param([string]$FbeExe = (Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe'), [int]$ParagraphCount = 2000, [int]$TimeoutSeconds = 90)

$ErrorActionPreference = 'Stop'
$FbeExe = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($FbeExe)
if (-not (Test-Path -LiteralPath $FbeExe -PathType Leaf)) { throw "FBE not found: $FbeExe" }
$directory = Join-Path ([IO.Path]::GetTempPath()) ('fbe-spell-local-edit-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $directory | Out-Null
try {
    $fixture = Join-Path $directory 'long-section.fb2'; $report = Join-Path $directory 'spell.tsv'
    $paragraphs = 1..$ParagraphCount | ForEach-Object { "<p>ordinary paragraph $_</p>" }
    @('<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>spell</book-title><lang>en</lang></title-info><document-info><program-used>test</program-used><id>spell-local-edit</id><version>1.0</version></document-info></description><body><section>', ($paragraphs -join ''), '</section></body></FictionBook>') | Set-Content -LiteralPath $fixture -Encoding utf8
    $oldMode, $oldScenario = $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO
    try {
        $env:FBE_NEXT_TEST_MODE = '1'; $env:FBE_NEXT_TEST_SCENARIO = 'spellcheck-local-edit'
        $process = Start-Process -FilePath $FbeExe -ArgumentList @('-b', $report, $fixture) -PassThru
        if (-not $process.WaitForExit($TimeoutSeconds * 1000)) { Stop-Process -Id $process.Id -Force; throw 'FBE did not complete the local spellcheck scenario.' }
        if ($process.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $report)) { throw "FBE local spellcheck scenario failed: exit $($process.ExitCode)." }
    } finally { $env:FBE_NEXT_TEST_MODE=$oldMode; $env:FBE_NEXT_TEST_SCENARIO=$oldScenario }
    $result = @{}; Get-Content -LiteralPath $report | ForEach-Object { $parts = $_ -split "`t", 2; if ($parts.Count -eq 2) { $result[$parts[0]] = [int]$parts[1] } }
    if ($result.paragraph_count -lt $ParagraphCount) { throw "Fixture unexpectedly contains only $($result.paragraph_count) paragraphs." }
    if ($result.checked_paragraphs -ne 1) { throw "A local edit checked $($result.checked_paragraphs) paragraphs instead of exactly one." }
    Write-Host 'Production local spellcheck bounded-work regression passed.'
} finally { Remove-Item -LiteralPath $directory -Recurse -Force -ErrorAction SilentlyContinue }
