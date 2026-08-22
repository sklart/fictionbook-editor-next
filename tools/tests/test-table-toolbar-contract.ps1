<#
.SYNOPSIS
Guards native bitmap and UpdateUI integration for table toolbar buttons.
#>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$cpp = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\mainfrm.cpp')
$header = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\mainfrm.h')

$bitmapHelper = [regex]::Match($cpp, '(?s)static int AddToolbarBitmapFromModule\(.*?return imageIndex;\s*\}')
if (-not $bitmapHelper.Success -or
    $bitmapHelper.Value -notmatch 'LoadImage\(module, MAKEINTRESOURCE\(bitmapResourceId\)' -or
    $bitmapHelper.Value -notmatch 'toolbar\.GetImageList\(\)' -or
	$bitmapHelper.Value -notmatch 'DIBSECTION bitmapSection' -or
    $bitmapHelper.Value -notmatch 'CreateBitmap\(24, 24, 1, 1, NULL\)' -or
	$bitmapHelper.Value -notmatch 'pixel\[0\] == 192 && pixel\[1\] == 192 && pixel\[2\] == 192' -or
	$bitmapHelper.Value -notmatch 'pixel\[0\] = 0;' -or
	$bitmapHelper.Value -notmatch 'SetPixel\(maskDc, x, y, transparent \? RGB\(255, 255, 255\) : RGB\(0, 0, 0\)\)' -or
    $bitmapHelper.Value -notmatch 'ImageList_Add\(imageList, colorBitmap, maskBitmap\)') {
    throw 'Table toolbar bitmap helper must append a normalized 24x24 color bitmap with an explicit 1-bpp RGB(192,192,192) mask.'
}
foreach ($forbidden in @('TB_ADDBITMAP', 'ImageList_AddMasked', 'ImageList_Replace', 'TB_SETIMAGELIST', 'SetImageList')) {
    if ($bitmapHelper.Value.Contains($forbidden)) { throw "Table toolbar bitmap helper must not use $forbidden." }
}

if ($cpp -notmatch '(?s)LRESULT CMainFrame::OnCommandToolbarCustomDraw\(.*?pnmh->hwndFrom != m_CmdToolbar\.m_hWnd.*?IsTableToolbarCommand\(commandId\).*?TB_GETBITMAP.*?DrawThemeParentBackground.*?ILS_SATURATE.*?ImageList_DrawIndirect.*?CDRF_SKIPDEFAULT' -or
    $header -notmatch 'NOTIFY_CODE_HANDLER\(NM_CUSTOMDRAW, OnCommandToolbarCustomDraw\)') {
    throw 'Table toolbar custom draw must grayscale only disabled table icons with ImageList_DrawIndirect.'
}
foreach ($forbidden in @('TBCDRF_BLENDICON', 'TBCDRF_NOETCHEDEFFECT', 'DrawState', 'SetDisabledImageList', 'TB_SETDISABLEDIMAGELIST')) {
    if ($cpp.Contains($forbidden) -or $header.Contains($forbidden)) { throw "Forbidden table toolbar drawing workaround remains: $forbidden." }
}

$tableCommands = @(
    'ID_TABLE_INSERT_ROW_ABOVE', 'ID_TABLE_INSERT_ROW_BELOW', 'ID_TABLE_DELETE_ROW',
    'ID_TABLE_INSERT_COLUMN_LEFT', 'ID_TABLE_INSERT_COLUMN_RIGHT', 'ID_TABLE_DELETE_COLUMN',
    'ID_TABLE_MAKE_HEADER_CELLS', 'ID_TABLE_MAKE_NORMAL_CELLS'
)
foreach ($command in $tableCommands) {
    if ($header -notmatch "UPDATE_ELEMENT\($command,\s*UPDUI_MENUPOPUP\|UPDUI_TOOLBAR\)") {
        throw "$command must be registered as both menu popup and toolbar UI."
    }
}
if ($cpp -match 'm_CmdToolbar\.EnableButton\(tableCommands\[') { throw 'Table buttons must use CUpdateUI instead of manual EnableButton.' }

foreach ($required in @('TBIF_IMAGE', 'm_table_toolbar_image_indices[index]', 'DeploymentContext::RegistryPersistenceAllowed()', 'm_CmdToolbar.RestoreState', 'm_CmdToolbar.SaveState')) {
    if (-not $cpp.Contains($required)) { throw "Missing table toolbar persistence contract: $required" }
}

$bitmapPaths = Get-ChildItem -LiteralPath (Join-Path $repoRoot 'src\fbe\res') -Filter 'table_toolbar_*.bmp'
if ($bitmapPaths.Count -ne 8) { throw "Expected 8 table toolbar bitmaps, found $($bitmapPaths.Count)." }
foreach ($path in $bitmapPaths) {
    $bytes = [IO.File]::ReadAllBytes($path.FullName)
    if ($bytes.Length -lt 26) { throw "$($path.Name) is not a valid bitmap." }
    $width = [BitConverter]::ToInt32($bytes, 18)
    $height = [Math]::Abs([BitConverter]::ToInt32($bytes, 22))
    if ($width -ne 24 -or $height -ne 24) { throw "$($path.Name) must be 24x24, got ${width}x${height}." }
}

Write-Host 'Table toolbar native bitmap and UpdateUI contract passed.'
