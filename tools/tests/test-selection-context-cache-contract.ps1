$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent (Split-Path $PSScriptRoot -Parent)
$header = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\mainfrm.h')
$source = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\mainfrm.cpp')
foreach($token in @('struct SelectionContext', 'm_selection_context', 'InvalidateSelectionContext()', 'RebuildSelectionContext()')) {
    if($header.IndexOf($token, [StringComparison]::Ordinal) -lt 0) { throw "Selection context declaration is missing '$token'." }
}
foreach($token in @('m_selection_context.valid ||', 'SelectionStructCon()', 'SelectionStructTableCon()', 'm_selection_context.tableCell', 'm_selection_context.anchor', 'InvalidateSelectionContext();')) {
    if($source.IndexOf($token, [StringComparison]::Ordinal) -lt 0) { throw "Selection context implementation is missing '$token'." }
}
$idleStart = $source.IndexOf('BOOL CMainFrame::OnIdle()')
$idleEnd = $source.IndexOf('void CMainFrame::AddTbButton', $idleStart)
$idle = $source.Substring($idleStart, $idleEnd - $idleStart)
if($idle.IndexOf('SelectionStructTableCon()', [StringComparison]::Ordinal) -ge 0) { throw 'OnIdle table commands must consume the cached selection context.' }
Write-Host 'Selection context cache contract passed.'
