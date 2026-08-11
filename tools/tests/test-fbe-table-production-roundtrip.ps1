<#
.SYNOPSIS
Exercises the real FBE FB2 -> visual DOM -> Source -> visual DOM pipeline.
#>
[CmdletBinding()]
param(
    [string]$FbeExe = (Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe'),
    [int]$TimeoutSeconds = 180
)

$ErrorActionPreference = 'Stop'
$FbeExe = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($FbeExe)
if (-not (Test-Path -LiteralPath $FbeExe -PathType Leaf)) { throw "Не найден FBE: $FbeExe" }

$directory = Join-Path ([IO.Path]::GetTempPath()) ('fbe-table-roundtrip-' + [guid]::NewGuid().ToString('N'))
[void](New-Item -ItemType Directory -Path $directory)
$fixture = Join-Path $directory 'table.fb2'
$report = Join-Path $directory 'report.tsv'
try {
    @'
<?xml version="1.0" encoding="utf-8"?>
<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0" xmlns:l="http://www.w3.org/1999/xlink">
 <description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>table</book-title><lang>en</lang></title-info><document-info><program-used>test</program-used><id>table-test</id><version>1.0</version></document-info></description>
 <body><section><title><p>Table</p></title><table id="table-id" style="border: 1px solid"><tr align="center"><th id="head" valign="top"><strong>Header</strong></th><td id="cell" colspan="2" rowspan="1" align="right" valign="bottom"><emphasis>Cell</emphasis> <a l:href="#note">link</a></td></tr><tr><td>second</td><td>third</td><td>fourth</td></tr></table></section></body>
 <body name="notes"><section id="note"><title><p>Note</p></title><p>note</p></section></body>
</FictionBook>
'@ | Set-Content -LiteralPath $fixture -Encoding utf8

    $process = Start-Process -FilePath $FbeExe -ArgumentList @('-c', '-b', $report, $fixture) -PassThru
    if (-not $process.WaitForExit($TimeoutSeconds * 1000)) { Stop-Process -Id $process.Id -Force; throw 'FBE не завершил table round-trip benchmark.' }
    if (-not (Test-Path -LiteralPath $report)) { throw 'FBE не записал отчёт table round-trip.' }

    $rows = Import-Csv -LiteralPath $report -Delimiter "`t"
    $tableRows = @($rows | Where-Object { $_.phase -like 'table-*' })
    if ($tableRows.Count -lt 5) { throw "Недостаточно production table snapshots: $($tableRows.Count)." }
    foreach ($row in $tableRows) {
        if ($row.phase -notmatch 'table=1;tr=2;td=4;th=1') { throw "Table structure changed in $($row.phase)." }
    }
    Write-Host "Production table round-trip passed ($($tableRows.Count) snapshots)."
}
finally {
    Remove-Item -LiteralPath $directory -Recurse -Force -ErrorAction SilentlyContinue
}
