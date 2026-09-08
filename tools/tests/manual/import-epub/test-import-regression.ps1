<# Validates semantic counters emitted by an ImportEPUBBatch corpus run. #>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$ReportPath,
    [string]$OutputPath = '',
    [switch]$RequireLunaSvg
)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path
if (-not $OutputPath) { $OutputPath = Join-Path $repoRoot 'out\manual\import-epub\regression-report.txt' }
if (-not (Test-Path -LiteralPath $ReportPath -PathType Leaf)) { throw "CSV report not found: $ReportPath" }
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $OutputPath) | Out-Null

$lines = [Collections.Generic.List[string]]::new()
$errors = [Collections.Generic.List[string]]::new()
function Add-Line([string]$Text) { $lines.Add($Text); Write-Host $Text }
function Add-Error([string]$Text) { $errors.Add($Text); Add-Line "ERROR: $Text" }
function Add-Warning([string]$Text) { Add-Line "WARNING: $Text" }
function To-Int($Value) { $result = 0; if ([int]::TryParse([string]$Value, [ref]$result)) { return $result }; return 0 }
function Find-Rows([object[]]$Rows, [string]$Pattern, [switch]$ConvertedOnly) {
    return @($Rows | Where-Object { ($_.Source -like $Pattern -or $_.Output -like $Pattern) -and (-not $ConvertedOnly -or $_.Status -notlike 'SKIPPED*') })
}

$rows = @(Import-Csv -LiteralPath $ReportPath -Delimiter ';')
Add-Line 'ImportEPUB regression test report'
Add-Line "Report: $ReportPath"
Add-Line "Rows: $($rows.Count)"

$failed = @($rows | Where-Object Status -like 'FAILED*')
if ($failed.Count) { Add-Error "FAILED rows found: $($failed.Count)"; $failed | Select-Object -First 10 | ForEach-Object { Add-Line "  FAILED: $($_.Source) -> $($_.Message)" } } else { Add-Line 'OK: no FAILED rows' }

$missingImages = @($rows | Where-Object { (To-Int $_.MissingImageBinaries) -gt 0 })
if ($missingImages.Count) { Add-Error "MissingImageBinaries is non-zero in $($missingImages.Count) row(s)" } else { Add-Line 'OK: MissingImageBinaries = 0' }

$noteRows = Find-Rows $rows '*friedrich-engels*working-class*' -ConvertedOnly
if ($noteRows.Count) {
    $maxNoteLinks = @($noteRows | ForEach-Object { To-Int $_.NoteLinks } | Measure-Object -Maximum).Maximum
    if ($maxNoteLinks -le 0) { Add-Error 'friedrich-engels: NoteLinks must be greater than 0' } else { Add-Line "OK: friedrich-engels NoteLinks = $maxNoteLinks" }
} elseif ((Find-Rows $rows '*friedrich-engels*working-class*').Count) { Add-Warning 'friedrich-engels was skipped; NoteLinks was not rechecked' } else { Add-Warning 'friedrich-engels row was not present' }

$tableRows = Find-Rows $rows '*linear-algebra*' -ConvertedOnly
if ($tableRows.Count) {
    $maxTables = @($tableRows | ForEach-Object { To-Int $_.Tables } | Measure-Object -Maximum).Maximum
    if ($maxTables -le 0) { Add-Error 'linear-algebra: Tables must be greater than 0' } else { Add-Line "OK: linear-algebra Tables = $maxTables" }
} elseif ((Find-Rows $rows '*linear-algebra*').Count) { Add-Warning 'linear-algebra was skipped; Tables was not rechecked' } else { Add-Warning 'linear-algebra row was not present' }

$duplicateRows = @($rows | Where-Object { (To-Int $_.DuplicateIds) -gt 0 })
if ($duplicateRows.Count) { Add-Warning "DuplicateIds is non-zero in $($duplicateRows.Count) row(s)" } else { Add-Line 'OK: DuplicateIds = 0' }

$svgRows = @($rows | Where-Object { $_.Source -like '*svg-in-spine*' -or $_.Output -like '*svg-in-spine*' })
if ($svgRows.Count) {
    foreach ($row in $svgRows) {
        if ((To-Int $row.Images) -ne (To-Int $row.Binaries)) { Add-Error "SVG: Images != Binaries for $($row.Source)" }
        if ($RequireLunaSvg -and (To-Int $row.SvgImages) -gt 0) {
            if ((To-Int $row.SvgConverted) -le 0) { Add-Error "SVG: LunaSVG did not convert $($row.Source)" }
            if ((To-Int $row.SvgPlaceholders) -gt 0) { Add-Error "SVG: LunaSVG used a placeholder for $($row.Source)" }
        }
    }
    Add-Line "OK: SVG rows checked: $($svgRows.Count)"
} else { Add-Warning 'SVG rows were not present' }

Add-Line "Errors: $($errors.Count)"
$lines | Set-Content -LiteralPath $OutputPath -Encoding utf8
if ($errors.Count) { throw ($errors -join '; ') }
