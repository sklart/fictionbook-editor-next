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
Require $barsHeader 'SetMode\s*\(' 'central context-bar visibility update'
Require $barsSource 'TB_DELETEBUTTON' 'context-owned caption-toolbar rebuild'
Require $barsSource 'ShowWindow\(m_linksBar' 'context-owned links-bar visibility'
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

Write-Host 'Context attribute bars boundary contract passed.'
