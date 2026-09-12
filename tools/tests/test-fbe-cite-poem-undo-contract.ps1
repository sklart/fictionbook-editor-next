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
$source = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\structure\BodyStructuralEditor.cpp')
$viewSource = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\FBEview.cpp')

function Get-FunctionBody([string]$Name) {
    $match = [regex]::Match($source, "StructuralOperationResult BodyStructuralEditor::$Name\(bool checkOnly\)\s*\{")
    if(-not $match.Success) { throw "BodyStructuralEditor::$Name was not found." }
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
    foreach($required in @('FbeDom::MarkupUndoUnitScope undo', 'undo.Close();', 'insertBefore', 'removeNode(VARIANT_TRUE)', 'StructuralOperationResult::Applied()', 'StructuralOperationResult::NotApplicable()', 'StructuralOperationResult::Failed(error.Error(), documentChanged)')) {
        if($body -notlike "*$required*") { throw "$name misses Undo contract fragment: $required" }
    }
    if($body -match 'FixupParagraphs\(pe\)|PackText\(pe, Document\(\)\)') { throw "$name performs global normalization." }
    if($body.IndexOf('FbeDom::MarkupUndoUnitScope undo') -lt $body.IndexOf('CString rngHTML')) { throw "$name begins undo before HTML preparation." }
}

foreach($name in @('InsertCite', 'InsertPoem')) {
    $wrapper = [regex]::Match($viewSource, "bool CFBEView::$name\(bool fCheck\)\s*\{(?<body>.*?)\n\}", [Text.RegularExpressions.RegexOptions]::Singleline)
    if(-not $wrapper.Success -or $wrapper.Groups['body'].Value -notmatch "BodyStructuralEditor editor\(Document\(\), m_mk_srv\);\s*const FbeStructure::StructuralOperationResult result = editor\.$name\(fCheck\);" -or $wrapper.Groups['body'].Value -notmatch 'return result.IsApplied\(\);') { throw "CFBEView::$name is not a structural-editor wrapper." }
}

$cite = Get-FunctionBody 'InsertCite'
if($cite.Contains('createElement(L"DIV")') -and $cite.Contains('className = L"cite"')) { throw 'InsertCite changes className as a separate live operation.' }
if(-not $cite.Contains('createElement(L"<DIV class=cite>")')) { throw 'InsertCite does not create Cite in final form.' }

$poem = Get-FunctionBody 'InsertPoem'
if(-not $poem.Contains('const bool wasCollapsed = range->compareEndPoints(L"StartToEnd", range) == 0')) { throw 'InsertPoem does not capture a collapsed range before expanding paragraphs.' }
if($poem.Contains('selectedText.Trim().IsEmpty()')) { throw 'InsertPoem treats whitespace-only text as an empty selection.' }
if(-not $poem.Contains('if (wasCollapsed && !expandedHasContent)')) { throw 'InsertPoem does not limit an empty Poem to a collapsed range without paragraph content.' }

$undoSource = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\dom\MarkupUndoUnitScope.h')
foreach($required in @('class MarkupUndoUnitScope', '~MarkupUndoUnitScope()', 'try { m_services->EndUndoUnit(); }', 'catch (_com_error&) { }')) {
    if($undoSource -notlike "*$required*") { throw "Markup undo RAII misses: $required" }
}
$close = [regex]::Match($undoSource, 'void Close\(\)\s*\{(?<body>.*?)\n\s*\}', [Text.RegularExpressions.RegexOptions]::Singleline)
if(-not $close.Success) { throw 'MarkupUndoUnitScope::Close was not found.' }
$closeBody = $close.Groups['body'].Value
if($closeBody.IndexOf('m_services->EndUndoUnit();') -gt $closeBody.IndexOf('m_active = false;')) {
    throw 'Close deactivates the scope before EndUndoUnit succeeds.'
}

Write-Host 'Cite/Poem Undo contract passed.'
