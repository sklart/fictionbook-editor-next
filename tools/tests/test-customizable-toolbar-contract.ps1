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
$adapterHeader = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\toolbars\ToolbarLayoutAdapter.h')
$adapter = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\toolbars\ToolbarLayoutAdapter.cpp')

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
foreach ($required in @('SetWindowSubclass(m_CmdToolbar, ToolbarCustomizeSubclassProc', 'SetWindowSubclass(m_ScriptsToolbar, ToolbarCustomizeSubclassProc', 'message == WM_LBUTTONDBLCLK', 'ShowCommandToolbarCustomizeDialog()', 'ShowScriptsToolbarCustomizeDialog(window)', 'RemoveWindowSubclass(m_CmdToolbar, ToolbarCustomizeSubclassProc', 'RemoveWindowSubclass(m_ScriptsToolbar, ToolbarCustomizeSubclassProc')) {
	if ($mainFrameSource.IndexOf($required, [StringComparison]::Ordinal) -lt 0) {
		throw "Command и Scripts toolbar должны использовать один безопасный double-click subclass: $required"
	}
}

$runtimeUi = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\ui\MainFrameRuntimeUi.inl')
foreach ($required in @('isScriptsToolbar = pnmh->hwndFrom == m_ScriptsToolbar.m_hWnd', 'm_scriptToolbars.Items()[index].window == pnmh->hwndFrom', 'ThemeManager::ControlBrush()', 'ThemeManager::DisabledTextColor()', 'ThemeManager::HoverColor()', 'ThemeManager::PressedColor()', 'ThemeManager::Brush(THEME_COLOR_BORDER)')) {
	if ($runtimeUi.IndexOf($required, [StringComparison]::Ordinal) -lt 0) {
		throw "Scripts toolbar does not use the required dark custom draw: $required"
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
foreach ($required in @('ThemeManager::ApplyToWindow(m_hWnd)', 'ThemeManager::ControlColor()', 'ThemeManager::TextColor()', 'ThemeManager::SelectionBackgroundColor()', 'ThemeManager::SelectionTextColor()', 'ThemeManager::DisabledTextColor()', 'ThemeManager::IsHighContrast()')) {
    if ($dialogSource.IndexOf($required, [StringComparison]::Ordinal) -lt 0) {
        throw "Настройка панели скриптов не использует dark-aware palette: $required"
    }
}
foreach ($required in @('const bool themed = ThemeManager::IsDark() && !ThemeManager::IsHighContrast()',
    'themed ? (selected ? ThemeManager::SelectionBackgroundColor() : ThemeManager::ControlColor())',
    'themed ? (disabled ? ThemeManager::DisabledTextColor()')) {
    if ($dialogSource.IndexOf($required, [StringComparison]::Ordinal) -lt 0) {
        throw "DrawListItem must retain its explicit Dark/Light palette split: $required"
    }
}
foreach ($required in @('BeginDeferWindowPos', 'DeferWindowPos', 'EndDeferWindowPos', 'RDW_ALLCHILDREN', 'SaveDC', 'IntersectClipRect', 'RestoreDC', 'ActivateList')) {
    if ($dialogSource.IndexOf($required, [StringComparison]::Ordinal) -lt 0) {
        throw "Диалог не содержит безопасную layout/paint-защиту: $required"
    }
}
if ($dialogSource.IndexOf('if(other.GetSelCount() > 0) { ::SendMessage(other, LB_SETSEL, FALSE, -1); UpdateButtonState(); }', [StringComparison]::Ordinal) -lt 0) {
    throw 'ActivateList must update button state only after clearing the opposite list.'
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

foreach ($required in @(
    'bool Apply(HWND toolbar', 'TestFailureBeforeDelete', 'TestFailureBeforeAdd', 'TestFailureBeforeRollback', 'SetTestFailurePointForTest'
)) {
    if ($adapterHeader.IndexOf($required, [StringComparison]::Ordinal) -lt 0) {
        throw "ToolbarLayoutAdapter does not expose the checked transactional contract: $required"
    }
}
foreach ($required in @(
    'if(!::IsWindow(toolbar)) return false;', 'CaptureExact(toolbar, previous)', 'if(!target.DeleteButton(0)) return false;',
    'if(!target.AddButtons', 'if(target.GetButtonCount() != static_cast<int>(expected.size())) return false;',
    'actual.iBitmap != expected[index].iBitmap', 'if(!ReplaceChecked(toolbar, original, true))',
    'UiMetrics::DpiForWindow(toolbar)', 'LogicalSeparatorWidth', 'PhysicalSeparatorWidth'
)) {
    if ($adapter.IndexOf($required, [StringComparison]::Ordinal) -lt 0) {
        throw "ToolbarLayoutAdapter misses transactional/DPI guard: $required"
    }
}
if ($dialogSource.IndexOf('ReplaceToolbarButtons', [StringComparison]::Ordinal) -ge 0) {
    throw 'Customize dialog must not retain a second unchecked PortableToolbarItem-to-HWND implementation.'
}
foreach ($required in @('ToolbarLayoutAdapter::Apply(m_toolbar, CurrentItems(), ToolbarCatalog())', 'separator.width = 8', 'DisplayName', 'CompareNoCase', 'unavailable_script', 'VK_DELETE', 'VK_RETURN', 'VK_UP', 'VK_DOWN', 'FocusSearch')) {
    if ($dialogSource.IndexOf($required, [StringComparison]::Ordinal) -lt 0) {
        throw "Customize dialog misses required checked/keyboard behavior: $required"
    }
}

if ($mainFrameSource.IndexOf('m_CmdToolbar.Customize()', [StringComparison]::Ordinal) -ge 0 -or
    $mainFrameSource.IndexOf('m_ScriptsToolbar.Customize()', [StringComparison]::Ordinal) -ge 0) {
    throw 'Пользовательский путь настройки не должен открывать штатный TB_CUSTOMIZE.'
}
foreach ($required in @(
    'void CMainFrame::ShowCommandToolbarCustomizeDialog()',
    'void CMainFrame::ShowScriptsToolbarCustomizeDialog(HWND selectedToolbar)',
    'CScriptsToolbarCustomizeDlg dialog(m_CmdToolbar',
    'CScriptsToolbarCustomizeDlg dialog(selected',
    'ToolbarCustomizeSubclassProc',
    'message == WM_LBUTTONDBLCLK',
    'if(window == frame->m_CmdToolbar) frame->ShowCommandToolbarCustomizeDialog()',
    'else if(window == frame->m_ScriptsToolbar || frame->FindScriptToolbarRuntime(window) != NULL) frame->ShowScriptsToolbarCustomizeDialog(window)',
    'PortableToolbarStore::Save(layout)',
    'ToolbarLayoutAdapter::Capture(m_CmdToolbar, target.items)',
    'UpdateCommandToolbarItems(items)'
)) {
    if ($mainFrameSource.IndexOf($required, [StringComparison]::Ordinal) -lt 0) {
        throw "Общая настройка панелей не содержит '$required'."
    }
}
foreach ($required in @(
    'if (m_selToolbar == m_CmdToolbar) ShowCommandToolbarCustomizeDialog(); else',
    'if (m_selToolbar == m_ScriptsToolbar || FindScriptToolbarRuntime(m_selToolbar) != NULL) ShowScriptsToolbarCustomizeDialog(m_selToolbar);'
)) {
    if ($mainFrame.IndexOf($required, [StringComparison]::Ordinal) -lt 0) {
        throw "Контекстное меню панели не открывает общий FBE-диалог: $required"
    }
}
foreach ($forbidden in @('ToolbarCustomizeThemeProc', 'ToolbarCustomizeCbtProc', 'SetWindowsHookExW(WH_CBT')) {
    if ($mainFrameSource.IndexOf($forbidden, [StringComparison]::Ordinal) -ge 0) {
        throw "Штатный путь настройки toolbar не должен сохранять '$forbidden'."
    }
}
foreach ($required in @('m_showPanelSelector', 'if(!m_showPanelSelector)', 'commands-main', 'm_caption', 'SetWindowText(m_caption)')) {
    if ($dialogSource.IndexOf($required, [StringComparison]::Ordinal) -lt 0 -and $dialogHeader.IndexOf($required, [StringComparison]::Ordinal) -lt 0) {
        throw "Общий FBE-диалог не поддерживает command-toolbar режим: $required"
    }
}
Write-Host 'Customizable toolbar TBN_GETBUTTONINFO contract passed.'
