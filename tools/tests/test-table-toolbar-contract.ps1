<#
.SYNOPSIS
Guards table toolbar image-list, UpdateUI and disabled drawing integration.
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
    $bitmapHelper.Value -notmatch 'EnsureToolbarImageListHasMask\(toolbar\)' -or
    $bitmapHelper.Value -notmatch 'ImageList_AddMasked\(toolbar\.GetImageList\(\), colorBitmap, RGB\(192, 192, 192\)\)') {
    throw 'Table toolbar bitmap helper must append a 24x24 bitmap through an ILC_MASK-capable image list and the RGB(192,192,192) mask key.'
}
if ($bitmapHelper.Value -match 'pixel\[[012]\]\s*=\s*0') { throw 'Table toolbar transparency-key pixels must not be rewritten to visible black.' }
if ($cpp -notmatch '(?s)static bool ImageListHasMaskPlane\(.*?ImageList_GetImageInfo.*?hbmMask.*?static bool EnsureToolbarImageListHasMask\(.*?ImageListHasMaskPlane\(imageList\).*?ImageList_Create\(.*?ILC_MASK.*?ImageList_GetIcon.*?TB_SETIMAGELIST.*?existing image indices preserved') {
    throw 'Table toolbar must retain existing image indices when it upgrades an image list to ILC_MASK.'
}
foreach ($forbidden in @('TB_ADDBITMAP', 'ImageList_Replace', 'SetDisabledImageList', 'TB_SETDISABLEDIMAGELIST')) {
    if ($bitmapHelper.Value.Contains($forbidden)) { throw "Table toolbar bitmap helper must not use $forbidden." }
}
if ($bitmapHelper.Value.Contains('maskOneCount')) { throw 'Table toolbar bitmap helper must not retain the redundant mask-one counter.' }

if ($cpp -notmatch '(?s)LRESULT CMainFrame::OnCommandToolbarCustomDraw\(.*?pnmh->hwndFrom != m_CmdToolbar\.m_hWnd.*?IsTableToolbarCommand\(commandId\).*?TB_GETBITMAP.*?DrawThemeParentBackground.*?ILS_SATURATE.*?ImageList_DrawIndirect.*?CDRF_SKIPDEFAULT' -or
    $header -notmatch 'NOTIFY_CODE_HANDLER\(NM_CUSTOMDRAW, OnCommandToolbarCustomDraw\)') {
    throw 'Table toolbar custom draw must grayscale only disabled table icons with ImageList_DrawIndirect.'
}
if ($cpp -notmatch 'ImageList_DrawIndirect\(&draw\) \? CDRF_SKIPDEFAULT : CDRF_DODEFAULT') {
    throw 'Table toolbar custom draw must fall back to native painting when ImageList_DrawIndirect fails.'
}
if ($cpp -notmatch '(?s)GetRuntimeToolbarToolTipText\(UINT commandId\).*?kTableToolbarCommands.*?FbeLoadRuntimeStringByKey\(command\.localizationKey, command\.fallbackText\)' -or
    $cpp -notmatch '(?s)OnRuntimeToolTipTextA.*?GetRuntimeToolbarToolTipText\(static_cast<UINT>\(idCtrl\)\)' -or
    $cpp -notmatch '(?s)OnRuntimeToolTipTextW.*?GetRuntimeToolbarToolTipText\(static_cast<UINT>\(idCtrl\)\)') {
    throw 'Table toolbar tooltips must use the runtime-localized command captions in both ANSI and Unicode notifications.'
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
$disabledBitmapPaths = Get-ChildItem -LiteralPath (Join-Path $repoRoot 'src\fbe\res') -Filter 'table_toolbar_*_disabled.bmp'
if ($disabledBitmapPaths.Count -ne 0) { throw 'Disabled table toolbar bitmaps must not be present; disabled rendering is generated programmatically.' }
foreach ($path in $bitmapPaths) {
    $bytes = [IO.File]::ReadAllBytes($path.FullName)
    if ($bytes.Length -lt 54 -or $bytes[0] -ne [byte][char]'B' -or $bytes[1] -ne [byte][char]'M') { throw "$($path.Name) is not a valid BMP." }
    $width = [BitConverter]::ToInt32($bytes, 18)
    $signedHeight = [BitConverter]::ToInt32($bytes, 22)
    $height = [Math]::Abs($signedHeight)
    if ($width -ne 24 -or $height -ne 24) { throw "$($path.Name) must be 24x24, got ${width}x${height}." }
    $bitCount = [BitConverter]::ToInt16($bytes, 28)
    if ($bitCount -ne 24) { throw "$($path.Name) must be a 24-bpp BMP, got $bitCount bpp." }

    $pixelOffset = [BitConverter]::ToInt32($bytes, 10)
    $rowStride = [int]([Math]::Ceiling(($width * $bitCount) / 32.0) * 4)
    if ($pixelOffset -lt 54 -or $pixelOffset + ($rowStride * $height) -gt $bytes.Length) { throw "$($path.Name) has invalid BMP pixel data." }
    $hasTransparentKey = $false
    for ($y = 0; $y -lt $height -and -not $hasTransparentKey; $y++) {
        $rowStart = $pixelOffset + ($y * $rowStride)
        for ($x = 0; $x -lt $width; $x++) {
            $pixelStart = $rowStart + ($x * 3)
            if ($bytes[$pixelStart] -eq 192 -and $bytes[$pixelStart + 1] -eq 192 -and $bytes[$pixelStart + 2] -eq 192) {
                $hasTransparentKey = $true
                break
            }
        }
    }
    if (-not $hasTransparentKey) { throw "$($path.Name) must contain RGB(192,192,192) toolbar transparency-key pixels." }
}

Write-Host 'Table toolbar native bitmap and UpdateUI contract passed.'
