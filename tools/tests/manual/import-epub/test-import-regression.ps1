<# Checks semantic counters emitted by an ImportEPUBBatch corpus run. #>
[CmdletBinding()]
param([Parameter(Mandatory)][string]$ReportPath, [string]$OutputPath = '', [switch]$RequireLunaSvg)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path
if (-not $OutputPath) { $OutputPath = Join-Path $repoRoot 'out\manual\import-epub\regression-report.txt' }
if (-not (Test-Path -LiteralPath $ReportPath -PathType Leaf)) { throw "CSV report not found: $ReportPath" }
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $OutputPath) | Out-Null
$rows = @(Import-Csv -LiteralPath $ReportPath -Delimiter ';')
$errors = [Collections.Generic.List[string]]::new()
if (@($rows | Where-Object Status -like 'FAILED*').Count) { $errors.Add('The report contains FAILED rows.') }
if (@($rows | Where-Object { [int]$_.MissingImageBinaries -gt 0 }).Count) { $errors.Add('The report contains missing image binaries.') }
if ($RequireLunaSvg -and @($rows | Where-Object { [int]$_.SvgImages -gt 0 -and ([int]$_.SvgConverted -le 0 -or [int]$_.SvgPlaceholders -gt 0) }).Count) { $errors.Add('LunaSVG rows were not converted without placeholders.') }
$summary = @("Rows: $($rows.Count)", "Errors: $($errors.Count)") + $errors
$summary | Set-Content -LiteralPath $OutputPath -Encoding utf8
$summary | ForEach-Object { Write-Host $_ }
if ($errors.Count) { throw ($errors -join ' ') }
