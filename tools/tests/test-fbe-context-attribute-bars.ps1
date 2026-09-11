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
Require $barsHeader 'UpdateMetrics\s*\(' 'central metrics update'
Require $barsHeader 'UpdateLocalization\s*\(' 'central localization update'
Require $barsHeader 'SetMode\s*\(' 'central context-bar visibility update'
Require $barsSource 'SetCaptionText\s*\(' 'context-owned caption localization'
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
foreach($item in @('ui\ContextAttributeBars.cpp', 'ui\ContextAttributeControls.cpp', 'ui\ContextAttributeBars.h', 'ui\ContextAttributeControls.h')) {
    if(-not $project.Contains($item)) { throw "FBE project must include $item." }
}

Write-Host 'Context attribute bars boundary contract passed.'
