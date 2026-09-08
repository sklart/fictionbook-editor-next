<#
.SYNOPSIS
Guards the one-step MSHTML undo contract for Cite and Poem insertion.

Full runtime automation of the legacy MSHTML selection stack is unavailable in
CI. This guard verifies the implementation boundary that prevents extra undo
records; docs/manual-test-plan.md covers the live Ctrl+Z/Ctrl+Y smoke cases.
#>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$source = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\FBEview.cpp')

function Get-FunctionBody([string]$Name) {
    $match = [regex]::Match($source, "bool CFBEView::$Name\(bool fCheck\)\s*\{")
    if(-not $match.Success) { throw "CFBEView::$Name was not found." }
    $depth = 0
    for($index = $match.Index; $index -lt $source.Length; ++$index) {
        if($source[$index] -eq '{') { ++$depth }
        elseif($source[$index] -eq '}') {
            --$depth
            if($depth -eq 0) { return $source.Substring($match.Index, $index - $match.Index + 1) }
        }
    }
    throw "The end of CFBEView::$Name was not found."
}

foreach($name in @('InsertCite', 'InsertPoem')) {
    $body = Get-FunctionBody $name
    foreach($required in @('CMarkupUndoUnitScope undo', 'undo.Close();', 'insertBefore', 'removeNode(VARIANT_TRUE)', 'return true;', 'return false;')) {
        if($body -notlike "*$required*") { throw "$name misses Undo contract fragment: $required" }
    }
    if($body -match 'FixupParagraphs\(pe\)|PackText\(pe, Document\(\)\)') { throw "$name performs global normalization." }
    if($body.IndexOf('CMarkupUndoUnitScope undo') -lt $body.IndexOf('CString rngHTML')) { throw "$name begins undo before HTML preparation." }
}

$cite = Get-FunctionBody 'InsertCite'
if($cite.Contains('createElement(L"DIV")') -and $cite.Contains('ne->className = L"cite"')) { throw 'InsertCite changes className as a separate live operation.' }
if(-not $cite.Contains('createElement(L"<DIV class=cite>")')) { throw 'InsertCite does not create Cite in final form.' }

foreach($required in @('class CMarkupUndoUnitScope', '~CMarkupUndoUnitScope()', 'try { m_view.EndUndoUnit(); }', 'catch (_com_error&) { }')) {
    if($source -notlike "*$required*") { throw "Markup undo RAII misses: $required" }
}

Write-Host 'Cite/Poem Undo contract passed.'
