[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$view = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\FBEview.cpp')
$viewHeader = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\FBEview.h')
$frame = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\mainfrm.cpp')
$frameHeader = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\mainfrm.h')
$pane = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\FindResultsPane.cpp')
$paneHeader = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\FindResultsPane.h')
$settings = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\Settings.cpp')
$coordinator = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\search\DocumentSearchCoordinator.cpp')

function Assert-Contains([string]$text, [string]$pattern, [string]$description) {
    if ($text -notmatch $pattern) { throw "Missing $description." }
}
function Assert-NotContains([string]$text, [string]$pattern, [string]$description) {
    if ($text -match $pattern) { throw "Unexpected $description." }
}

Assert-NotContains $viewHeader 'CFindResultsDlg|m_find_results_dlg' 'legacy floating Results dialog ownership'
Assert-NotContains $view 'CFindResultsDlg|CloseFindResultsDialog' 'legacy floating Results dialog lifecycle'
Assert-Contains $frameHeader 'CHorSplitterWindow\s+m_editor_results_splitter' 'nested horizontal editor/results splitter'
Assert-Contains $frameHeader 'CFindResultsPane\s+m_find_results_pane' 'frame-owned Results pane'
Assert-Contains $frame 'SetSplitterPanes\(m_view, m_find_results_pane\)' 'editor above Results pane hierarchy'
Assert-Contains $frame 'SetSplitterPanes\(m_document_tree, m_editor_results_splitter\)' 'outer tree/editor-results hierarchy'
Assert-Contains $frame 'SetSinglePaneMode\(SPLIT_PANE_LEFT\)' 'hidden Results pane editor-only mode'
Assert-Contains $frame 'void CMainFrame::HideFindResultsPane\(\)[\s\S]*?SetSinglePaneMode\(SPLIT_PANE_LEFT\)' 'close hides rather than destroys Results pane'
Assert-Contains $view 'if \(showResults\)\s*ShowFindResults\(\);\s*else\s*::SendMessage\(m_frame, AU::WM_REFRESH_FIND_RESULTS_PANE' 'explicit Find All opens pane while incremental search only refreshes it'
Assert-Contains $view 'AdvanceSearchDocumentGeneration\([\s\S]*?WM_REFRESH_FIND_RESULTS_PANE' 'semantic mutation refreshes pane stale state'
Assert-Contains $view 'ReplaceAllSearchCore[\s\S]*?DoFindAll\(true, &searchError\)' 'Replace All preview opens the shared pane'
Assert-Contains $paneHeader 'LVN_ITEMACTIVATE' 'activation-only result navigation'
Assert-Contains $paneHeader 'LVN_GETDISPINFO' 'virtual ListView display callback'
Assert-Contains $paneHeader 'NM_CUSTOMDRAW' 'matched-preview custom drawing'
Assert-Contains $pane 'Search results are stale\. Run Find All again\.' 'stale pane state'
Assert-Contains $pane 'LVS_OWNERDATA' 'owner-data Results ListView'
Assert-Contains $pane 'SetItemCountEx\(itemCount' 'virtual result count assignment'
Assert-Contains $pane 'OnGetDispInfo[\s\S]*?FindResultPreview' 'lazy context retrieval'
Assert-NotContains $pane 'for \(std::size_t index = 0; index < m_view->FindResultCount\(\); \+\+index\).*InsertItem' 'eager ListView item creation'
Assert-NotContains $pane 'InsertItem|SetItemText' 'materialized Results ListView rows'
Assert-Contains $coordinator 'm_previewCached\.assign\(resultCount, false\)' 'empty preview cache after Find All'
Assert-Contains $coordinator 'GetResultPreview[\s\S]*?if \(!m_previewCached\[index\]\)[\s\S]*?BuildPreview' 'on-demand preview generation'
Assert-NotContains $coordinator 'for \(std::size_t index = 0; index < hits\.size\(\); \+\+index\)[\s\S]{0,700}BuildPreview' 'eager preview generation for every hit'
Assert-Contains $pane 'GetAncestor\(m_hWnd, GA_ROOT\)[\s\S]*?AU::WM_HIDE_FIND_RESULTS_PANE' 'close button routing to MainFrame'
Assert-NotContains $pane 'WM_APP \+ 43' 'magic Results-pane close message'
Assert-Contains $settings 'FindResultsPaneHeight' 'persisted Results pane height setting'
Assert-Contains $frame 'MulDiv\(savedHeight, dpi, 96\)' 'DPI-scaled persisted Results pane height'
Assert-Contains $frame 'MulDiv\(120, dpi, 96\)' 'DPI-scaled minimum Results pane height'
Assert-Contains $frame 'MulDiv\(160, dpi, 96\)' 'editor minimum height alongside Results pane'
Assert-Contains $frame 'RefreshFindResultsPane[\s\S]*?GetSinglePaneMode\(\) == SPLIT_PANE_NONE' 'hidden pane refresh suppression'
Assert-Contains $frame 'OnDetachFindResultsPane[\s\S]*?m_find_results_pane\.Detach[\s\S]*?HideFindResultsPane' 'detach clears and hides Results pane'
Assert-Contains $frame 'OnDpiChanged[\s\S]*?SetFindResultsPaneHeight\(MulDiv\(height, 96, oldDpi\)\)' 'actual splitter height preserved across DPI changes'
Assert-Contains $frame 'm_editor_results_splitter\.m_cxyMin' 'DPI-aware splitter drag constraint'

Write-Host 'Docked Find Results pane UI contract passed.'
