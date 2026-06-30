param(
    [Parameter(Mandatory=$true)][string]$ReportPath,
    [Parameter(Mandatory=$true)][string]$OutputPath,
    [int]$LunaSvgPresent = 0
)

$ErrorActionPreference = 'Continue'
$lines = New-Object System.Collections.Generic.List[string]
$errors = 0
$warnings = 0

function Add-Line([string]$text) {
    $lines.Add($text) | Out-Null
    Write-Host $text
}

function Add-Error([string]$text) {
    $script:errors++
    Add-Line ('ERROR: ' + $text)
}

function Add-Warning([string]$text) {
    $script:warnings++
    Add-Line ('WARNING: ' + $text)
}

function To-Int($value) {
    $n = 0
    if ([int]::TryParse([string]$value, [ref]$n)) { return $n }
    return 0
}

function Find-Row([object[]]$rows, [string]$pattern) {
    # PowerShell can unwrap a single-element array returned from a function.
    # Return the array as a single object so .Count and [0] work reliably both
    # in Windows PowerShell 5.1 and PowerShell 7.
    $result = @($rows | Where-Object { $_.Source -like $pattern -or $_.Output -like $pattern } | Select-Object -First 1)
    return ,$result
}

function Find-ConvertedRow([object[]]$rows, [string]$pattern) {
    # In resume mode skipped rows intentionally have empty statistics.
    # Do not use them for semantic regression checks such as Tables/NoteLinks.
    $result = @($rows | Where-Object { ($_.Source -like $pattern -or $_.Output -like $pattern) -and $_.Status -notlike 'SKIPPED*' } | Select-Object -First 1)
    return ,$result
}

Add-Line 'ImportEPUB regression test report'
Add-Line ('Generated: ' + (Get-Date -Format 'yyyy-MM-dd HH:mm:ss'))
Add-Line ('Report:    ' + $ReportPath)
Add-Line ('LunaSVG:   ' + ($(if ($LunaSvgPresent -ne 0) { 'present' } else { 'missing' })))
Add-Line ''

if (-not (Test-Path -LiteralPath $ReportPath)) {
    Add-Error ('CSV report not found: ' + $ReportPath)
    $lines | Set-Content -LiteralPath $OutputPath -Encoding UTF8
    exit 2
}

$rows = @(Import-Csv -LiteralPath $ReportPath -Delimiter ';')
Add-Line ('Rows:      ' + $rows.Count)

$failed = @($rows | Where-Object { $_.Status -like 'FAILED*' })
if ($failed.Count -gt 0) {
    Add-Error ('FAILED rows found: ' + $failed.Count)
    $failed | Select-Object -First 10 | ForEach-Object { Add-Line ('  FAILED: ' + $_.Source + ' -> ' + $_.Message) }
} else {
    Add-Line 'OK: no FAILED rows'
}

$missingImages = @($rows | Where-Object { (To-Int $_.MissingImageBinaries) -gt 0 })
if ($missingImages.Count -gt 0) {
    Add-Error ('Missing image binaries found in ' + $missingImages.Count + ' file(s)')
    $missingImages | Select-Object -First 10 | ForEach-Object { Add-Line ('  MissingImageBinaries=' + $_.MissingImageBinaries + ': ' + $_.Source) }
} else {
    Add-Line 'OK: MissingImageBinaries = 0 for all rows'
}

$friedrichRows = @($rows | Where-Object { ($_.Source -like '*friedrich-engels*working-class*' -or $_.Output -like '*friedrich-engels*working-class*') -and $_.Status -notlike 'SKIPPED*' })
$friedrichAny = Find-Row $rows '*friedrich-engels*working-class*'
if ($friedrichRows.Count -gt 0) {
    $friedrichFailed = @($friedrichRows | Where-Object { $_.Status -like 'FAILED*' } | Select-Object -First 1)
    if ($friedrichFailed.Count -gt 0) {
        Add-Error 'friedrich-engels test: conversion failed'
    } else {
        $maxNoteLinks = 0
        foreach ($row in $friedrichRows) {
            $value = To-Int $row.NoteLinks
            if ($value -gt $maxNoteLinks) { $maxNoteLinks = $value }
        }
        if ($maxNoteLinks -le 0) {
            Add-Error 'friedrich-engels test: NoteLinks must be greater than 0'
        } else {
            Add-Line ('OK: friedrich-engels note links = ' + $maxNoteLinks)
        }
    }
} elseif ($friedrichAny.Count -gt 0) {
    Add-Warning 'friedrich-engels test was skipped in resume report; NoteLinks check was not repeated'
} else {
    Add-Warning 'friedrich-engels test file not found in report'
}

$svgRows = @($rows | Where-Object { $_.Source -like '*svg-in-spine*' -or $_.Source -like '*sous-le-vent_svg-in-spine*' })
if ($svgRows.Count -gt 0) {
    foreach ($r in $svgRows) {
        if ((To-Int $r.Images) -ne (To-Int $r.Binaries)) {
            Add-Error ('SVG test: Images != Binaries for ' + $r.Source + ' (' + $r.Images + '/' + $r.Binaries + ')')
        }
        if ($LunaSvgPresent -ne 0) {
            if ((To-Int $r.SvgImages) -gt 0 -and (To-Int $r.SvgConverted) -le 0) {
                Add-Error ('SVG test: LunaSVG present but SvgConverted=0 for ' + $r.Source)
            }
            if ((To-Int $r.SvgPlaceholders) -gt 0) {
                Add-Error ('SVG test: LunaSVG present but placeholders were used for ' + $r.Source)
            }
        }
    }
    if ($errors -eq 0) { Add-Line ('OK: SVG regression rows checked: ' + $svgRows.Count) }
} else {
    Add-Warning 'SVG test files not found in report'
}

$linear = Find-ConvertedRow $rows '*linear-algebra*'
$linearAny = Find-Row $rows '*linear-algebra*'
if ($linear.Count -gt 0) {
    if ((To-Int $linear[0].Tables) -le 0) { Add-Error 'linear-algebra test: Tables must be greater than 0' }
    else { Add-Line ('OK: linear-algebra tables = ' + $linear[0].Tables) }
} elseif ($linearAny.Count -gt 0) {
    Add-Warning 'linear-algebra test was skipped in resume report; Tables check was not repeated'
} else {
    Add-Warning 'linear-algebra test file not found in report'
}

$duplicateRows = @($rows | Where-Object { (To-Int $_.DuplicateIds) -gt 0 })
if ($duplicateRows.Count -gt 0) {
    Add-Warning ('DuplicateIds still present in ' + $duplicateRows.Count + ' file(s)')
    $duplicateRows | Select-Object -First 10 | ForEach-Object { Add-Line ('  DuplicateIds=' + $_.DuplicateIds + ': ' + $_.Source) }
} else {
    Add-Line 'OK: DuplicateIds = 0 for all rows'
}

Add-Line ''
Add-Line ('Errors:   ' + $errors)
Add-Line ('Warnings: ' + $warnings)

$parent = Split-Path -Parent $OutputPath
if ($parent -and -not (Test-Path -LiteralPath $parent)) { New-Item -ItemType Directory -Force -Path $parent | Out-Null }
$lines | Set-Content -LiteralPath $OutputPath -Encoding UTF8

if ($errors -gt 0) { exit 1 }
exit 0
