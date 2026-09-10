<#
.SYNOPSIS
Guards the TBN_GETBUTTONINFO contract for dynamically added toolbar buttons.
#>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$source = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\extras\atlctrlsext.h')
$dialogSource = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\ScriptsToolbarCustomizeDlg.cpp')
$dialogHeader = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\ScriptsToolbarCustomizeDlg.h')
$mainFrame = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\mainfrm.h')
$mainFrameSource = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\mainfrm.cpp')
$resource = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\FBE.rc')

foreach ($required in @(
    'if (!lpTbNotify || lpTbNotify->iItem < 0)',
    'if (lpTbNotify->iItem >= aButtons.GetSize()) return FALSE;',
    'const int textIndex = m_BtnText.FindKey(btn.idCommand);',
    'if (textIndex < 0) return FALSE;',
    'btn.iString = -1;',
    'if (lpTbNotify->pszText && lpTbNotify->cchText > 0)'
)) {
    if ($source.IndexOf($required, [StringComparison]::Ordinal) -lt 0) {
        throw "TBN_GETBUTTONINFO не содержит защиту: $required"
    }
}

if ($source.IndexOf('btn.iString = tb.AddStrings(pstr);', [StringComparison]::Ordinal) -ge 0) {
    throw 'TBN_GETBUTTONINFO не должен изменять строковый пул toolbar во время перечисления кнопок.'
}

foreach ($required in @(
    'CSimpleMap<HWND, TBBUTTONS>',
    'BOOL GetAvailableButtons(HWND hWndToolBar, TBBUTTONS& buttons) const',
    'BOOL GetDefaultButtons(HWND hWndToolBar, TBBUTTONS& buttons) const',
    'BOOL GetCurrentButtons(HWND hWndToolBar, TBBUTTONS& buttons) const'
)) {
    if ($source.IndexOf($required, [StringComparison]::Ordinal) -lt 0) {
        throw "Каталог настраиваемой панели не содержит API: $required"
    }
}

foreach ($required in @(
    'PopulateAvailable(', 'PopulateCurrent(', 'OnReset', 'GetScriptsToolbarCustomizeSize', 'relativePath',
    'ToolbarContainsCommand', 'if(ToolbarContainsCommand(m_available[i].command)) continue;',
    'ToolbarContainsCommand(m_available[item].command)', 'item == kSeparatorItem', 'TBSTYLE_SEP',
    'fbe.scripts_toolbar_customize.separator', 'FbeLoadRuntimeStringByKey', 'm_currentList.SetItemData(row, static_cast<DWORD_PTR>(i));',
    'RefreshLists', 'CenterWindow(GetParent())', 'buttonColumn', 'UpdateButtonState',
    'TB_GETIMAGELIST', 'ImageList_Draw'
)) {
    if ($dialogSource.IndexOf($required, [StringComparison]::Ordinal) -lt 0) {
        throw "Диалог панели скриптов не содержит ожидаемое поведение: $required"
    }
}
if ($mainFrameSource.IndexOf('fbe.hotkey.scripts.last_script', [StringComparison]::Ordinal) -lt 0) {
    throw 'Last script не получает runtime-локализацию при формировании каталога панели.'
}
foreach ($required in @('SetWindowSubclass(m_ScriptsToolbar, ScriptsToolbarSubclassProc', 'message == WM_LBUTTONDBLCLK', 'ShowScriptsToolbarCustomizeDialog()', 'RemoveWindowSubclass(m_ScriptsToolbar, ScriptsToolbarSubclassProc')) {
    if ($mainFrameSource.IndexOf($required, [StringComparison]::Ordinal) -lt 0) {
        throw "Scripts toolbar не перехватывает double-click через безопасный subclass: $required"
    }
}

foreach ($required in @('MESSAGE_HANDLER(WM_CLOSE, OnWindowClose)', 'MESSAGE_HANDLER(WM_DPICHANGED, OnDpiChanged)')) {
    if ($dialogHeader.IndexOf($required, [StringComparison]::Ordinal) -lt 0) {
        throw "Заголовок диалога не обрабатывает системное событие: $required"
    }
}
foreach ($listId in @('IDC_SCRIPTS_TOOLBAR_AVAILABLE', 'IDC_SCRIPTS_TOOLBAR_CURRENT')) {
    if ($resource -notmatch "$listId,[^\r\n]*LBS_EXTENDEDSEL[^\r\n]*LBS_HASSTRINGS[^\r\n]*LBS_OWNERDRAWFIXED[^\r\n]*WS_CLIPSIBLINGS") {
        throw "Owner-draw listbox $listId must retain strings and extended selection."
    }
}
foreach ($required in @('CreateDialogFontForDpi', 'FbeApplyRuntimeDialogLocalization(m_hWnd, IDD)', 'WM_SETREDRAW', 'RefreshLists')) {
    if ($dialogSource.IndexOf($required, [StringComparison]::Ordinal) -lt 0) {
        throw "Диалог не сохраняет требуемое локальное поведение: $required"
    }
}
foreach ($required in @('BeginDeferWindowPos', 'DeferWindowPos', 'EndDeferWindowPos', 'RDW_ALLCHILDREN', 'SaveDC', 'IntersectClipRect', 'RestoreDC', 'ActivateList')) {
    if ($dialogSource.IndexOf($required, [StringComparison]::Ordinal) -lt 0) {
        throw "Диалог не содержит безопасную layout/paint-защиту: $required"
    }
}
foreach ($required in @(
    'SetWindowSubclass(m_currentList, CurrentListSubclassProc', 'WM_LBUTTONDOWN', 'WM_MOUSEMOVE', 'WM_LBUTTONUP',
    'WM_KEYDOWN', 'VK_ESCAPE', 'SM_CXDRAG', 'DrawDragIndicator', 'UpdateDragInsert', 'UpdateDragScroll',
    'WM_TIMER', 'SB_LINEUP', 'SB_LINEDOWN', 'm_dragRows', 'MoveDraggedButtons',
    'destination = insert'
)) {
    if ($dialogSource.IndexOf($required, [StringComparison]::Ordinal) -lt 0) {
        throw "Диалог не содержит ожидаемую реализацию drag-перестановки: $required"
    }
}
if ($dialogSource.IndexOf('UiMetrics::UpdateForWindow(m_hWnd)', [StringComparison]::Ordinal) -ge 0) {
    throw 'Диалог не должен инвалидировать глобальные шрифты UiMetrics главного окна.'
}

if ($mainFrame.IndexOf('m_ScriptsToolbar.Customize()', [StringComparison]::Ordinal) -ge 0) {
    throw 'Scripts toolbar не должна открывать штатный TB_CUSTOMIZE.'
}
if ($mainFrame.IndexOf('ShowScriptsToolbarCustomizeDialog()', [StringComparison]::Ordinal) -lt 0) {
    throw 'Панель скриптов не открывает собственный диалог настройки.'
}
if ($mainFrame.IndexOf('m_selBandID == ATL_IDW_BAND_FIRST+2) ShowScriptsToolbarCustomizeDialog()', [StringComparison]::Ordinal) -lt 0) {
    throw 'Контекстное меню Scripts toolbar не открывает собственный диалог.'
}

Write-Host 'Customizable toolbar TBN_GETBUTTONINFO contract passed.'
