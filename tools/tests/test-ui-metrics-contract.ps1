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
Require $toolbarFactory 'CommandToolbarImageSize' 'DPI-aware command-toolbar image size helper'
Require $toolbarFactory 'ScaleForDpi\(24, dpi' '24px logical command-toolbar image base'
foreach($dpi in 96,120,144,168,192) { if([Math]::Round(24 * $dpi / 96) -notin 24,30,36,42,48) { throw "Unexpected toolbar size for DPI $dpi" } }
Require $toolbarFactory 'TB_SETBITMAPSIZE[^\r\n]*imageSize' 'DPI-aware command-toolbar bitmap geometry'
Require $toolbarFactory 'TB_SETBUTTONSIZE[^\r\n]*ScaleForDpi\(toolbarData->width \+ 7' 'DPI-aware command-toolbar button geometry'
Require $toolbarFactory 'AutoSizeToolbar\(toolbar\)' 'command-toolbar autosize'
Require $mainFrame 'm_MenuBar\.AttachMenu\(GetMenu\(\)\);[\s\S]{0,600}UiMetrics::MenuFont\(\)' 'menu font applied after AttachMenu'
Require $contextBars 'SetDialogFontForToolbarRow\(m_linksBar\)' 'links row receives DialogFont'
Require $contextBars 'SendMessage\(m_linksBar, WM_GETFONT' 'links row obtains its configured font'
Require $contextBars 'GetTextExtentPoint32W' 'context captions measure localized text'
Require $contextBars 'UiMetrics::ScaleForDpi' 'context fields use DPI-aware width classes'
Require $contextBars 'SetContextRowHeight' 'context rows account for nested ComboBox height'
Require $contextBars 'NativeControlHeight' 'context rows retain the native closed-control height'
Require $contextBars 'MulDiv\(saved->second\.height, static_cast<int>\(UiMetrics::DpiForWindow\(box\)\)' 'native control height scales for the current DPI'

Require $contextBars 'controlHeight = \(std::min\)\(rowHeight, NativeControlHeight' 'context controls retain native height inside the row'
Require $contextBars 'controlTop = \(rowHeight - controlHeight\) / 2' 'context controls use vertical centering'
if($contextBars -match 'TBSTYLE_SEP') { throw 'Context gaps must not be painted toolbar separators.' }
Require $contextBars 'AttributePairGap\(toolbar\)' 'context gaps remain DPI-aware fixed spacing'
Require $contextBars 'TB_SETBUTTONSIZE' 'context rows explicitly set toolbar height'
Require $mainFrame 'NormalizeRebarBands' 'rebar bands follow context child height'
Require $contextBars 'SetDialogFontForToolbarRow\(m_tableBar\);' 'first table row receives DialogFont'
Require $contextBars 'SetDialogFontForToolbarRow\(m_tableBar2\);' 'second table row receives DialogFont'
Require $contextControls 'LRESULT\s+OnSetFont\([^\)]*WPARAM wParam[^\)]*BOOL& bHandled\)' 'CCustomStatic WM_SETFONT handler'
Require $contextControlsSource 'm_font\s*=\s*reinterpret_cast<HFONT>\(wParam\)' 'CCustomStatic updates its borrowed font handle'
Require $contextControls 'MESSAGE_HANDLER\(WM_SETFONT, OnSetFont\)' 'CCustomStatic WM_SETFONT message map'
Require $contextControlsSource 'bHandled\s*=\s*FALSE' 'CCustomStatic chains WM_SETFONT to the Static superclass'
Require $contextControlsSource 'SendMessage\(m_hWnd, WM_SETFONT' 'CCustomStatic SetFont uses the WM_SETFONT path'
Require $contextControlsSource 'GetSysColorBrush\(COLOR_WINDOW\)' 'light captions use the context toolbar surface'
if($contextControlsSource -like '*ControlBrush()*' -and $contextControlsSource -notlike '*if(dark)*ThemeManager::ControlBrush()*') { throw 'Light captions must not use the generic control brush.' }
Require $contextControlsSource 'if\(dark\)[\s\S]{0,100}ThemeManager::ControlBrush\(\)' 'dark captions retain the theme palette'
Require $mainFrame 'm_contextAttributeBars\.UpdateMetrics\(\);[\s\S]{0,220}NormalizeRebarBands\(m_rebar\);[\s\S]{0,120}m_rebar\.SendMessage\(WM_SIZE\);[\s\S]{0,120}UpdateLayout\(\)' 'context band height changes relayout the main frame'
Require $mainFrame 'm_status\.SetFont\(UiMetrics::DialogFont\(\)\)' 'status bar receives the dialog font during initialization and DPI changes'
Require $mainFrame 'StatusBarThemeProc[\s\S]{0,1800}WM_GETFONT' 'custom status painting selects the control font before drawing text'

Write-Host 'UiMetrics and toolbar geometry contract passed.'
