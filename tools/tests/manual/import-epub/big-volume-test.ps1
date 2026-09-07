<# Runs ImportEPUBBatch against an explicitly supplied corpus. #>
[CmdletBinding()]
param([Parameter(Mandatory)][string]$InputDirectory, [string]$BatchExecutable, [string]$OutputDirectory, [int]$MaxFiles = 0, [switch]$Resume)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path
if (-not (Test-Path -LiteralPath $InputDirectory -PathType Container)) { throw "InputDirectory does not exist: $InputDirectory" }
if (-not $BatchExecutable) { $BatchExecutable = Join-Path $repoRoot 'out\Release\ImportEPUBBatch.exe' }
if (-not (Test-Path -LiteralPath $BatchExecutable -PathType Leaf)) { throw "ImportEPUBBatch.exe not found: $BatchExecutable" }
if (-not $OutputDirectory) { $OutputDirectory = Join-Path $repoRoot ('out\manual\import-epub\run-' + (Get-Date -Format 'yyyyMMdd-HHmmss')) }
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$reportPath = Join-Path $OutputDirectory 'ImportEPUBBatch_report.csv'
$arguments = @('--batch', $InputDirectory, $OutputDirectory, '--recursive', '--preserve-tree', '--stats', '--log', '--svg', 'png', '--flush-report-each', '--report', $reportPath)
if ($Resume) { $arguments += '--skip-existing' } else { $arguments += '--overwrite' }
if ($MaxFiles -gt 0) { $arguments += @('--max-files', $MaxFiles) }
& $BatchExecutable @arguments
if ($LASTEXITCODE -ne 0) { throw "ImportEPUBBatch failed: $LASTEXITCODE" }
Write-Host "Report: $reportPath"
