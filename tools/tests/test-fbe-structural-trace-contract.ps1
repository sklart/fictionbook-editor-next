<# Guards StructuralTrace's TSV record contract. #>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$source = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\structure\StructuralTrace.cpp')
foreach($required in @('timestamp\toperation\tbackend\tcase\tphase\tevent\thresult\tdetails\r\n', 'Sanitize(operation', 'Sanitize(caseName', 'Sanitize(safePhase', 'Sanitize(safeEvent', 'Sanitize(safeDetails', '0x%08X')) {
    if($source -notlike "*$required*") { throw "StructuralTrace misses TSV contract fragment: $required" }
}
if($source -match 'Sanitize\(line\)') { throw 'StructuralTrace sanitizes the completed TSV line.' }

$trace = Join-Path ([IO.Path]::GetTempPath()) ('fbe-structural-trace-' + [guid]::NewGuid().ToString('N') + '.tsv')
try {
    @(
        "timestamp`toperation`tbackend`tcase`tphase`tevent`thresult`tdetails",
        "t1`tcite`textracted`tcase`tp1`tafter`t0x00000000`tTAB CR LF",
        "t2`tpoem`textracted`tcase`tp2`tbefore`tNA`tclean"
    ) | Set-Content -LiteralPath $trace -Encoding utf8
    $rows = Import-Csv -LiteralPath $trace -Delimiter "`t"
    if(@($rows).Count -ne 2) { throw 'StructuralTrace TSV record count is wrong.' }
    foreach($column in @('timestamp', 'operation', 'backend', 'case', 'phase', 'event', 'hresult', 'details')) {
        if(-not ($rows[0].PSObject.Properties.Name -contains $column)) { throw "StructuralTrace TSV lacks column: $column" }
    }
    if($rows[0].details -ne 'TAB CR LF' -or $rows[0].hresult -ne '0x00000000') { throw 'StructuralTrace field sanitization contract is wrong.' }
}
finally { Remove-Item -LiteralPath $trace -Force -ErrorAction SilentlyContinue }
Write-Host 'StructuralTrace TSV contract passed.'
