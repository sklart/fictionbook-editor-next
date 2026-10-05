[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$source = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\FBEview.cpp')
$header = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\FBEview.h')
$controller = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\search\DesignSearchController.cpp')
$dialog = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\SearchReplace.h')
$pane = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\FindResultsPane.cpp')
$frame = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\mainfrm.cpp')
$parser = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\search\ReplacementParser.cpp')
$snapshot = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\search\SearchTextSnapshot.cpp')
$regexBackend = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\search\RegexBackend.h')

function Assert-Contains([string]$text, [string]$pattern, [string]$description) {
    if ($text -notmatch $pattern) { throw "Missing $description." }
}

function Assert-NotContains([string]$text, [string]$pattern, [string]$description) {
    if ($text -match $pattern) { throw "Unexpected $description." }
}

Assert-Contains $controller 'm_lastZeroLengthQuery' 'zero-length query identity'
Assert-Contains $source 'PrepareZeroLengthSearch\(query\)' 'zero-length criteria reset delegation'
Assert-Contains $controller 'HasSameSearchCriteria\(m_lastZeroLengthQuery, query\)' 'zero-length criteria reset'
Assert-Contains $controller 'm_lastZeroLengthQuery\.Direction != query\.Direction' 'zero-length direction reset'
Assert-Contains $source 'CanReuseDocumentSearch\(query, generation\)' 'cached Find Next decision'
Assert-Contains $source 'GetSession\(\)\.IsValidFor\(generation\)' 'cached session generation validation'
Assert-Contains $source 'GetResults\(\)\.IsValidFor\(generation\)' 'cached results generation validation'
Assert-Contains $source 'openingReplace = !m_replace_dlg \|\| !m_replace_dlg->IsValid\(\)' 'Replace reopen detection'
Assert-Contains $source 'm_design_search\.ClearReplacePreview\(\)' 'Replace preview reset on reopen'
Assert-NotContains $source 'm_fo\.scope = AU::Search::SearchScope::WholeDocument' 'Replace must not discard Find scope on open'
Assert-NotContains $source 'm_fo\.unicodeProperties = false' 'Replace must not discard UCP on open'
Assert-Contains $source 'return DoSearchNative\(fMore, AU::Search::SearchMode::Regex\);' 'native regex Find Next'
Assert-NotContains $source 'DoSearchNative\(fMore, AU::Search::SearchMode::Regex\);\s*/\*\s*Legacy implementation' 'unreachable legacy regexp implementation'
Assert-NotContains $source 'GetReplStr\(' 'replacement parser copy in FBEview'
Assert-Contains $source 'AU::Search::ExpandRegexReplacement' 'FBE uses the production replacement parser'
Assert-Contains $parser 'Empty and nonparticipating captures are intentional empty output' 'empty capture replacement semantics'
Assert-Contains $parser 'CloseRun\(result, activeFlags, activeStart, formatting\)' 'format runs close at mode changes'
Assert-Contains $parser 'ReplacementFormatStrong' 'production strong formatting run'
Assert-Contains $snapshot 'explicit anchor for an empty source paragraph' 'mapped zero-length paragraph anchor'
Assert-Contains $frame 'SCI_SETSTATUS, SC_STATUS_OK' 'Source search resets Scintilla status'
Assert-Contains $frame 'SCI_GETSTATUS' 'Source search reads Scintilla status'
Assert-Contains $frame 'fbe\.regex\.error\.source' 'Source regexp error is localized'
Assert-NotContains $frame 'num_pat_nbsp|num_rep_nbsp' 'NBSP byte-offset correction arithmetic'
Assert-Contains $frame 'ScintillaPositionAfter\(m_source, static_cast<int>\(m_source\.SendMessage\(SCI_GETTARGETEND\)\)\)' 'Source Replace All advances zero-length matches through SCI_POSITIONAFTER'
Assert-Contains $source 'skipCurrentZeroLength[\s\S]*?ScintillaPositionAfter\(src, p1\)' 'Source Find advances a replaced zero-length match through SCI_POSITIONAFTER'
Assert-Contains $source 'ScintillaPositionBefore\(src, p1\)' 'Source reverse search uses a Scintilla character boundary'
Assert-Contains $source 'exhaustedZeroLengthPosition = nextPosition == p1' 'Source recognizes exhausted zero-length document boundaries'
Assert-Contains $source 'zeroLengthGuardStart, int zeroLengthGuardEnd' 'Source Find receives the replaced zero-length guard interval'
Assert-Contains $source 'isProtectedZeroLengthHit' 'Source wrap recognizes a replaced internal zero-length hit'
Assert-Contains $source 'hitStart >= zeroLengthGuardFirst && hitStart <= zeroLengthGuardLast' 'Source protects the complete zero-length source-to-result interval'
Assert-Contains $frame 'm_zero_length_guard_start = zeroLengthGuardStart' 'Source Replace records the original zero-length anchor'
Assert-Contains $frame 'm_zero_length_guard_end = resume' 'Source Replace records the post-replacement zero-length anchor'
Assert-Contains $source 'CString searchPattern\(m_fo\.pattern\)' 'Source Find normalizes a pattern copy'
Assert-Contains $frame 'CString patternText\(m_view->m_fo\.pattern\)' 'Source Replace All normalizes a pattern copy'
Assert-Contains $frame 'CString replacementText\(m_view->m_fo\.replacement\)' 'Source Replace All normalizes a replacement copy'
Assert-Contains $source 'CheckReplacementRange' 'production replacement preflight'
Assert-Contains $source 'fbe\.replace\.cross_paragraph' 'clear cross-paragraph replacement error'
Assert-Contains $source 'const std::size_t count = m_design_search\.Coordinator\(\)\.GetResults\(\)\.GetCount\(\);\s*if \(count == 0\)\s*return 0;' 'GlobalReplace does not open an empty mutation path'
Assert-Contains $source 'if \(mutationApplied\)\s*AdvanceSearchDocumentGeneration\(\);' 'failed replacement advances semantic generation only after a real DOM mutation'
Assert-Contains $header 'CString\s+m_last_search_error' 'native search diagnostic channel'
Assert-Contains $source 'm_last_search_error\s*=\s*nativeError\.c_str\(\)' 'PCRE2 diagnostic retained by native Find'
Assert-Contains $dialog 'LastSearchError\(\)\.IsEmpty\(\)' 'Find Next checks the actual regexp diagnostic'
Assert-Contains $dialog 'LastSearchErrorIsRegexp\(\)' 'Find Next reserves modal diagnostics for real regexp failures'
Assert-Contains $dialog '::MessageBox\(m_hWnd, m_view->LastSearchError\(\)' 'Find Next shows the actual regexp diagnostic'
Assert-Contains $source 'm_last_search_error_is_regexp = expressionError' 'scope-state and mapping failures are not classified as regexp errors'
Assert-Contains $source 'Coordinator\(\)\.Rebuild\(Document\(\), generation, query, errorText, &range\)[\s\S]*?\*expressionError = query\.Mode == AU::Search::SearchMode::Regex;' 'scoped regexp query failures preserve the regexp diagnostic'
Assert-Contains $dialog 'DoFindAll\(true, &error\)' 'Find All receives native regexp diagnostic'
Assert-Contains $dialog 'fbe\.search\.error\.mapping_failed' 'unknown Find All failure has an explicit status error rather than a false regexp diagnosis'
Assert-Contains $dialog 'm_tooltips\.UpdateText\(FRBase::GetDlgItem\(IDC_FIND_STATUS\), tooltip\)' 'Find status tooltip tracks the exact current diagnostic'
Assert-Contains $dialog 'OnCancel[\s\S]*?GetData\(\);[\s\S]*?SaveSearchOptions\(\);' 'Cancel persists Find options without history'
Assert-Contains $dialog 'OnClose[\s\S]*?GetData\(\);[\s\S]*?SaveSearchOptions\(\);' 'close button persists Find options without history'
Assert-Contains $dialog 'CBN_DROPDOWN, OnScopeDropDown' 'Scope is refreshed when its list opens'
Assert-Contains $dialog 'HasSavedSearchScope\(\)' 'saved Selection scope stays available while generation is current'
Assert-Contains $dialog '::EnableWindow\(unicode,[\s\S]*?::IsDlgButtonChecked' 'UCP availability follows RegExp without clearing its state'
Assert-Contains $dialog 'class CReplaceDlgBase[\s\S]*?PopulateFindScopes\(\)' 'Replace exposes the same scope list as Find'
Assert-Contains $dialog 'class CReplaceDlgBase[\s\S]*?IDC_FIND_UNICODE_PROPERTIES' 'Replace exposes UCP'
Assert-Contains $dialog 'class CReplaceDlgBase[\s\S]*?IDC_FIND_FROM_START' 'Replace exposes From start'
Assert-Contains $dialog 'class CReplaceDlgBase[\s\S]*?OnScopeChanged[\s\S]*?ResetSearchScope' 'Replace scope changes invalidate only the saved scope range'
Assert-Contains $dialog 'class CReplaceDlgBase[\s\S]*?OnFindFromStart[\s\S]*?DoSearchFromScopeStart' 'Replace From start uses the shared Search Core entry point'
Assert-Contains $dialog 'otherDialogOpen[\s\S]*?IsFindDialogOpen\(\)[\s\S]*?IsReplaceDialogOpen\(\)' 'opening the peer dialog preserves unsaved common search options'
Assert-Contains $pane 'OnListCustomDraw' 'Results pane custom-draw match presentation'
Assert-Contains $pane 'selected \? THEME_COLOR_SELECTION_BACKGROUND : THEME_COLOR_WINDOW' 'selected result context uses the themed selection background'
Assert-Contains $pane 'ThemeManager::Brush\(selected \? THEME_COLOR_SELECTION_BACKGROUND : THEME_COLOR_WINDOW\)' 'each custom-drawn context cell is fully cleared before repaint'
Assert-Contains $pane 'ThemeManager::SelectionTextColor\(\)' 'selected result context uses the themed selection text color'
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
Assert-Contains $source 'm_design_search\.Scope\(query\.Scope\) \? m_design_search\.Scope\(query\.Scope\)->Start : 0' 'From start begins at the current scope boundary'
Assert-Contains $source 'AU::Search::SearchDirection::Forward, &wrapped' 'From start selects the first match without adding a third direction'
Assert-Contains $dialog 'SyncSearchOptionsToOpenDialogs\(this\)' 'common options update the shared model immediately'
Assert-Contains $source 'SyncSearchOptionsToOpenDialogs\(FRBase\* source\)' 'open Find and Replace dialogs synchronize their common controls'

$singleReplace = [regex]::Match($source, 'void\s+CFBEView::DoReplace\(\)\s*\{[\s\S]*?\r?\n}\r?\n\r?\nint\s+CFBEView::ReplaceAllSearchCore').Value
if ([string]::IsNullOrWhiteSpace($singleReplace)) { throw 'Unable to locate native single Replace path.' }
Assert-Contains $singleReplace 'CheckReplacementRange\(sel, m_fo\.fRegexp\) == ReplacementPreflightResult::CrossParagraph[\s\S]*?CrossParagraphReplacementError\(\)[\s\S]*?return;' 'single Replace production preflight rejection'
$singleGuard = $singleReplace.IndexOf('CheckReplacementRange(sel, m_fo.fRegexp)')
$singleUndo = $singleReplace.IndexOf('BeginUndoUnit(L"replace")')
if ($singleGuard -lt 0 -or $singleUndo -lt 0 -or $singleGuard -gt $singleUndo) { throw 'Single Replace must reject a cross-paragraph range before opening Undo.' }
if ($header -notmatch 'MSHTML::IHTMLTxtRangePtr\s+m_selection_search_scope') { throw 'Selection scope must retain a live MSHTML anchor across a single replacement.' }
Assert-Contains $singleReplace 'bool replaced = false;' 'single Replace tracks a successful DOM text mutation'
Assert-Contains $singleReplace 'zeroLengthNoOp = m_fo\.match->Length == 0 && rep\.IsEmpty\(\) && rl\.empty\(\)' 'Design single Replace recognizes zero-length empty no-op'
Assert-Contains $singleReplace 'if \(!zeroLengthNoOp\)[\s\S]*?BeginUndoUnit\(L"replace"\)' 'Design no-op does not open an Undo mutation'
Assert-Contains $singleReplace 'if \(replaced\)[\s\S]*?m_fo\.ClearMatch\(\);[\s\S]*?AdvanceSearchDocumentGeneration\(\);' 'single Replace clears the saved match and invalidates snapshots before the next Find'
Assert-NotContains $singleReplace 'oldLen|newLen|replacementDelta|offsetDelta' 'single Replace must not compensate stale search offsets by text-length deltas'
Assert-Contains $source 'void CFBEView::ResetSearchScope\(\)[\s\S]*?m_selection_search_scope\.Release\(\);' 'resetting search scope releases its live Selection anchor'
Assert-Contains $source 'if \(m_selection_search_scope\)[\s\S]*?scopeSelection = m_selection_search_scope->duplicate\(\);' 'Selection search remaps its live scope after snapshot invalidation'
Assert-Contains $source 'm_design_search\.SetScope\(range, query\.Scope\);[\s\S]*?if \(query\.Scope == AU::Search::SearchScope::Selection\)[\s\S]*?m_selection_search_scope = scopeSelection->duplicate\(\);' 'initial Selection search captures a live scope anchor'

$replaceAll = [regex]::Match($source, 'int\s+CFBEView::ReplaceAllSearchCore\(CString\* errorText\)[\s\S]*?\r?\n}\r?\n\r?\nint\s+CFBEView::GlobalReplace').Value
if ([string]::IsNullOrWhiteSpace($replaceAll)) { throw 'Unable to locate native Replace All path.' }
Assert-Contains $replaceAll 'CheckReplacementRange\(ranges\[index\], m_fo\.fRegexp\) == ReplacementPreflightResult::CrossParagraph[\s\S]*?\*errorText = CrossParagraphReplacementError\(\)[\s\S]*?return -1;' 'Replace All production preflight rejection and error'
$allGuard = $replaceAll.IndexOf('CheckReplacementRange(ranges[index], m_fo.fRegexp)')
$allUndo = $replaceAll.IndexOf('BeginUndoUnit(L"replace all")')
if ($allGuard -lt 0 -or $allUndo -lt 0 -or $allGuard -gt $allUndo) { throw 'Replace All must reject a cross-paragraph range before opening Undo.' }
Assert-Contains $replaceAll 'SetReplaceAllCompletion\(replaced\);[\s\S]*?WM_FINALIZE_REPLACE_ALL_COMPLETION' 'successful Replace All defers completion until queued MSHTML notifications have drained'
Assert-Contains $replaceAll 'if \(!previewIsCurrent\(\)\)[\s\S]*?DoFindAll\(true, &searchError\)' 'first Replace All builds and displays the preview'
Assert-Contains $replaceAll 'MB_YESNO \| MB_ICONQUESTION' 'Replace All has one Yes/No confirmation'
Assert-Contains $replaceAll 'MB_YESNO \| MB_ICONQUESTION\) != IDYES\)\s*return -2;[\s\S]*?if \(!previewIsCurrent\(\)\)' 'preview identity is rechecked after confirmation'
Assert-NotContains $replaceAll 'fbe\.replace\.preview\.ready|MB_OK \| MB_ICONINFORMATION' 'obsolete second-click Replace All information prompt'
Assert-Contains $replaceAll 'MB_YESNO \| MB_ICONQUESTION\) != IDYES\)\s*return -2;[\s\S]*?std::vector<MSHTML::IHTMLTxtRangePtr> ranges' 'No leaves preview intact before any replacement range is opened'
Assert-Contains $replaceAll 'if \(!previewIsCurrent\(\)\)[\s\S]*?return -1;[\s\S]*?std::vector<MSHTML::IHTMLTxtRangePtr> ranges' 'changed preview identity blocks replacement before ranges and Undo'
Assert-Contains $source 'OnFinalizeReplaceAllCompletion[\s\S]*?m_find_results_completion_status = completion;' 'successful replacement clears stale preview rows and reports completion'
Assert-Contains $replaceAll 'bool undoStarted = false;' 'Replace All tracks whether a real mutation opened Undo'
Assert-Contains $replaceAll 'zeroLengthNoOp = result->Hit\.Length == 0 && replacement\.IsEmpty\(\) && formatting\.empty\(\);[\s\S]*?if \(zeroLengthNoOp\)[\s\S]*?continue;' 'Design Replace All skips zero-length empty no-op writes'
Assert-Contains $replaceAll 'if \(!undoStarted\)[\s\S]*?BeginUndoUnit\(L"replace all"\)' 'Replace All opens Undo only for a real write'
Assert-Contains $replaceAll 'SetReplaceAllCompletion\(replaced\);[\s\S]*?PostMessage\(m_hWnd, AU::WM_FINALIZE_REPLACE_ALL_COMPLETION' 'successful Replace All schedules one protected completion phase'

Assert-Contains $source 'OnFinalizeReplaceAllCompletion[\s\S]*?TakeReplaceAllCompletion\(\);[\s\S]*?AdvanceSearchDocumentGeneration\(false\);[\s\S]*?m_find_results_completion_status = completion;[\s\S]*?WM_REFRESH_FIND_RESULTS_PANE' 'posted completion finalizes one invalidation before publishing the Results-pane status'
Assert-Contains $source 'case RANGE_SINK:[\s\S]*?if \(!m_design_search\.ControlledReplaceAllMutation\(\) && !m_design_search\.ReplaceAllCompletionPending\(\)\)\s*AdvanceSearchDocumentGeneration\(\);' 'ordinary RANGE_SINK invalidates searches while controlled and pending Replace All notifications are coalesced'

$globalReplace = [regex]::Match($source, 'int\s+CFBEView::GlobalReplace\(MSHTML::IHTMLElementPtr elem, CString cntTag\)[\s\S]*?\r?\n}\r?\n\r?\nint\s+CFBEView::ToolWordsGlobalReplace').Value
if ([string]::IsNullOrWhiteSpace($globalReplace)) { throw 'Unable to locate Search Core GlobalReplace path.' }
Assert-Contains $globalReplace 'const std::size_t count = m_design_search\.Coordinator\(\)\.GetResults\(\)\.GetCount\(\);\s*if \(count == 0\)\s*return 0;' 'GlobalReplace does not open a no-op Undo unit or invalidate results'
Assert-Contains $globalReplace 'bool mutationApplied = false;' 'GlobalReplace tracks actual DOM writes'
Assert-Contains $globalReplace 'zeroLengthNoOp = result->Hit\.Length == 0 && replacement\.IsEmpty\(\) && formatting\.empty\(\);[\s\S]*?if \(zeroLengthNoOp\)[\s\S]*?continue;' 'Global Replace skips zero-length empty no-op writes'
Assert-Contains $globalReplace 'if \(!undoStarted\)[\s\S]*?BeginUndoUnit\(L"replace"\)' 'GlobalReplace opens Undo only for a real write'
Assert-Contains $globalReplace 'catch \(_com_error& err\)[\s\S]*?if \(mutationApplied\)\s*AdvanceSearchDocumentGeneration\(\);' 'GlobalReplace invalidates partial failed writes only'
Assert-Contains $regexBackend 'return L"\(\?<!\[\\\\p\{L\}\\\\p\{N\}_\]\)\(\?:" \+ pattern' 'whole-word RegExp always adds external Unicode boundaries'
Assert-NotContains $regexBackend 'pattern\.Find\(L"\\\\b"\)|pattern\.Find\(L"\(\?<="\)' 'whole-word RegExp no longer depends on internal boundaries or lookarounds'

Write-Host 'Native Search Core contracts passed.'
