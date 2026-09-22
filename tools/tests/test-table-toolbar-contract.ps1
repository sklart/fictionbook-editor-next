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
$runtimeUi = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\ui\MainFrameRuntimeUi.inl')
$factory = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\toolbars\ToolbarFactory.cpp')
$tableCatalog = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\toolbars\TableToolbarCommands.cpp')

$selectControls = [regex]::Match($cpp, 'LRESULT\s+CMainFrame::OnSelectCtl\([\s\S]*?\r?\n}\r?\n\r?\nLRESULT\s+CMainFrame::OnNextItem').Value
if ([string]::IsNullOrWhiteSpace($selectControls)) { throw 'Unable to locate attribute-band selection handler.' }
foreach ($tableControl in @('ID_SELECT_IDT', 'ID_SELECT_STYLET', 'ID_SELECT_STYLE')) {
    if ($selectControls -notmatch "(?s)case\s+$tableControl\s*:.*?ATL_IDW_BAND_FIRST\s*\+\s*4") {
        throw "$tableControl must reveal the first table attribute band (+4), not the links/image band."
    }
}

$bitmapHelper = [regex]::Match($factory, '(?s)int ToolbarFactory::AddBitmapFromModule\(.*?return imageIndex;\s*\}')
if (-not $bitmapHelper.Success -or
    $bitmapHelper.Value -notmatch 'LoadImage\(module, MAKEINTRESOURCE\(bitmapResourceId\)' -or
    $bitmapHelper.Value -notmatch 'toolbar\.GetImageList\(\)' -or
	$bitmapHelper.Value -notmatch 'CreateAlphaBitmap\(source, 24, 24\)' -or
    $bitmapHelper.Value -notmatch 'ImageList_Add\(toolbar\.GetImageList\(\), alpha, NULL\)') {
    throw 'Table toolbar bitmap helper must append a 24x24 alpha bitmap through the owned image list.'
}
if ($factory -notmatch '(?s)HBITMAP ToolbarFactory::CreateAlphaBitmap\(.*?biBitCount = 32.*?keyBlue = 0xFF.*?keyGreen = 0x00.*?keyRed = 0xFF.*?blue == keyBlue && green == keyGreen && red == keyRed \? 0.*?return target;') {
	throw 'Table toolbar bitmap conversion must create 32-bit alpha pixels using only the exact magenta transparency key.'
}
foreach ($forbidden in @('connectedCanvas', 'edge-connected', 'pending.Enqueue', 'maximum =', 'minimum =')) {
	if ($factory.Contains($forbidden)) { throw "Table toolbar alpha conversion must not use a brightness or flood-fill heuristic: $forbidden" }
}
if ($factory -notmatch '(?s)HWND ToolbarFactory::CreateCommandToolbarCtrl\(.*?FindResource\(.*?RT_TOOLBAR.*?ownedImages\.Create\(24, 24, ILC_COLOR32 \| ILC_MASK.*?ImageList_LoadImage\(.*?CopyToolbarImages\(ownedImages, sourceImages, standardImageCount\).*?TB_SETIMAGELIST.*?TB_ADDBUTTONS') {
    throw 'Command toolbar must create one application-owned ILC_COLOR32|ILC_MASK image list from the RT_TOOLBAR strip before adding buttons.'
}
if ($cpp -match 'EnsureToolbarImageListHasMask') {
    throw 'Delayed command-toolbar image-list reconstruction must not remain.'
}
if ($header -notmatch 'CImageList\s+m_commandToolbarImages') {
    throw 'CMainFrame must explicitly own the command toolbar image list.'
}
if ($cpp -notmatch '(?s)m_CmdToolbar = ToolbarFactory::CreateCommandToolbarCtrl\(m_hWnd, m_commandToolbarImages, IDR_MAINFRAME.*?InitToolBar\(m_CmdToolbar, IDR_MAINFRAME\)') {
    throw 'The owned image list must be installed during command toolbar creation while InitToolBar retains customization metadata.'
}
if ($cpp -notmatch '(?s)LRESULT CMainFrame::OnDestroy\(.*?m_CmdToolbar\.SetImageList\(NULL\).*?m_commandToolbarImages\.Destroy\(\)') {
    throw 'OnDestroy must detach the owned image list before destroying it.'
}
foreach ($forbidden in @('TB_ADDBITMAP', 'ImageList_Replace', 'SetDisabledImageList', 'TB_SETDISABLEDIMAGELIST')) {
    if ($cpp.Contains($forbidden)) { throw "Command toolbar implementation must not use $forbidden." }
}
if ($bitmapHelper.Value.Contains('maskOneCount')) { throw 'Table toolbar bitmap helper must not retain the redundant mask-one counter.' }

