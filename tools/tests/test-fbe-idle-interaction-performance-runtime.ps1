<#
.SYNOPSIS
Exercises real BODY caret, selection and edit notifications on a medium FB2,
then proves that the following idle streak repeats none of their UI work.
#>
[CmdletBinding()]
param(
    [string]$FbeExe = (Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe'),
    [int]$ParagraphCount = 10000,
    [int]$TimeoutSeconds = 300
)

$ErrorActionPreference = 'Stop'
$FbeExe = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($FbeExe)
if (-not (Test-Path -LiteralPath $FbeExe -PathType Leaf)) { throw "FBE not found: $FbeExe" }

function New-Fixture([string]$Path, [int]$Count) {
    $paragraphs = 1..$Count | ForEach-Object { "<p>interaction performance paragraph $_</p>" }
    @('<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>idle interaction performance</book-title><lang>en</lang></title-info><document-info><program-used>test</program-used><id>idle-interaction-performance</id><version>1.0</version></document-info></description><body><section>', ($paragraphs -join ''), '</section></body></FictionBook>') | Set-Content -LiteralPath $Path -Encoding utf8
}

function Read-Report([string]$Path) {
    $result = @{}
    Get-Content -LiteralPath $Path | ForEach-Object {
        $parts = $_ -split "`t", 2
        if ($parts.Count -eq 2) { $result[$parts[0]] = [UInt64]$parts[1] }
    }
    return $result
}

$directory = Join-Path ([IO.Path]::GetTempPath()) ('fbe-idle-interaction-performance-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $directory | Out-Null
$oldMode, $oldScenario, $oldTrace = $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO, $env:FBE_NEXT_TRACE
try {
    $fixture = Join-Path $directory 'medium.fb2'
    $reportPath = Join-Path $directory 'report.tsv'
    New-Fixture $fixture $ParagraphCount
    $env:FBE_NEXT_TEST_MODE = '1'; $env:FBE_NEXT_TEST_SCENARIO = 'idle-interaction-performance'; $env:FBE_NEXT_TRACE = '1'
    $process = Start-Process -FilePath $FbeExe -ArgumentList @('-b', $reportPath, $fixture) -PassThru
    if (-not $process.WaitForExit($TimeoutSeconds * 1000)) { Stop-Process -Id $process.Id -Force; throw 'FBE did not complete the idle interaction scenario.' }
    if ($process.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $reportPath)) { throw "FBE idle interaction scenario failed: exit $($process.ExitCode)." }
    $result = Read-Report $reportPath
    foreach ($expected in @{ caret_moves = 1000; selection_changes = 1000; typing_edits = 10; interaction_idle_cycles = 2010; stable_idle_cycles = 1000 }.GetEnumerator()) {
        if ($result[$expected.Key] -ne $expected.Value) { throw "Expected $($expected.Key)=$($expected.Value), got $($result[$expected.Key])." }
    }
    if ($result.command_state_updates -lt 2000 -or $result.command_state_updates -gt 2010) { throw "Each interaction must cause one bounded command-state update; got $($result.command_state_updates)." }
    if ($result.selection_context_builds -lt 2000 -or $result.selection_context_builds -gt 2010) { throw "Each interaction must cause one bounded selection-context build; got $($result.selection_context_builds)." }
    foreach ($metric in 'stable_command_state_updates', 'stable_selection_context_builds', 'stable_toolbar_updates', 'stable_check_command_calls', 'stable_js_com_calls') {
        if ($result[$metric] -ne 0) { throw "Unchanged idle repeated $metric=$($result[$metric])." }
    }
    Write-Host "Idle interaction performance runtime test passed: paragraphs=$ParagraphCount, interaction_elapsed_ms=$($result.interaction_elapsed_ms), command_updates=$($result.command_state_updates)."
} finally {
    $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO, $env:FBE_NEXT_TRACE = $oldMode, $oldScenario, $oldTrace
    Remove-Item -LiteralPath $directory -Recurse -Force -ErrorAction SilentlyContinue
}
