<#
.SYNOPSIS
Guards the TBN_GETBUTTONINFO contract for dynamically added toolbar buttons.
#>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$source = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\extras\atlctrlsext.h')

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

Write-Host 'Customizable toolbar TBN_GETBUTTONINFO contract passed.'
