[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$source = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\SearchReplace.h')
$runtime = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\testing\RuntimeTestEditorAndExport.inl')
$resources = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\FBE.rc')
$catalog = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $root 'localization\app-ui\fbe-small-dialogs.json') | ConvertFrom-Json
function Require([string]$pattern, [string]$description) { if ($source -notmatch $pattern) { throw "Missing $description." } }
function RequireRuntime([string]$pattern, [string]$description) { if ($runtime -notmatch $pattern) { throw "Missing $description." } }
Require 'OnTogglePresets[\s\S]*?SetPresetPanelVisible\(!collapse\)[\s\S]*?collapse && _Settings\.SearchTemplatesPanelPinned\(\)[\s\S]*?SetSearchTemplatesPanelPinned\(false, true\)' 'manual collapse pin reset'
Require 'RefreshPresetPanel\(\);[\s\S]*?SetWindowPos[\s\S]*?ResizePresetPanelForCurrentSelection\(\);[\s\S]*?ShowWindow' 'template panel populates and lays out before becoming visible'
Require 'TVM_SETREDRAW|WM_SETREDRAW' 'template tree redraw is suspended during population'
Require 'GetPresetCategoryName' 'built-in templates are grouped by localized category'
foreach ($treeId in @('IDC_FIND_PRESETS_TREE')) {
    if ($resources -notmatch "$treeId.*TVS_HASBUTTONS" -and $resources -notmatch "TVS_HASBUTTONS.*$treeId") { throw 'Preset tree must expose expand/collapse buttons.' }
}
Require 'SearchPresetSafety::ReviewOnly' 'review-only templates have an explicit safety state'
if ($source -match 'for\(std::map<int, HTREEITEM>::const_iterator category.*?TVE_EXPAND') { throw 'Built-in categories must start collapsed.' }
Require 'if\(parent && parent != userRoot\) TreeView_Expand\(tree, parent, TVE_EXPAND\)' 'selected preset category is expanded on restore'
Require 'kPresetTreeRootData = -1' 'distinct root tree-node data'
Require 'kPresetTreeCategoryBase = -100' 'distinct category tree-node data range'
Require 'CategoryTreeData\(FbeSearchPresets::SearchPresetCategory category\)' 'category tree-node encoder'
Require 'IsCategoryTreeData\(LPARAM data\)' 'category tree-node discriminator'
Require 'CategoryFromTreeData\(LPARAM data\)' 'category tree-node decoder'
Require 'std::map<int, bool> expandedCategories' 'category expansion state is retained during panel refresh'
Require 'TVIF_PARAM; categoryInfo\.hItem = categoryItem' 'category identity is read before rebuilding the tree'
Require 'CategoryFromTreeData\(categoryInfo\.lParam\)' 'category state is keyed by category data, not tree position'
Require 'expandedCategories\.find\(category->first\)' 'previously expanded categories are restored by category identity'
Require 'CategoryTreeData\(builtIns\[index\]\.category\)' 'built-in category nodes receive encoded category data'
Require 'preset->builtIn && preset->safety == FbeSearchPresets::SearchPresetSafety::ReviewOnly' 'ReviewOnly warning limited to built-ins'
Require 'item\.lParam < 0' 'Tree root and category nodes cannot resolve to a preset'
Require 'EnableWindow\(GetDlgItem\(IDC_FIND_PRESET_APPLY\), preset != NULL\)' 'Apply is disabled for Tree root and category nodes'
Require 'void ApplySelectedPreset\(\)[\s\S]*?if \(!preset\) return;' 'Enter or double-click on a category cannot apply an arbitrary preset'
$pinStart = $source.IndexOf('LRESULT OnTogglePresetPin')
$pinEnd = $source.IndexOf('LRESULT OnApplyPreset', $pinStart)
$pinHandler = $source.Substring($pinStart, $pinEnd - $pinStart)
if ($pinHandler -notmatch 'SetSearchTemplatesPanelPinned\(pinned, true\)' -or $pinHandler -notmatch 'InvalidateRect') { throw 'Missing pin persistence and visual update.' }
if ($pinHandler -match 'SetPresetPanelVisible') { throw 'Unexpected pin visibility mutation.' }
$visibleStart = $source.IndexOf('void SetPresetPanelVisible')
$visibleEnd = $source.IndexOf('FbeSearchPresets::SearchPreset CurrentPreset', $visibleStart)
$visibilityHandler = $source.Substring($visibleStart, $visibleEnd - $visibleStart)
if ($visibilityHandler -match 'SetSearchTemplatesPanelPinned') { throw 'Unexpected implicit pin reset in visibility setter.' }
Require 'struct PresetPanelMetrics' 'shared preset panel metrics'
Require 'GetPresetPanelMetrics\(int availableHeight = 0\)' 'adaptive metrics function'
Require 'VisiblePresetTreeRowsFrom' 'logical visible tree-row counter'
Require 'const int targetRows = \(std::max\)\(10, visibleRows\)' 'ten-row adaptive tree minimum'
Require 'itemHeight\) \* targetRows' 'tree height based on the actual item height'
Require 'PreviewHeightForCurrentSelection' 'preview height follows selected preset content'
Require 'DT_CALCRECT \| DT_WORDBREAK' 'preview uses wrapped text measurement'
Require 'lineHeight \* 4' 'preview is bounded to four lines'
Require 'LocalizedButtonWidth' 'Apply button measures the localized caption'
Require 'GetTextExtentPoint32W' 'Apply caption measurement uses the button font'
Require 'applyMinimum' 'Apply width has a DPI-aware minimum'
Require 'applyMaximum' 'Apply width has a bounded maximum'
Require 'row2Width = \(std::max\)\(0, \(contentWidth - margin \* 2\) / 3\)' 'equal second-row actions'
Require 'm_presetPanelHeight = GetPresetPanelMetrics\(availablePanelHeight\)\.totalHeight' 'monitor constrained expanded height'
Require 'const HMONITOR monitor = ::MonitorFromWindow\(dialog, MONITOR_DEFAULTTONEAREST\)' 'selection resize monitor lookup'
Require 'const int availablePanelHeight = hasWorkArea' 'selection resize constrained available height'
Require 'const int desiredHeight = GetPresetPanelMetrics\(availablePanelHeight\)\.totalHeight' 'selection resize uses monitor constrained metrics'
if ($source -match 'void ResizePresetPanelForCurrentSelection\(\)[\s\S]{0,600}GetPresetPanelMetrics\(\)\.totalHeight') { throw 'Selection resize must not use an unconstrained panel height.' }
Require 'const int footerHeight' 'preset layout reserves the action-button footer'
Require 'const int minimumTree' 'preset layout has a tree minimum before shrinking preview'
RequireRuntime 'verifyLongPreviewLayout' 'runtime smoke selects a long-regexp fixture'
RequireRuntime 'MonitorFromWindow\(dialog, MONITOR_DEFAULTTONEAREST\)' 'runtime long-preview work-area check'
RequireRuntime 'IDC_FIND_PRESET_APPLY, IDC_FIND_PRESET_SAVE, IDC_FIND_PRESET_UPDATE, IDC_FIND_PRESET_RENAME, IDC_FIND_PRESET_DELETE' 'runtime long-preview checks every footer action'
Require 'UiMetrics::ScaleForDpi\(18, UiMetrics::DpiForWindow\(dialog\)\)' 'DPI-aware compact pin size'
foreach ($control in @('IDC_FIND_PRESET_APPLY','IDC_FIND_PRESET_SAVE','IDC_FIND_PRESET_UPDATE','IDC_FIND_PRESET_RENAME','IDC_FIND_PRESET_DELETE')) { Require ("SetWindowPos\(GetDlgItem\(" + $control + '\)') "layout for $control" }
foreach ($asset in @('src\fbe\res\icons\lucide\pin.svg','src\fbe\res\icons\lucide\pin-mask-16.bmp','src\fbe\res\icons\lucide\pin-mask-20.bmp','src\fbe\res\icons\lucide\pin-mask-24.bmp','src\fbe\res\icons\lucide\pin-mask-32.bmp','src\fbe\res\icons\lucide\LICENSE.txt')) { if (-not (Test-Path (Join-Path $root $asset))) { throw "Missing asset $asset" } }
foreach($resource in @('IDB_FIND_PRESETS_PIN_16 BITMAP','IDB_FIND_PRESETS_PIN_20 BITMAP','IDB_FIND_PRESETS_PIN_24 BITMAP','IDB_FIND_PRESETS_PIN_32 BITMAP')) { if($resources -notlike "*$resource*") { throw "Missing pin mask resource $resource." } }
if ($source -match 'DrawIconEx\(|GetIconInfo\(|IDI_FIND_PRESETS_PIN|IMAGE_ICON|maskPixels\\[index\\]') { throw 'Pin renderer must not recover alpha from an ICO.' }
Require 'PresetPinMaskResource' 'pin bitmap-mask resource selection'
Require 'IMAGE_BITMAP' 'pin bitmap-mask loader'
Require 'GetDIBits' 'pin reads the authored bitmap mask'
Require 'struct BitmapInfo1Bit' 'pin allocates both monochrome palette entries'
Require 'RGBQUAD colors\[2\]' 'pin cannot let GetDIBits overrun a one-entry BITMAPINFO'
Require 'maskInfo.header.biBitCount = 1' 'pin reads monochrome mask coverage'
Require 'maskBits\[' 'pin tints authored mask coverage'
Require 'ThemeManager::AccentColor\(\)' 'pinned accent glyph'
Require 'ThemeManager::SecondaryTextColor\(\)' 'unpinned secondary glyph'
if ($source -match 'IDI_FIND_PRESETS_PIN_OFF') { throw 'Pin-off icon remains a production dependency.' }
Require 'UiMetrics::ScaleForDpi\(16, dpi\)' 'DPI-aware pin glyph rectangle'
Require 'ODS_HOTLIGHT' 'flat pin hover state'
Require 'ODS_SELECTED' 'flat pin pressed state'
Require 'ThemeManager::HoverColor\(\)' 'flat pin hover surface'
Require 'ThemeManager::PressedColor\(\)' 'flat pin selected surface'
if ($source -match 'DrawState|DSS_MONO') { throw 'Pin glyph must not use monochrome DrawState rendering.' }
$pin = $catalog.strings.'fbe.search_preset.pin'.translations
foreach ($language in $catalog.targetLanguages) { if ([string]::IsNullOrWhiteSpace([string]$pin.$language)) { throw "Missing pin tooltip for $language." } }
if ($pin.'en-US' -ne 'Always open the templates panel' -or $pin.'ru-RU' -ne 'Всегда открывать панель шаблонов') { throw 'Pin tooltip semantics are not canonical.' }
Write-Host 'Templates pin, icon, and adaptive layout contract passed.'
