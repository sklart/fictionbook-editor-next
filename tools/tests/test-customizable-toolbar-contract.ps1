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
$mainFrame = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\mainfrm.h')

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

foreach ($required in @('PopulateAvailable()', 'OnReset', 'GetScriptsToolbarCustomizeSize', 'relativePath', 'TBSTYLE_SEP')) {
    if ($dialogSource.IndexOf($required, [StringComparison]::Ordinal) -lt 0) {
        throw "Диалог панели скриптов не содержит ожидаемое поведение: $required"
    }
}

if ($mainFrame.IndexOf('m_ScriptsToolbar.Customize()', [StringComparison]::Ordinal) -ge 0) {
    throw 'Scripts toolbar не должна открывать штатный TB_CUSTOMIZE.'
}
if ($mainFrame.IndexOf('ShowScriptsToolbarCustomizeDialog()', [StringComparison]::Ordinal) -lt 0) {
    throw 'Панель скриптов не открывает собственный диалог настройки.'
}

Write-Host 'Customizable toolbar TBN_GETBUTTONINFO contract passed.'
