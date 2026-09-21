<# Runs the real Release editor against BODY and SOURCE fixtures and verifies
   that 1000 unchanged idle iterations do not re-query UI state. #>
[CmdletBinding()]
param(
    [string]$FbeExe = (Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe'),
    [int]$SmallParagraphCount = 1000,
    [int]$MediumParagraphCount = 10000,
    [int]$TimeoutSeconds = 180
)

$ErrorActionPreference = 'Stop'
$FbeExe = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($FbeExe)
if (-not (Test-Path -LiteralPath $FbeExe -PathType Leaf)) { throw "FBE not found: $FbeExe" }

function New-Fixture([string]$Path, [int]$ParagraphCount) {
    $paragraphs = 1..$ParagraphCount | ForEach-Object { "<p>idle performance paragraph $_</p>" }
    @('<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>idle-performance</book-title><lang>en</lang></title-info><document-info><program-used>test</program-used><id>idle-performance</id><version>1.0</version></document-info></description><body><section>', ($paragraphs -join ''), '</section></body></FictionBook>') | Set-Content -LiteralPath $Path -Encoding utf8
}

function Read-Report([string]$Path) {
    $result = @{}
    Get-Content -LiteralPath $Path | ForEach-Object {
        $parts = $_ -split "`t", 2
        if ($parts.Count -eq 2) {
            $result[$parts[0]] = if ($parts[0] -eq 'view') { $parts[1] } else { [UInt64]$parts[1] }
        }
    }
    return $result
}

$directory = Join-Path ([IO.Path]::GetTempPath()) ('fbe-idle-performance-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $directory | Out-Null
$oldMode, $oldScenario, $oldTrace, $oldIdleView = $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO, $env:FBE_NEXT_TRACE, $env:FBE_NEXT_TEST_IDLE_VIEW
try {
    $env:FBE_NEXT_TEST_MODE = '1'; $env:FBE_NEXT_TEST_SCENARIO = 'idle-performance'; $env:FBE_NEXT_TRACE = '1'
    foreach ($case in @(
            @{ Name = 'small-body'; Paragraphs = $SmallParagraphCount; View = 'body' },
            @{ Name = 'medium-body'; Paragraphs = $MediumParagraphCount; View = 'body' },
            @{ Name = 'medium-source'; Paragraphs = $MediumParagraphCount; View = 'source' })) {
        $fixture = Join-Path $directory ($case.Name + '.fb2')
        $reportPath = Join-Path $directory ($case.Name + '.tsv')
        New-Fixture $fixture $case.Paragraphs
        $env:FBE_NEXT_TEST_IDLE_VIEW = $case.View
        $process = Start-Process -FilePath $FbeExe -ArgumentList @('-b', $reportPath, $fixture) -PassThru
        if (-not $process.WaitForExit($TimeoutSeconds * 1000)) { Stop-Process -Id $process.Id -Force; throw "$($case.Name): FBE did not complete the idle scenario." }
        if ($process.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $reportPath)) { throw "$($case.Name): FBE idle scenario failed: exit $($process.ExitCode)." }
        $result = Read-Report $reportPath
        if ($result.idle_cycles -ne 1000) { throw "$($case.Name): expected 1000 idle cycles, got $($result.idle_cycles)." }
        foreach ($metric in 'command_state_updates', 'selection_context_builds', 'toolbar_updates', 'clipboard_checks', 'check_command_calls', 'selection_container_queries', 'selection_struct_con_queries', 'selection_struct_table_con_queries', 'js_com_calls') {
            if ($result[$metric] -ne 0) { throw "$($case.Name): unchanged idle performed $metric=$($result[$metric])." }
        }
        if ($result.file_fingerprint_checks -gt 1) { throw "$($case.Name): throttle allowed $($result.file_fingerprint_checks) file checks in 1000 immediate idle iterations." }
        if ($result.view -ne $case.View) { throw "$($case.Name): expected view $($case.View), got $($result.view)." }
        Write-Host "Idle performance runtime test passed: $($case.Name), paragraphs=$($case.Paragraphs), elapsed_ms=$($result.elapsed_ms)."
    }
} finally {
    $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO, $env:FBE_NEXT_TRACE = $oldMode, $oldScenario, $oldTrace
    $env:FBE_NEXT_TEST_IDLE_VIEW = $oldIdleView
    Remove-Item -LiteralPath $directory -Recurse -Force -ErrorAction SilentlyContinue
}
