$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path $PSScriptRoot)
$source = Get-Content -Raw -LiteralPath (Join-Path $root 'runtime\main.js')
foreach($assignment in @('for\(k\s*=', 'for\s*\(kj\s*=')) {
    if($source -match $assignment) { throw "Accidental JavaScript global remains: $assignment" }
}
foreach($required in @('var html = new String(range.htmlText)', 'var per = range.parentElement()', 'var ped = end.parentElement()', 'var targ = np.tagName', 'for(var k = 0;', 'for (var kj=')) {
    if(-not $source.Contains($required)) { throw "Missing localized variable: $required" }
}
Write-Host 'Known accidental JavaScript globals are localized.'
