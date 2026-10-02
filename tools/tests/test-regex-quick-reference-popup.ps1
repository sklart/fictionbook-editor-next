<# Guards the native quick-reference popup integration and its modal-help escape hatch. #>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$dialog = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\SearchReplace.h')
$popup = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\search\ui\RegexQuickReferencePopup.cpp')
$popupHeader = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\search\ui\RegexQuickReferencePopup.h')
$resources = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\resource.h')
foreach($token in @('RegexQuickReferencePopup', 'GetDlgItem(IDC_FIND_REGEX_HELP)', 'RegexQuickReferenceMode::Replacement', 'm_lastRegexTarget', 'm_view->SyncSearchOptionsToOpenDialogs(this)', 'InvalidateSearchSelectionState()', 'ShowRegexHelpDialog')) {
    if($dialog -notmatch [regex]::Escape($token)) { throw "Missing quick-reference dialog behavior: $token" }
}
foreach($token in @('WS_EX_TOOLWINDOW', 'MonitorFromWindow', 'UiMetrics::DpiForWindow', 'UiMetrics::ScaleForDpi', 'VK_ESCAPE', 'VK_RETURN', 'VK_F1', 'VK_LEFT', 'VK_RIGHT', 'm_left', 'm_right', 'AddRows', 'fbe.regex_quick.full_help', 'OnKillFocus', 'AddMessageFilter(this)', 'RemoveMessageFilter(this)', 'WM_LBUTTONDOWN', 'WM_RBUTTONDOWN', 'WM_MBUTTONDOWN', 'WM_NCLBUTTONDOWN', 'UpdateWindow()', 'OnNcDestroy', 'OnThemeChanged', 'LBS_OWNERDRAWFIXED', 'CreateFontW', 'GetTextExtentPoint32', 'm_syntaxColumnWidth', 'description is drawn in its own column')) {
    if($popup -notmatch [regex]::Escape($token)) { throw "Missing quick-reference popup behavior: $token" }
}
foreach($token in @('ThemeManager::ApplyToWindow(m_hWnd)', 'ThemeManager.h', 'ThemeManager::SeparatorColor()', 'CreateSolidBrush', 'FrameRect', 'WM_MOUSEMOVE', 'LB_ITEMFROMPOINT', 'TrackMouseEvent', 'UpdateHoverSelection', 'ClearOtherSelection')) {
    if($popup -notmatch [regex]::Escape($token)) { throw "Missing quick-reference popup theme integration: $token" }
}
foreach($required in @('WM_LBUTTONUP', 'ActivateAtPoint', 'LB_ITEMFROMPOINT', 'ThemeManager::WindowColor()', 'THEME_COLOR_WINDOW')) {
    if($popup -notlike "*$required*") { throw "Quick-reference single-click/surface contract is missing $required." }
}
if($popupHeader -match 'LBN_DBLCLK|OnMouseMove|OnMouseLeave') { throw 'Quick-reference popup retains an obsolete double-click or parent mouse path.' }
if($popup -match 'HWND hwnd = Create\([\s\S]{0,180}WS_POPUP \| WS_BORDER') { throw 'Quick-reference popup must not use the system WS_BORDER.' }
if($popup -notmatch 'rows\[row\] < 0') { throw 'Quick-reference hover must skip section headers.' }
if($popup -notmatch 'ClearOtherSelection\(listWindow\)') { throw 'Quick-reference hover must clear the other column selection.' }
if($popup -match 'displaySyntax \+ L') { throw 'Quick-reference syntax and descriptions must be rendered in separate columns.' }
if ($popup -match 'syntax\.right = syntax\.left \+ \(text\.right - text\.left\) \* 36 / 100') { throw 'Quick-reference syntax column must be measured, not fixed at 36%.' }
foreach ($token in @('VK_UP', 'VK_DOWN', 'VK_HOME', 'VK_END', 'MoveSelection', 'FirstEntryRow')) { if ($popup -notmatch [regex]::Escape($token)) { throw "Missing keyboard entry navigation: $token" } }
if($popupHeader -notmatch 'WM_DRAWITEM' -or $popupHeader -notmatch 'WM_MEASUREITEM') { throw 'Popup must route owner-draw messages.' }
if($popupHeader -notmatch 'public CMessageFilter') { throw 'Popup must be registered as a message filter.' }
if($popup -match 'ShowWindow\(SW_SHOW\)\s*!=\s*FALSE') { throw 'ShowWindow return value must not control popup ownership.' }
if($popup -notmatch 'HWND hwnd = Create\(' -or $popup -notmatch 'if\(hwnd == NULL\) return false;' -or $popup -notmatch 'ShowWindow\(SW_SHOW\);\s*UpdateWindow\(\);\s*return true;') { throw 'Show must transfer ownership only after a valid HWND was created.' }
if($popup -notmatch 'const std::function<void\(\)> callback = m_openFullHelp;\s*DestroyWindow\(\);\s*if\(callback\) callback\(\);') { throw 'Full Help callback must be copied before self-destruction.' }
foreach($id in @('IDC_REGEX_QUICK_LEFT', 'IDC_REGEX_QUICK_RIGHT', 'IDC_REGEX_QUICK_CAPTION', 'IDC_REGEX_QUICK_FULL_HELP')) {
    if($resources -notmatch "#define\s+$id\s+(\d+)") { throw "Missing unique quick-reference control ID: $id" }
    $value = [int]([regex]::Match($resources, "#define\s+$id\s+(\d+)").Groups[1].Value)
    if($value -in @(1, 2, 3, 4, 6, 7)) { throw "Quick-reference control ID collides with a standard dialog ID: $id" }
    if($popup -notmatch [regex]::Escape($id)) { throw "Popup does not use $id" }
}
if($popup -match 'm_caption\.Create[\s\S]*?, 0, [1234]\)' -or $popup -match 'm_fullHelp\.Create[\s\S]*?, 0, [1234]\)') { throw 'Popup caption or Full Help still uses a standard dialog ID.' }
if($popupHeader -notmatch 'IDC_REGEX_QUICK_FULL_HELP, OnFullHelp') { throw 'Full Help control must dispatch through its unique ID.' }
foreach($token in @('ScaleForDpi(560, dpi)', 'DesiredPopupHeight(dpi)', 'workMargin', 'info.rcWork.right - info.rcWork.left', 'info.rcWork.bottom - info.rcWork.top')) {
    if($popup -notmatch [regex]::Escape($token)) { throw "Popup must use the available monitor work area for its expanded geometry: $token" }
}
foreach($token in @('DesiredPopupHeight', 'rowHeight', 'listChrome', 'reserve', 'ThemeManager::Brush(THEME_COLOR_SEPARATOR)', 'WM_PAINT', 'TOOLTIPS_CLASS', 'TTF_IDISHWND | TTF_SUBCLASS', 'TTN_GETDISPINFOW', 'DescriptionIsTruncated', 'TTM_ADDTOOLW', 'displaySyntax), static_cast<LPCWSTR>(description)')) {
    if($popup -notmatch [regex]::Escape($token) -and $popupHeader -notmatch [regex]::Escape($token)) { throw "Missing truncated-description tooltip behavior: $token" }
}
Write-Host 'Regex quick-reference popup contract passed.'
