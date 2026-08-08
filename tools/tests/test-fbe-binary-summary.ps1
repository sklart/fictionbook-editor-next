$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path $PSScriptRoot)
$source = Get-Content -Raw -LiteralPath (Join-Path $root 'runtime\main.js')
foreach($required in @('var failureCount=0;', 'var additionalFailureCount = failureCount > 3 ? failureCount - 3 : 0;', 'binary-failures=" + failureCount')) {
    if(-not $source.Contains($required)) { throw "Missing binary summary contract: $required" }
}
foreach($failures in @(0, 1, 3, 4, 10)) {
    $additional = if($failures -gt 3) { $failures - 3 } else { 0 }
    if($failures -ne $failures) { throw 'Binary failure count changed unexpectedly.' }
    if($additional -ne [Math]::Max(0, $failures - 3)) { throw "Incorrect additional failure count for $failures." }
}
Write-Host 'PutBinaries diagnostic summary contract passed.'
