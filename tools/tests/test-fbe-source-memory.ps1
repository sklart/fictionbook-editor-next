<#
.SYNOPSIS
Measures real full-process FBE Source memory for representative FB2 fixtures.

.DESCRIPTION
Each fixture is opened twice: normal Undo selection history and disabled (-u).
FBE writes process-memory checkpoints itself, so the measurements include the
DOM, UI, Source serialization, Scintilla styling and matched-tag traversal.
#>
[CmdletBinding()]
param(
    [ValidateSet(1, 5, 20, 50, 100)]
    [int[]]$SizesMiB = @(1, 5, 20, 50),

    [string]$FbeExe = (Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe'),

    [string]$OutputDirectory = (Join-Path $PSScriptRoot '..\..\out\tests\fbe-source-memory'),

    [ValidateRange(30, 1800)]
    [int]$TimeoutSeconds = 600,

    [switch]$RunViewCycles
)

$ErrorActionPreference = 'Stop'
$fixtureGenerator = Join-Path $PSScriptRoot 'new-fb2-memory-fixture.ps1'
$FbeExe = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($FbeExe)
$OutputDirectory = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputDirectory)

if (-not (Test-Path -LiteralPath $FbeExe -PathType Leaf)) {
    throw "FBE executable was not found: $FbeExe"
}
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null

$summary = [System.Collections.Generic.List[object]]::new()
foreach ($sizeMiB in $SizesMiB) {
    $fixture = Join-Path $OutputDirectory ("fixture-{0}MiB.fb2" -f $sizeMiB)
    & $fixtureGenerator -SizeMiB $sizeMiB -OutputPath $fixture

    foreach ($undoSelectionHistory in @('on', 'off')) {
        $report = Join-Path $OutputDirectory ("fbe-{0}MiB-undo-{1}.tsv" -f $sizeMiB, $undoSelectionHistory)
        Remove-Item -LiteralPath $report -Force -ErrorAction SilentlyContinue
        $arguments = @('-b', $report, $fixture)
		if ($RunViewCycles) {
			$arguments = @('-c') + $arguments
		}
        if ($undoSelectionHistory -eq 'off') {
            $arguments = @('-u') + $arguments
        }

        $process = Start-Process -FilePath $FbeExe -ArgumentList $arguments -PassThru
        if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
            Stop-Process -Id $process.Id -Force
            throw "FBE timed out for $sizeMiB MiB (Undo selection history $undoSelectionHistory)."
        }
        if (-not (Test-Path -LiteralPath $report -PathType Leaf)) {
            throw "FBE did not write its report for $sizeMiB MiB (Undo selection history $undoSelectionHistory, exit $($process.ExitCode))."
        }

        foreach ($row in (Import-Csv -LiteralPath $report -Delimiter "`t")) {
            $summary.Add([pscustomobject]@{
                SizeMiB = $sizeMiB
                UndoSelectionHistory = $undoSelectionHistory
                Phase = $row.phase
                ElapsedMs = [Int64]$row.elapsed_ms
                PrivateMiB = [Math]::Round(([Int64]$row.private_bytes) / 1MB, 2)
                WorkingSetMiB = [Math]::Round(([Int64]$row.working_set_bytes) / 1MB, 2)
                CommittedMiB = [Math]::Round(([Int64]$row.committed_bytes) / 1MB, 2)
                ReservedMiB = [Math]::Round(([Int64]$row.reserved_bytes) / 1MB, 2)
                SourceMiB = [Math]::Round(([Int64]$row.source_bytes) / 1MB, 2)
                SourceLines = [Int64]$row.source_lines
            })
        }
    }
}

$summaryPath = Join-Path $OutputDirectory 'summary.tsv'
$summary | Export-Csv -LiteralPath $summaryPath -Delimiter "`t" -NoTypeInformation -Encoding utf8
$summary | Format-Table -AutoSize
Write-Host "Full-process FBE memory benchmark completed: $summaryPath"
