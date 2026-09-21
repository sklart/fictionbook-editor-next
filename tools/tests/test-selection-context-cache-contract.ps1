$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent (Split-Path $PSScriptRoot -Parent)
$header = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\mainfrm.h')
$source = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\mainfrm.cpp')
foreach($token in @('struct SelectionContext', 'm_selection_context', 'InvalidateSelectionContext()', 'RebuildSelectionContext()')) {
    if($header.IndexOf($token, [StringComparison]::Ordinal) -lt 0) { throw "Selection context declaration is missing '$token'." }
}
foreach($token in @('m_selection_context.valid ||', 'SelectionContainer()', 'for (MSHTML::IHTMLElementPtr current', 'SelectionStructTableCon()', 'm_selection_context.tableCell', 'm_selection_context.anchor', 'InvalidateSelectionContext();')) {
    if($source.IndexOf($token, [StringComparison]::Ordinal) -lt 0) { throw "Selection context implementation is missing '$token'." }
}
$idleStart = $source.IndexOf('BOOL CMainFrame::OnIdle()')
$idleEnd = $source.IndexOf('void CMainFrame::AddTbButton', $idleStart)
$idle = $source.Substring($idleStart, $idleEnd - $idleStart)
if($idle.IndexOf('SelectionStructTableCon()', [StringComparison]::Ordinal) -ge 0) { throw 'OnIdle table commands must consume the cached selection context.' }
$builderStart = $source.IndexOf('void CMainFrame::RebuildSelectionContext()')
$builderEnd = $source.IndexOf('BOOL CMainFrame::OnIdle()', $builderStart)
$builder = $source.Substring($builderStart, $builderEnd - $builderStart)
if(([regex]::Matches($builder, 'm_selection_context\.container\s*=\s*m_doc->m_body\.SelectionContainer\(\)')).Count -ne 1) { throw 'SelectionContext builder must query SelectionContainer exactly once.' }
if($builder.IndexOf('StartupTrace::CountUiSelectionContextContainerQuery()', [StringComparison]::Ordinal) -lt 0) { throw 'SelectionContext builder must expose its dedicated container-query counter.' }
foreach($helper in @('SelectionStructCon()', 'SelectionStructImage()', 'SelectionStructSection()', 'SelectionStructTable()', 'SelectionAnchor()')) {
    if($builder.IndexOf($helper, [StringComparison]::Ordinal) -ge 0) { throw "SelectionContext builder must not restart traversal through $helper." }
}
Write-Host 'Selection context cache contract passed.'
