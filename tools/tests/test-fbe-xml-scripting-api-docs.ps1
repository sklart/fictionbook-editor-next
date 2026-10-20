<# Verifies that the documented public XML Source API matches the implemented COM contract. #>
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$docs = Get-Content -Raw -LiteralPath (Join-Path $root 'docs\scripting-api.md')
$idl = Get-Content -Raw -LiteralPath (Join-Path $root 'src\contracts\fbe.idl')
foreach($method in 'GetSourceText','ValidateSourceText','GetLastSourceDiagnostic','ApplySourceText') {
    if($docs -notmatch ('\b(?:window\.external|ext)\.' + [regex]::Escape($method) + '\s*\(')) { throw "Documentation has no window.external example for $method." }
    if($docs -notmatch [regex]::Escape('<a id="api-' + $method.ToLowerInvariant() + '"></a>')) { throw "Documentation has no method anchor for $method." }
    if($idl -notmatch ("\b" + [regex]::Escape($method) + "\b")) { throw "IDL no longer exposes $method." }
}
$source = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\ExternalHelper.cpp')
foreach($method in 'GetBinaryInfo','GenerateUniqueBinaryId','AddBinaryFromFile','ReplaceBinaryFromFile','RemoveBinary','SaveAllBinaries') {
    if($source -notmatch [regex]::Escape('L"' + $method + '"')) { throw "Documented binary method is not registered: $method." }
    $anchor = 'api-' + $method.ToLowerInvariant()
    if($docs -notmatch [regex]::Escape('<a id="' + $anchor + '"></a>') -or $docs -notmatch [regex]::Escape('](#' + $anchor + ')')) { throw "Binary method is missing its anchor or TOC entry: $method." }
    if($docs -notmatch ('\b(?:window\.external|ext)\.' + [regex]::Escape($method) + '\s*\(')) { throw "Binary method has no runnable example: $method." }
}
foreach($field in 'valid','message','line','column') { if($docs -notmatch [regex]::Escape(('`' + $field + '`'))) { throw "Diagnostic JSON field is undocumented: $field." } }
foreach($required in 'MSHTML','FB2','FBD','ValidateSourceText','не меняется' ,'Undo/Redo','несохранённые изменения в режиме Source','eval') {
    if($docs -notmatch [regex]::Escape($required)) { throw "XML Source API documentation is missing: $required" }
}
Write-Host 'XML scripting API documentation consistency passed.'
