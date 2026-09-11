<# Guards the native UI-font and fixed 24x24 command-toolbar contract. #>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$metricsHeader = Get-Content -LiteralPath (Join-Path $root 'src\fbe\UiMetrics.h') -Raw
$metricsSource = Get-Content -LiteralPath (Join-Path $root 'src\fbe\UiMetrics.cpp') -Raw
$mainFrame = Get-Content -LiteralPath (Join-Path $root 'src\fbe\mainfrm.cpp') -Raw
$mainFrameHeader = Get-Content -LiteralPath (Join-Path $root 'src\fbe\mainfrm.h') -Raw
$contextControls = Get-Content -LiteralPath (Join-Path $root 'src\fbe\ui\ContextAttributeControls.h') -Raw
$contextControlsSource = Get-Content -LiteralPath (Join-Path $root 'src\fbe\ui\ContextAttributeControls.cpp') -Raw
$contextBars = Get-Content -LiteralPath (Join-Path $root 'src\fbe\ui\ContextAttributeBars.cpp') -Raw
$toolbarFactory = Get-Content -LiteralPath (Join-Path $root 'src\fbe\toolbars\ToolbarFactory.cpp') -Raw

function Require([string]$Text, [string]$Pattern, [string]$Description) {
    if ($Text -notmatch $Pattern) { throw "UiMetrics contract missing: $Description" }
}

Require $metricsHeader 'HFONT\s+DialogFont\s*\(\s*\)' 'DialogFont declaration'
Require $metricsHeader 'HFONT\s+MenuFont\s*\(\s*\)' 'MenuFont declaration'
Require $metricsSource 'SystemParametersInfoForDpi' 'dynamic per-DPI non-client metrics lookup'
Require $metricsSource 'GetProcAddress\([^\r\n]*SystemParametersInfoForDpi' 'Windows 7-safe dynamic API lookup'
Require $metricsSource 'SystemParametersInfoW\(SPI_GETNONCLIENTMETRICS' 'SystemParametersInfoW fallback'
Require $metricsSource 'lfMessageFont' 'message font source'
Require $metricsSource 'lfMenuFont' 'menu font source'
Require $toolbarFactory 'TB_SETBITMAPSIZE[^\r\n]*MAKELONG\(24, 24\)' 'fixed 24x24 command-toolbar bitmap geometry'
Require $toolbarFactory 'TB_SETBUTTONSIZE[^\r\n]*toolbarData->width \+ 7, toolbarData->height \+ 7' 'compact pre-metrics command-toolbar button geometry'
Require $toolbarFactory 'AutoSizeToolbar\(window\)' 'command-toolbar autosize'
Require $mainFrame 'm_MenuBar\.AttachMenu\(GetMenu\(\)\);[\s\S]{0,200}UiMetrics::MenuFont\(\)' 'menu font applied after AttachMenu'
Require $contextBars 'SetDialogFontForToolbarRow\(m_linksBar\)' 'links row receives DialogFont'
Require $contextBars 'SendMessage\(m_linksBar, WM_GETFONT' 'links row obtains its configured font'
Require $contextBars 'SetDialogFontForToolbarRow\(m_tableBar\);' 'first table row receives DialogFont'
Require $contextBars 'SetDialogFontForToolbarRow\(m_tableBar2\);' 'second table row receives DialogFont'
Require $contextControls 'LRESULT\s+OnSetFont\([^\)]*WPARAM wParam[^\)]*BOOL& bHandled\)' 'CCustomStatic WM_SETFONT handler'
Require $contextControlsSource 'm_font\s*=\s*reinterpret_cast<HFONT>\(wParam\)' 'CCustomStatic updates its borrowed font handle'
Require $contextControls 'MESSAGE_HANDLER\(WM_SETFONT, OnSetFont\)' 'CCustomStatic WM_SETFONT message map'
Require $contextControlsSource 'bHandled\s*=\s*FALSE' 'CCustomStatic chains WM_SETFONT to the Static superclass'
Require $contextControlsSource 'SendMessage\(m_hWnd, WM_SETFONT' 'CCustomStatic SetFont uses the WM_SETFONT path'
Require $contextControlsSource 'GetSysColorBrush\(COLOR_BTNFACE\)' 'attribute captions paint an opaque system toolbar background'
Require $mainFrame 'm_contextAttributeBars\.UpdateMetrics\(\)' 'context bars receive centralized DPI/font update'

Write-Host 'UiMetrics and toolbar geometry contract passed.'
