<# Verifies that the documented public XML Source API matches the implemented COM contract. #>
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$docs = Get-Content -Raw -LiteralPath (Join-Path $root 'docs\scripting-api.md')
$idl = Get-Content -Raw -LiteralPath (Join-Path $root 'src\contracts\fbe.idl')
foreach($method in 'GetSourceText','ValidateSourceText','GetLastSourceDiagnostic','ApplySourceText') {
    if($docs -notmatch [regex]::Escape("window.external.$method")) { throw "Documentation does not expose window.external.$method." }
    if($idl -notmatch ("\b" + [regex]::Escape($method) + "\b")) { throw "IDL no longer exposes $method." }
}
foreach($field in 'valid','message','line','column') { if($docs -notmatch [regex]::Escape(('`' + $field + '`'))) { throw "Diagnostic JSON field is undocumented: $field." } }
foreach($required in 'MSHTML','FB2','FBD','ValidateSourceText','не меняется' ,'Undo/Redo','несохранённые изменения в режиме Source','eval') {
    if($docs -notmatch [regex]::Escape($required)) { throw "XML Source API documentation is missing: $required" }
}
Write-Host 'XML scripting API documentation consistency passed.'