if ($runtimeUi -notmatch '(?s)LRESULT CMainFrame::OnCommandToolbarCustomDraw\(.*?isCommandToolbar\s*=\s*pnmh->hwndFrom == m_CmdToolbar\.m_hWnd.*?isScriptsToolbar\s*=\s*pnmh->hwndFrom == m_ScriptsToolbar\.m_hWnd.*?!isCommandToolbar && !isScriptsToolbar && !isContextAttributeBar.*?isCommandToolbar && IsTableToolbarCommand\(commandId\).*?disabled.*?TB_GETBITMAP.*?DrawThemeParentBackground.*?ILS_SATURATE.*?ImageList_DrawIndirect.*?CDRF_SKIPDEFAULT' -or
    $header -notmatch 'NOTIFY_CODE_HANDLER\(NM_CUSTOMDRAW, OnCommandToolbarCustomDraw\)') {
    throw 'Table toolbar custom draw must grayscale only disabled table icons with ImageList_DrawIndirect.'
}
if ($runtimeUi -notmatch 'ImageList_DrawIndirect\(&draw\) \? CDRF_SKIPDEFAULT : CDRF_DODEFAULT') {
    throw 'Table toolbar custom draw must fall back to native painting when ImageList_DrawIndirect fails.'
}
if ($cpp -notmatch '(?s)GetRuntimeToolbarToolTipText\(UINT commandId\).*?kTableToolbarCommands.*?FbeLoadRuntimeStringByKey\(command\.localizationKey, command\.fallbackText\)' -or
    $runtimeUi -notmatch '(?s)OnRuntimeToolTipTextA.*?GetRuntimeToolbarToolTipText\(static_cast<UINT>\(idCtrl\)\)' -or
    $runtimeUi -notmatch '(?s)OnRuntimeToolTipTextW.*?GetRuntimeToolbarToolTipText\(static_cast<UINT>\(idCtrl\)\)') {
    throw 'Table toolbar tooltips must use the runtime-localized command captions in both ANSI and Unicode notifications.'
}
if ($cpp -notmatch '(?s)GetRuntimeToolbarToolTipText\(UINT commandId\).*?FindRuntimeMainFrameMenuCommandKey\(commandId\).*?FbeLoadRuntimeStringByKey\(key, fallback\).*?StripMenuMnemonics') {
    throw 'Regular command-toolbar tooltips must use the same runtime-localized menu key instead of an English string-table fallback.'
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

$resourceDirectory = Join-Path $repoRoot 'src\fbe\res'
$toolbarBitmapPaths = Get-ChildItem -LiteralPath $resourceDirectory -Filter 'table_toolbar_*.bmp'
$menuBitmapPaths = @('insert_row_above.bmp', 'insert_row_below.bmp', 'delete_row.bmp', 'insert_column_left.bmp',
	'insert_column_right.bmp', 'delete_column.bmp', 'make_header_cells.bmp', 'make_normal_cells.bmp') |
	ForEach-Object { Get-Item -LiteralPath (Join-Path $resourceDirectory $_) }
if ($toolbarBitmapPaths.Count -ne 8) { throw "Expected 8 table toolbar bitmaps, found $($toolbarBitmapPaths.Count)." }
if ($menuBitmapPaths.Count -ne 8) { throw "Expected 8 table menu bitmaps, found $($menuBitmapPaths.Count)." }
$disabledBitmapPaths = Get-ChildItem -LiteralPath $resourceDirectory -Filter 'table_toolbar_*_disabled.bmp'
if ($disabledBitmapPaths.Count -ne 0) { throw 'Disabled table toolbar bitmaps must not be present; disabled rendering is generated programmatically.' }
$operationColors = @{
	'insert_row_above.bmp' = @(45, 170, 100)
	'insert_row_below.bmp' = @(45, 170, 100)
	'delete_row.bmp' = @(205, 70, 70)
	'insert_column_left.bmp' = @(45, 170, 100)
	'insert_column_right.bmp' = @(45, 170, 100)
	'delete_column.bmp' = @(205, 70, 70)
	'make_header_cells.bmp' = @(65, 130, 190)
	'make_normal_cells.bmp' = @(110, 165, 195)
}
$neutralColors = @('220,225,230', '195,205,215')
$gridColor = '70,78,88'

function Assert-TableBitmapSet([System.IO.FileInfo[]]$paths, [int]$expectedSize, [string]$setName) {
foreach ($path in $paths) {
    $bytes = [IO.File]::ReadAllBytes($path.FullName)
    if ($bytes.Length -lt 54 -or $bytes[0] -ne [byte][char]'B' -or $bytes[1] -ne [byte][char]'M') { throw "$($path.Name) is not a valid BMP." }
    $width = [BitConverter]::ToInt32($bytes, 18)
    $signedHeight = [BitConverter]::ToInt32($bytes, 22)
    $height = [Math]::Abs($signedHeight)
    if ($width -ne $expectedSize -or $height -ne $expectedSize) { throw "$($path.Name) must be ${expectedSize}x${expectedSize}, got ${width}x${height}." }
    $bitCount = [BitConverter]::ToInt16($bytes, 28)
    if ($bitCount -ne 24) { throw "$($path.Name) must be a 24-bpp BMP, got $bitCount bpp." }

    $pixelOffset = [BitConverter]::ToInt32($bytes, 10)
    $rowStride = [int]([Math]::Ceiling(($width * $bitCount) / 32.0) * 4)
    if ($pixelOffset -lt 54 -or $pixelOffset + ($rowStride * $height) -gt $bytes.Length) { throw "$($path.Name) has invalid BMP pixel data." }
	$keyPixels = 0
	$opaquePixels = 0
	$neutralPixels = 0
	$gridXs = [System.Collections.Generic.HashSet[int]]::new()
	$gridYs = [System.Collections.Generic.HashSet[int]]::new()
	$operationPixels = 0
	$operationName = $path.Name -replace '^table_toolbar_'
	$operation = ($operationColors[$operationName] -join ',')
	for ($y = 0; $y -lt $height; $y++) {
        $rowStart = $pixelOffset + ($y * $rowStride)
        for ($x = 0; $x -lt $width; $x++) {
            $pixelStart = $rowStart + ($x * 3)
			$color = "$($bytes[$pixelStart + 2]),$($bytes[$pixelStart + 1]),$($bytes[$pixelStart])"
			if ($color -eq '255,0,255') { ++$keyPixels; continue }
			++$opaquePixels # CreateAlphaBitmap assigns alpha=255 to every non-key pixel.
			if ($neutralColors -contains $color) { ++$neutralPixels }
			if ($color -eq $gridColor) { [void]$gridXs.Add($x); [void]$gridYs.Add($y) }
			if ($color -eq $operation) { ++$operationPixels }
        }
    }
	if ($keyPixels -lt ($expectedSize * 3) -or $opaquePixels -lt ($expectedSize * 3)) { throw "$setName $($path.Name) must use magenta only as a surrounding transparency key, not as its glyph." }
	if ($neutralPixels -lt 18) { throw "$setName $($path.Name) must retain opaque neutral table cells after alpha conversion." }
	if ($gridXs.Count -lt 4 -or $gridYs.Count -lt 4) { throw "$($path.Name) is not a full table grid; a colored strip is insufficient." }
	if ($operationPixels -lt 8) { throw "$($path.Name) must visibly distinguish its table operation with the assigned colour." }
}
}
Assert-TableBitmapSet $toolbarBitmapPaths 24 'toolbar'
Assert-TableBitmapSet $menuBitmapPaths 16 'menu'

Write-Host 'Table toolbar native bitmap and UpdateUI contract passed.'
