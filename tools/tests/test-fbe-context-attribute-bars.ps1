<# Guards the UI-only boundary of contextual attribute bars. #>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$barsHeader = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\ui\ContextAttributeBars.h')
$barsSource = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\ui\ContextAttributeBars.cpp')
$controlsHeader = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\ui\ContextAttributeControls.h')
$mainHeader = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\mainfrm.h')
$mainSource = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\mainfrm.cpp')
$mainRuntimeUi = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\ui\MainFrameRuntimeUi.inl')
$project = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\FBE.vcxproj')

function Require([string]$text, [string]$pattern, [string]$description) {
    if($text -notmatch $pattern) { throw "Context attribute bars contract missing: $description" }
}

foreach($forbidden in @('mainfrm\.h', 'FBDoc', 'FB::Doc', 'MSHTML', 'DocumentSession', 'SearchReplace', 'Settings')) {
    if($barsHeader -match $forbidden -or $barsSource -match $forbidden -or $controlsHeader -match $forbidden) {
        throw "Context attribute UI must not depend on $forbidden."
    }
}

Require $barsHeader 'class\s+ContextAttributeBars' 'ContextAttributeBars owner'
Require $barsHeader 'struct\s+LinkAttributeState' 'link UI state'
Require $barsHeader 'struct\s+TableAttributeState' 'table UI state'
Require $barsHeader 'SetLinkState\s*\(' 'link state setter'
Require $barsHeader 'GetLinkState\s*\(' 'link state getter'
Require $barsHeader 'SetTableState\s*\(' 'table state setter'
Require $barsHeader 'GetTableState\s*\(' 'table state getter'
Require $barsHeader 'LinkAttributeAvailability' 'link availability state'
Require $barsHeader 'TableAttributeAvailability' 'table availability state'
Require $barsHeader 'SetLinkAvailability\s*\(' 'link availability setter'
Require $barsHeader 'SetTableAvailability\s*\(' 'table availability setter'
Require $barsHeader 'ApplySelectionState\s*\(' 'selection state application'
Require $barsHeader 'UpdateMetrics\s*\(' 'central metrics update'
Require $barsHeader 'UpdateLocalization\s*\(' 'central localization update'
Require $barsSource 'AttributePairGap' 'context-owned caption/control layout rebuild'
if($barsSource -match '1234567890') { throw 'Context attribute layout must not size fields with digit placeholders.' }
foreach($token in @('ContextAttributeFieldWidth::Short', 'ContextAttributeFieldWidth::Medium', 'ContextAttributeFieldWidth::Long', 'ContextAttributeFieldWidth::Dropdown', 'FieldWidth(', 'CaptionWidth(', 'GetTextExtentPoint32W', 'UiMetrics::ScaleForDpi', 'AddAttributePairSlots')) {
    if($barsSource -notmatch [regex]::Escape($token)) { throw "Context attribute sizing must use $token." }
}
foreach($required in @('m_colspanCaption.*ContextAttributeFieldWidth::Short', 'm_rowspanCaption.*ContextAttributeFieldWidth::Short', 'm_hrefCaption.*ContextAttributeFieldWidth::Long', 'm_imageTitleCaption.*ContextAttributeFieldWidth::Long')) {
    if($barsSource -notmatch $required) { throw "Context attribute field class is missing: $required" }
}
if($barsSource -match 'TBSTYLE_SEP') { throw 'Context attribute spacing must not use painted toolbar separators.' }
Require $barsSource 'AttributePairGap\(' 'context pairs retain a fixed gap helper'
Require $barsSource 'CaptionFieldGap\(' 'captions and fields retain a DPI-aware gap'
Require $barsSource 'info.cxMinChild = \(std::max\)\(1L, child.right - child.left\)' 'rebar preserves the explicit compact child width'
Require $barsSource 'if\(index \+ 1 < count\) left \+= AttributePairGap\(toolbar\)' 'context pair gap is fixed and DPI-aware'
Require $barsSource 'const int fieldLeft = left \+ captionWidth \+ CaptionFieldGap\(toolbar\)' 'caption and field placement uses the gap'
foreach($token in @('CaptionHeight(', 'SetContextRowHeight(', 'TB_SETBUTTONSIZE', 'TB_GETBUTTONSIZE', 'HIWORD(buttonSize)', 'UiMetrics::ScaleForDpi(4', 'NormalizeRebarBands', 'RBBIM_CHILDSIZE', 'info.cyChild = height', 'info.cyMinChild = height', 'info.cyMaxChild = height', 'info.cyIntegral = height')) {
    if($barsSource -notmatch [regex]::Escape($token) -and $barsHeader -notmatch [regex]::Escape($token)) { throw "Context row height contract is missing $token." }
}
Require $mainSource 'UpdateMetrics\(\);[\s\S]{0,220}NormalizeRebarBands\(m_rebar\);[\s\S]{0,120}m_rebar\.SendMessage\(WM_SIZE\);[\s\S]{0,120}UpdateLayout\(\)' 'context metric updates trigger a full main-frame relayout'
Require $barsSource 'g_contextAttributeBoxNativeHeights' 'native closed ComboBox height is retained'
Require $barsSource 'NativeControlHeight\(' 'row sizing uses the retained native height'
Require $barsSource 'height = \(std::max\)\(height, NativeControlHeight' 'row height derives from native control height'
Require $barsSource 'controlTop = \(rowHeight - controlHeight\) / 2' 'controls are vertically centred in their toolbar row'
Require $barsSource 'captionTop = \(rowHeight - captionHeight\) / 2' 'captions are vertically centred in their toolbar row'
if($barsSource -match 'editorRect\.bottom - editorRect\.top, SWP_NOACTIVATE \| SWP_NOZORDER') { throw 'Context controls must not be stretched to the toolbar slot height.' }
if($barsHeader -match 'ContextBarMode|\bSetMode\s*\(|\bMode\s*\(' -or $barsSource -match 'ContextBarMode|\bSetMode\s*\(') { throw 'ContextAttributeBars must not retain test-only visibility modes.' }
Require $controlsHeader 'class\s+CCustomEdit' 'custom edit moved from main frame'
Require $controlsHeader 'class\s+CCustomStatic' 'custom static moved from main frame'
Require $controlsHeader 'class\s+CTableToolbarsWindow' 'toolbar window moved from main frame'
Require $controlsHeader 'WM_PAINT' 'caption painting behavior'
Require $controlsHeader 'WM_SETFONT' 'caption font behavior'
Require $controlsHeader 'IDN_ED_RETURN' 'edit parent notification'
Require $mainHeader 'ContextAttributeBars\s+m_contextAttributeBars' 'single main-frame context UI owner'
if($mainHeader -match 'class\s+CCustomEdit\b|class\s+CCustomStatic\b|class\s+CTableToolbarsWindow\b') { throw 'mainfrm.h must not define context custom controls.' }
if($mainSource -match '(?m)^#define\s+m_(id|href|section|hWndLinksBar|hWndTableBar)') { throw 'mainfrm.cpp must not retain context-control compatibility macros.' }
foreach($accessor in @('IdBox', 'IdEdit', 'IdCaption', 'HrefEdit', 'HrefCaption', 'TableIdBox', 'TableStyleBox', 'CellIdBox', 'CellStyleBox', 'ColspanBox', 'RowspanBox', 'RowAlignBox', 'AlignBox', 'VAlignBox')) {
    if($barsHeader -match "\b$accessor\s*\(") { throw "ContextAttributeBars must not expose raw $accessor access." }
    if($mainSource -match "m_contextAttributeBars\.$accessor\s*\(") { throw "CMainFrame must not access raw $accessor." }
}
foreach($item in @('ui\ContextAttributeBars.cpp', 'ui\ContextAttributeControls.cpp', 'ui\ContextAttributeBars.h', 'ui\ContextAttributeControls.h')) {
    if(-not $project.Contains($item)) { throw "FBE project must include $item." }
}

