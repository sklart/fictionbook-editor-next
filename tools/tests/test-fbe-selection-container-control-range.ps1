<#
.SYNOPSIS
Verifies the MSHTML Control selection path used when an image is selected from
the document tree.
#>
[CmdletBinding()]
param(
    [string]$RepoRoot = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot))
)

$ErrorActionPreference = 'Stop'
$viewSource = Get-Content -Raw -LiteralPath (Join-Path $RepoRoot 'src\fbe\FBEview.cpp')
$fixture = Join-Path $RepoRoot 'tools\tests\fixtures\tree-image-scrollbar.fb2'
if(-not (Test-Path -LiteralPath $fixture -PathType Leaf)) { throw 'Tree image scrollbar regression fixture is missing.' }
$fixtureText = Get-Content -Raw -LiteralPath $fixture
if($fixtureText -notmatch '<body>.*<image l:href="#image-1"' -or $fixtureText -notmatch '<binary id="image-1"') {
    throw 'Regression fixture must contain a body image with an existing binary.'
}
if($fixtureText -match '#undefined') { throw 'Regression fixture must not depend on a missing #undefined binary.' }

$start = $viewSource.IndexOf('MSHTML::IHTMLElementPtr CFBEView::SelectionContainerImp()')
$end = $viewSource.IndexOf('MSHTML::IHTMLElementPtr CFBEView::SelectionAnchor()', $start)
if($start -lt 0 -or $end -lt 0) { throw 'CFBEView::SelectionContainerImp was not found.' }
$body = $viewSource.Substring($start, $end - $start)

if($body -match 'commonParentElement\s*\(') {
    throw 'SelectionContainerImp must not call IHTMLControlRange::commonParentElement for a transient Control selection.'
}
foreach($marker in @(
    'controls->length',
    'controls->item(0)',
    'QueryInterface(IHTMLControlRange)',
    'IHTMLControlRange::get_length',
    'IHTMLControlRange::item(0)',
    'IHTMLElement::tagName/parentElement',
    'TraceSelectionContainerFailure')) {
    if($body.IndexOf($marker, [StringComparison]::Ordinal) -lt 0) {
        throw "SelectionContainerImp lacks required Control selection handling: $marker"
    }
}
if($body -notmatch 'selected->tagName, L"IMG"' -or $body -notmatch 'parent->className, L"image"') {
    throw 'A raw IMG Control selection is not normalized to its logical image wrapper.'
}
if($body -match 'U::ReportError\s*\(') {
    throw 'Best-effort SelectionContainer synchronization must not show a modal COM error.'
}
foreach($marker in @('operation=SelectionContainer', 'HRESULT_NAME=', 'documentTree=%s', 'treeImages=%s', 'StartupTrace::HResult')) {
    if($viewSource.IndexOf($marker, [StringComparison]::Ordinal) -lt 0) {
        throw "SelectionContainer diagnostics lack required context: $marker"
    }
}

Write-Host 'MSHTML Control selection container path uses a safe selected-element query and diagnostic tracing.'
