[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$source = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\FBEview.cpp')
$header = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\FBEview.h')
$dialog = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\SearchReplace.h')
$pane = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\FindResultsPane.cpp')
$frame = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\mainfrm.cpp')

function Assert-Contains([string]$text, [string]$pattern, [string]$description) {
    if ($text -notmatch $pattern) { throw "Missing $description." }
}

function Assert-NotContains([string]$text, [string]$pattern, [string]$description) {
    if ($text -match $pattern) { throw "Unexpected $description." }
}

Assert-Contains $header 'm_last_zero_length_query' 'zero-length query identity'
Assert-Contains $source 'HasSameSearchCriteria\(m_last_zero_length_query, query\)' 'zero-length criteria reset'
Assert-Contains $source 'm_last_zero_length_query\.Direction == query\.Direction' 'zero-length direction reset'
Assert-Contains $source 'CanReuseDocumentSearch\(query, generation\)' 'cached Find Next decision'
Assert-Contains $source 'GetSession\(\)\.IsValidFor\(generation\)' 'cached session generation validation'
Assert-Contains $source 'GetResults\(\)\.IsValidFor\(generation\)' 'cached results generation validation'
Assert-Contains $source 'openingReplace = !m_replace_dlg \|\| !m_replace_dlg->IsValid\(\)' 'Replace reopen detection'
Assert-Contains $source 'm_has_replace_preview = false' 'Replace preview reset on reopen'
Assert-NotContains $source 'm_fo\.scope = AU::Search::SearchScope::WholeDocument' 'Replace must not discard Find scope on open'
Assert-NotContains $source 'm_fo\.unicodeProperties = false' 'Replace must not discard UCP on open'
Assert-Contains $source 'return DoSearchNative\(fMore, AU::Search::SearchMode::Regex\);' 'native regex Find Next'
Assert-NotContains $source 'DoSearchNative\(fMore, AU::Search::SearchMode::Regex\);\s*/\*\s*Legacy implementation' 'unreachable legacy regexp implementation'
Assert-Contains $source 'CheckReplacementRange' 'production replacement preflight'
Assert-Contains $source 'fbe\.replace\.cross_paragraph' 'clear cross-paragraph replacement error'
Assert-Contains $source 'const std::size_t count = m_document_search.GetResults\(\)\.GetCount\(\);\s*if \(count == 0\)\s*return 0;' 'GlobalReplace does not open an empty mutation path'
Assert-Contains $source 'if \(mutationApplied\)\s*AdvanceSearchDocumentGeneration\(\);' 'failed replacement advances semantic generation only after a real DOM mutation'
Assert-Contains $header 'CString\s+m_last_search_error' 'native search diagnostic channel'
Assert-Contains $source 'm_last_search_error\s*=\s*nativeError\.c_str\(\)' 'PCRE2 diagnostic retained by native Find'
Assert-Contains $dialog 'LastSearchError\(\)\.IsEmpty\(\)' 'Find Next checks the actual regexp diagnostic'
Assert-Contains $dialog 'LastSearchErrorIsRegexp\(\)' 'Find Next reserves modal diagnostics for real regexp failures'
Assert-Contains $dialog '::MessageBox\(m_hWnd, m_view->LastSearchError\(\)' 'Find Next shows the actual regexp diagnostic'
Assert-Contains $source 'm_last_search_error_is_regexp = expressionError' 'scope-state and mapping failures are not classified as regexp errors'
Assert-Contains $dialog 'DoFindAll\(true, &error\)' 'Find All receives native regexp diagnostic'
Assert-Contains $dialog 'fbe\.search\.error\.mapping_failed' 'unknown Find All failure has an explicit status error rather than a false regexp diagnosis'
Assert-Contains $dialog 'm_tooltips\.UpdateText\(FRBase::GetDlgItem\(IDC_FIND_STATUS\), tooltip\)' 'Find status tooltip tracks the exact current diagnostic'
Assert-Contains $dialog 'OnCancel[\s\S]*?GetData\(\);[\s\S]*?SaveSearchOptions\(\);' 'Cancel persists Find options without history'
Assert-Contains $dialog 'OnClose[\s\S]*?GetData\(\);[\s\S]*?SaveSearchOptions\(\);' 'close button persists Find options without history'
Assert-Contains $dialog 'CBN_DROPDOWN, OnScopeDropDown' 'Scope is refreshed when its list opens'
Assert-Contains $dialog 'HasSavedSearchScope\(\)' 'saved Selection scope stays available while generation is current'
Assert-Contains $dialog 'EnableWindow\(unicode, ::IsDlgButtonChecked' 'UCP availability follows RegExp without clearing its state'
Assert-Contains $dialog 'class CReplaceDlgBase[\s\S]*?PopulateFindScopes\(\)' 'Replace exposes the same scope list as Find'
Assert-Contains $dialog 'class CReplaceDlgBase[\s\S]*?IDC_FIND_UNICODE_PROPERTIES' 'Replace exposes UCP'
Assert-Contains $dialog 'class CReplaceDlgBase[\s\S]*?IDC_FIND_FROM_START' 'Replace exposes From start'
Assert-Contains $dialog 'class CReplaceDlgBase[\s\S]*?OnScopeChanged[\s\S]*?ResetSearchScope' 'Replace scope changes invalidate only the saved scope range'
Assert-Contains $dialog 'class CReplaceDlgBase[\s\S]*?OnFindFromStart[\s\S]*?DoSearchFromScopeStart' 'Replace From start uses the shared Search Core entry point'
Assert-Contains $dialog 'otherDialogOpen[\s\S]*?IsFindDialogOpen\(\)[\s\S]*?IsReplaceDialogOpen\(\)' 'opening the peer dialog preserves unsaved common search options'
Assert-Contains $pane 'OnListCustomDraw' 'Results pane custom-draw match presentation'
Assert-Contains $pane 'selected \? COLOR_HIGHLIGHT : COLOR_WINDOW' 'selected result context uses the system highlight background'
Assert-Contains $pane 'GetSysColorBrush\(selected \? COLOR_HIGHLIGHT : COLOR_WINDOW\)' 'each custom-drawn context cell is fully cleared before repaint'
Assert-Contains $pane 'COLOR_HIGHLIGHTTEXT' 'selected result context uses the system highlight text color'
Assert-Contains $pane 'm_list\.GetItemState\(item, LVIS_SELECTED\) & LVIS_SELECTED' 'custom draw uses authoritative ListView selection state'
Assert-Contains $pane 'GetSubItemRect\(item, 1, LVIR_BOUNDS' 'selected result paints the complete context cell'
Assert-Contains $pane 'L" \\x2014 \\x00AB" \+ query \+ L"\\x00BB \\x2014 "' 'Results header uses codepage-independent Unicode punctuation'
Assert-Contains $pane 'OnItemActivate' 'Results pane activation navigates to a result'
Assert-Contains $frame 'SetSinglePaneMode\(SPLIT_PANE_LEFT\)' 'Results pane is hidden as a single editor pane by default and on close'
Assert-Contains $frame 'SetSplitterPanes\(m_view, m_find_results_pane\)' 'Results pane is nested below the editor'

$selectResult = [regex]::Match($source, 'bool\s+CFBEView::SelectFindResult\(std::size_t index\)\s*\{[\s\S]*?\r?\n}\r?\n\r?\nvoid\s+CFBEView::ClearSearchHighlights').Value
if ([string]::IsNullOrWhiteSpace($selectResult)) { throw 'Unable to locate Find result selection path.' }
Assert-Contains $selectResult 'PositionFoundRange' 'result selection navigates the editor'
Assert-Contains $selectResult 'RefreshSearchHighlights' 'result selection refreshes highlights'
Assert-NotContains $selectResult 'OnViewToolBar|IsBandVisible|ATL_IDW_BAND_FIRST' 'result selection must not change rebar-band visibility'
Assert-Contains $dialog 'IDC_FIND_FROM_START' 'Find exposes a separate From start command'
Assert-Contains $dialog 'DoSearchFromScopeStart' 'From start uses the current Search Core scope'
Assert-Contains $source 'DoSearchNative\(true, m_fo\.fRegexp \? AU::Search::SearchMode::Regex : AU::Search::SearchMode::Literal, true\)' 'From start keeps the existing direction state while using a dedicated action'
Assert-Contains $source 'm_has_find_scope_range \? m_find_scope_range\.Start : 0' 'From start begins at the current scope boundary'
Assert-Contains $source 'AU::Search::SearchDirection::Forward, &wrapped' 'From start selects the first match without adding a third direction'
Assert-Contains $dialog 'SyncSearchOptionsToOpenDialogs\(this\)' 'common options update the shared model immediately'
Assert-Contains $source 'SyncSearchOptionsToOpenDialogs\(FRBase\* source\)' 'open Find and Replace dialogs synchronize their common controls'

$singleReplace = [regex]::Match($source, 'void\s+CFBEView::DoReplace\(\)\s*\{[\s\S]*?\r?\n}\r?\n\r?\nint\s+CFBEView::ReplaceAllSearchCore').Value
if ([string]::IsNullOrWhiteSpace($singleReplace)) { throw 'Unable to locate native single Replace path.' }
Assert-Contains $singleReplace 'CheckReplacementRange\(sel, m_fo\.fRegexp\) == ReplacementPreflightResult::CrossParagraph[\s\S]*?CrossParagraphReplacementError\(\)[\s\S]*?return;' 'single Replace production preflight rejection'
$singleGuard = $singleReplace.IndexOf('CheckReplacementRange(sel, m_fo.fRegexp)')
$singleUndo = $singleReplace.IndexOf('BeginUndoUnit(L"replace")')
if ($singleGuard -lt 0 -or $singleUndo -lt 0 -or $singleGuard -gt $singleUndo) { throw 'Single Replace must reject a cross-paragraph range before opening Undo.' }

$replaceAll = [regex]::Match($source, 'int\s+CFBEView::ReplaceAllSearchCore\(CString\* errorText\)[\s\S]*?\r?\n}\r?\n\r?\nint\s+CFBEView::GlobalReplace').Value
if ([string]::IsNullOrWhiteSpace($replaceAll)) { throw 'Unable to locate native Replace All path.' }
Assert-Contains $replaceAll 'CheckReplacementRange\(ranges\[index\], m_fo\.fRegexp\) == ReplacementPreflightResult::CrossParagraph[\s\S]*?\*errorText = CrossParagraphReplacementError\(\)[\s\S]*?return -1;' 'Replace All production preflight rejection and error'
$allGuard = $replaceAll.IndexOf('CheckReplacementRange(ranges[index], m_fo.fRegexp)')
$allUndo = $replaceAll.IndexOf('BeginUndoUnit(L"replace all")')
if ($allGuard -lt 0 -or $allUndo -lt 0 -or $allGuard -gt $allUndo) { throw 'Replace All must reject a cross-paragraph range before opening Undo.' }
Assert-Contains $replaceAll 'm_replace_all_completion_pending = true;[\s\S]*?WM_FINALIZE_REPLACE_ALL_COMPLETION' 'successful Replace All defers completion until queued MSHTML notifications have drained'
Assert-Contains $replaceAll 'if \(!previewIsCurrent\(\)\)[\s\S]*?DoFindAll\(true, &searchError\)' 'first Replace All builds and displays the preview'
Assert-Contains $replaceAll 'MB_YESNO \| MB_ICONQUESTION' 'Replace All has one Yes/No confirmation'
Assert-Contains $replaceAll 'MB_YESNO \| MB_ICONQUESTION\) != IDYES\)\s*return -2;[\s\S]*?if \(!previewIsCurrent\(\)\)' 'preview identity is rechecked after confirmation'
Assert-NotContains $replaceAll 'fbe\.replace\.preview\.ready|MB_OK \| MB_ICONINFORMATION' 'obsolete second-click Replace All information prompt'
Assert-Contains $replaceAll 'MB_YESNO \| MB_ICONQUESTION\) != IDYES\)\s*return -2;[\s\S]*?std::vector<MSHTML::IHTMLTxtRangePtr> ranges' 'No leaves preview intact before any replacement range is opened'
Assert-Contains $replaceAll 'if \(!previewIsCurrent\(\)\)[\s\S]*?return -1;[\s\S]*?std::vector<MSHTML::IHTMLTxtRangePtr> ranges' 'changed preview identity blocks replacement before ranges and Undo'
Assert-Contains $source 'OnFinalizeReplaceAllCompletion[\s\S]*?m_find_results_completion_status = completion;' 'successful replacement clears stale preview rows and reports completion'
Assert-Contains $replaceAll 'm_controlled_replace_all_mutation = true;[\s\S]*?BeginUndoUnit\(L"replace all"\)' 'Replace All coalesces its own RANGE_SINK notifications before mutation'
Assert-Contains $replaceAll 'm_replace_all_completion_count = replaced;[\s\S]*?m_replace_all_completion_pending = true;[\s\S]*?PostMessage\(m_hWnd, AU::WM_FINALIZE_REPLACE_ALL_COMPLETION' 'successful Replace All schedules one protected completion phase'

Assert-Contains $source 'OnFinalizeReplaceAllCompletion[\s\S]*?AdvanceSearchDocumentGeneration\(false\);[\s\S]*?m_find_results_completion_status = completion;[\s\S]*?m_replace_all_completion_pending = false;[\s\S]*?WM_REFRESH_FIND_RESULTS_PANE' 'posted completion finalizes one invalidation before publishing the Results-pane status'
Assert-Contains $source 'case RANGE_SINK:[\s\S]*?if \(!m_controlled_replace_all_mutation && !m_replace_all_completion_pending\)\s*AdvanceSearchDocumentGeneration\(\);' 'ordinary RANGE_SINK invalidates searches while controlled and pending Replace All notifications are coalesced'

$globalReplace = [regex]::Match($source, 'int\s+CFBEView::GlobalReplace\(MSHTML::IHTMLElementPtr elem, CString cntTag\)[\s\S]*?\r?\n}\r?\n\r?\nint\s+CFBEView::ToolWordsGlobalReplace').Value
if ([string]::IsNullOrWhiteSpace($globalReplace)) { throw 'Unable to locate Search Core GlobalReplace path.' }
Assert-Contains $globalReplace 'const std::size_t count = m_document_search.GetResults\(\)\.GetCount\(\);\s*if \(count == 0\)\s*return 0;' 'GlobalReplace does not open a no-op Undo unit or invalidate results'
Assert-Contains $globalReplace 'bool mutationApplied = false;' 'GlobalReplace tracks actual DOM writes'
Assert-Contains $globalReplace 'catch \(_com_error& err\)[\s\S]*?if \(mutationApplied\)\s*AdvanceSearchDocumentGeneration\(\);' 'GlobalReplace invalidates partial failed writes only'

Write-Host 'Native Search Core contracts passed.'