$catalog = Get-Content -Raw -LiteralPath (Join-Path $root 'localization\app-ui\catalog.json') -Encoding UTF8 | ConvertFrom-Json
$contextKeys = @(
    'fbe.context_attribute.tooltip.id', 'fbe.context_attribute.tooltip.href', 'fbe.context_attribute.tooltip.section_id', 'fbe.context_attribute.tooltip.image_title',
    'fbe.context_attribute.tooltip.table_id', 'fbe.context_attribute.tooltip.table_style', 'fbe.context_attribute.tooltip.cell_id', 'fbe.context_attribute.tooltip.cell_style',
    'fbe.context_attribute.tooltip.colspan', 'fbe.context_attribute.tooltip.rowspan', 'fbe.context_attribute.tooltip.row_align', 'fbe.context_attribute.tooltip.cell_align', 'fbe.context_attribute.tooltip.cell_valign',
    'fbe.context_attribute.align.left', 'fbe.context_attribute.align.right', 'fbe.context_attribute.align.center',
    'fbe.context_attribute.valign.top', 'fbe.context_attribute.valign.middle', 'fbe.context_attribute.valign.bottom'
)
foreach($key in $contextKeys) {
    $entry = $catalog.seedStrings.PSObject.Properties[$key].Value
    if($null -eq $entry) { throw "Context attribute localization key is missing: $key" }
    foreach($language in @($catalog.targetLanguages)) {
        if([string]::IsNullOrWhiteSpace([string]$entry.translations.PSObject.Properties[$language].Value)) { throw "Context attribute localization is missing $language translation for $key" }
    }
}
Require $barsHeader 'class\s+ContextAttributeTooltips' 'context-owned tooltip manager'
Require $barsSource 'TTS_ALWAYSTIP' 'always-on context tooltip control'
Require $barsSource 'TTM_DELTOOLW' 'tooltip rectangles are replaced after layout'
Require $barsSource 'RECT tooltipRect = \{ left, 0, fieldLeft \+ fieldWidth, rowHeight \}' 'tooltip covers the complete caption/control pair'
Require $barsSource 'tooltips\.UpdateArea\(' 'tooltip area is updated during localization/layout'
Require $barsSource 'RebuildTableTokenCatalog\(m_rowAlignBox, kAlignTokens\)' 'row alignment display values are localized'
Require $barsSource 'RebuildTableTokenCatalog\(m_alignBox, kAlignTokens\)' 'cell alignment display values are localized'
Require $barsSource 'RebuildTableTokenCatalog\(m_valignBox, kVAlignTokens\)' 'vertical alignment display values are localized'
Require $barsSource 'SelectedTableToken\(m_rowAlignBox, kAlignTokens\)' 'row alignment returns canonical token'
Require $barsSource 'SelectedTableToken\(m_alignBox, kAlignTokens\)' 'cell alignment returns canonical token'
Require $barsSource 'SelectedTableToken\(m_valignBox, kVAlignTokens\)' 'vertical alignment returns canonical token'
Require $barsSource 'const CString selected = SelectedTableToken\(box, tokens\)' 'language rebuild retains the selected canonical token'
if($barsSource -match 'SelectString\(-1, s\.(rowAlign|align|valign)\)' -or $barsSource -match 'TextOf\(m_(rowAlignBox|alignBox|valignBox)\)') { throw 'Localized ComboBox display text must not be written into FB2 attributes.' }
Require $mainRuntimeUi 'm_contextAttributeBars\.UpdateLocalization\(\)' 'runtime language refresh rebuilds context tooltips and ComboBox display text'

Write-Host 'Context attribute bars boundary contract passed.'
