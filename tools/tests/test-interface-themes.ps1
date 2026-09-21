param([string]$RepositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path)

$ErrorActionPreference = 'Stop'
function Read-ProjectFile([string]$relativePath) {
    Get-Content -Raw -LiteralPath (Join-Path $RepositoryRoot $relativePath)
}

$manager = Read-ProjectFile 'src\fbe\ThemeManager.cpp'
$managerHeader = Read-ProjectFile 'src\fbe\ThemeManager.h'
$settings = Read-ProjectFile 'src\fbe\Settings.cpp'
$settingsDialog = Read-ProjectFile 'src\fbe\settings\ui\SettingsDlg.cpp'
$serialization = Read-ProjectFile 'src\fbe\settings\SettingsSerialization.cpp'
$generalPage = Read-ProjectFile 'src\fbe\settings\ui\SettingsGeneralPage.cpp'
$mainFrame = Read-ProjectFile 'src\fbe\mainfrm.cpp'
$toolbarUi = Read-ProjectFile 'src\fbe\ui\MainFrameRuntimeUi.inl'
$mainFrameHeader = Read-ProjectFile 'src\fbe\mainfrm.h'
$documentTree = Read-ProjectFile 'src\fbe\DocumentTree.cpp'
$documentTreeHeader = Read-ProjectFile 'src\fbe\DocumentTree.h'
$contextAttributeBars = Read-ProjectFile 'src\fbe\ui\ContextAttributeBars.cpp'
$contextAttributeControls = Read-ProjectFile 'src\fbe\ui\ContextAttributeControls.cpp'
$modelessDialog = Read-ProjectFile 'src\fbe\ModelessDialog.h'
$aboutBox = Read-ProjectFile 'src\fbe\AboutBox.cpp'
$scriptVisuals = Read-ProjectFile 'src\fbe\scripts\ScriptVisualResources.cpp'
$colorButton = Read-ProjectFile 'src\fbe\extras\ColorButton.cpp'

foreach($required in @('AppsUseLightTheme', 'g_highContrast', 'highContrastChanged', 'WH_CBT', 'HCBT_ACTIVATE', 'DwmSetWindowAttribute', 'SetWindowTheme', 'EnumThreadWindows', 'WM_FBE_THEMECHANGED')) {
    if($manager -notlike "*$required*") { throw "ThemeManager.cpp does not provide $required." }
}
foreach($required in @('THEME_COLOR_BORDER', 'THEME_COLOR_SEPARATOR', 'THEME_COLOR_SECONDARY_TEXT', 'THEME_COLOR_DISABLED_TEXT', 'THEME_COLOR_SELECTION_BACKGROUND', 'THEME_COLOR_SELECTION_TEXT', 'THEME_COLOR_HOVER', 'THEME_COLOR_PRESSED', 'THEME_COLOR_FOCUS', 'THEME_COLOR_ACCENT', 'THEME_COLOR_ERROR', 'THEME_COLOR_WARNING', 'THEME_COLOR_SUCCESS')) {
    if($managerHeader -notlike "*$required*") { throw "ThemeManager.h does not expose semantic colour $required." }
}
foreach($required in @('SetWindowSubclass', 'BS_GROUPBOX', 'BS_AUTORADIOBUTTON', 'IsRadioButton', 'HasClientEdge', 'PaintDarkClientEdge', 'WM_NCPAINT', 'PaintDarkGroupBox', 'TOOLBARCLASSNAMEW', 'TB_SETCOLORSCHEME', 'WM_CTLCOLORSTATIC', 'WM_CTLCOLORBTN', 'WM_CTLCOLOREDIT', 'WM_CTLCOLORLISTBOX', 'WC_COMBOBOXW', 'WC_COMBOBOXEXW', 'DarkMode_CFD', 'CBN_DROPDOWN', 'GetComboBoxInfo', 'hwndList', 'WC_TREEVIEWW', 'TVM_SETLINECOLOR', 'WC_LISTVIEWW', 'LVM_GETHEADER', 'WC_HEADERW', 'PaintDarkHeader', 'WM_MOUSELEAVE', 'HDF_SORTUP', 'HDF_SORTDOWN', 'WC_TABCONTROLW', 'STATUSCLASSNAMEW', 'REBARCLASSNAMEW', 'EM_SETBKGNDCOLOR')) {
    if($manager -notlike "*$required*") { throw "ThemeManager.cpp does not apply the semantic palette to $required." }
}
if($managerHeader -notlike '*INTERFACE_THEME_AUTOMATIC*' -or $managerHeader -notlike '*WindowBrush*') {
    throw 'Theme manager must expose Automatic and shared colour resources.'
}
if($serialization -notlike '*InterfaceTheme*' -or $serialization -notlike '*ThemeManager::SetSelectedTheme*') {
    throw 'Interface theme setting is not persisted and restored.'
}
if($generalPage -notlike '*IDC_INTERFACE_THEME*' -or $generalPage -notlike '*Automatic \x2014 follow Windows*') {
    throw 'General settings page does not expose Interface theme.'
}
if($settings -notlike '*ThemeManager::IsDark()*') { throw 'Source Automatic must follow effective FBE theme.' }
if($settings -like '*GetXmlSrcThemeColor*AppsUseLightTheme*') { throw 'Source Automatic must not read Windows theme directly.' }
if($mainFrame -notlike '*ThemeManager::RefreshSystemTheme()*' -or $mainFrame -notlike '*OnThemeChanged*') {
    throw 'Main frame does not dynamically refresh theme changes.'
}
if($mainFrame -notlike '*OnPostCreate*' -or $mainFrame -notlike '*ThemeManager::ApplyToWindow(m_hWnd);*') {
    throw 'Main frame does not reapply its DWM theme after creation has completed.'
}
foreach($required in @('WM_INITMENUPOPUP', 'native dark popup', 'CCommandBarCtrl::TrackPopupMenu', 'ThemeManager::TrackPopupMenu')) {
	if($mainFrame -notlike "*$required*") { throw "Native dark menus do not retain command-bar icons: $required." }
}
foreach($required in @('ApplyScriptNativeMenuBitmaps', 'g_scriptNativeMenuBitmaps', 'NativeMenuBitmap')) {
	if($mainFrame -notlike "*$required*") { throw "Native dark menus do not restore script bitmaps: $required." }
}
foreach($required in @('CreateMenuBitmap', 'ImageList_DrawIndirect', 'ILC_COLOR32')) {
	if($scriptVisuals -notlike "*$required*") { throw "Script visual resources do not create native alpha menu bitmaps: $required." }
}
foreach($required in @('useDarkPalette', 'ThemeManager::ControlColor()', 'ThemeManager::TextColor()', 'ThemeManager::SelectionBackgroundColor()', 'ThemeManager::HoverColor()', 'ThemeManager::BorderColor()', 'wc .hbrBackground = NULL')) {
	if($colorButton -notlike "*$required*") { throw "Color picker popup does not follow the interface theme: $required." }
}
foreach($required in @('TrackPopupMenu', 'TaskDialogIndirect', 'TDN_CREATED', 'ThemedTaskDialogCallback', 'ApplyToWindow(window)')) {
	if($manager -notlike "*$required*") { throw "Theme manager does not provide the native popup and TaskDialog wrappers: $required." }
}
if($mainFrameHeader -notlike '*ThemeManager::TrackPopupMenu(tp->hMenu*') {
	throw 'BODY context menu does not use the shared native popup helper.'
}
if($scriptVisuals -notlike '*CreateMenuBitmap*') { throw 'Script visual resources no longer provide native menu bitmaps.' }
foreach($required in @('pagesToTheme', 'ThemeManager::ApplyToWindow(page)', 'before any page becomes visible')) {
	if($settingsDialog -notlike "*$required*") { throw "Settings pages do not receive the selected theme on creation: $required." }
}
foreach($dialogSource in @($modelessDialog, $aboutBox)) {
    if($dialogSource -notlike '*ThemeManager::ApplyToWindow(m_hWnd)*') { throw 'A top-level dialog does not apply the theme after creating its HWND.' }
}
foreach($required in @('ApplyTheme()', 'ContextAttributeBarThemeProc', 'WM_CTLCOLORSTATIC', 'WM_CTLCOLOREDIT', 'WM_CTLCOLORLISTBOX', 'TB_SETCOLORSCHEME', 'CCM_SETBKCOLOR', 'ThemeManager::ControlBrush()', 'ThemeManager::DisabledTextColor()')) {
	if($contextAttributeBars -notlike "*$required*") { throw "Context attribute bars do not apply the theme palette: $required." }
}
foreach($required in @('ThemeManager::ControlBrush()', 'ThemeManager::TextColor()', 'ThemeManager::DisabledTextColor()', 'IsWindowEnabled(m_hWnd)')) {
	if($contextAttributeControls -notlike "*$required*") { throw "Context attribute captions do not apply the theme palette: $required." }
}
foreach($required in @('m_contextAttributeBars.ApplyTheme()', 'ApplyContextAttributeRebarBandTheme', 'RBBIM_COLORS', 'ThemeManager::ControlColor()')) {
	if($mainFrame -notlike "*$required*") { throw "Main frame does not refresh context attribute bar bands: $required." }
}
foreach($required in @('CThemedSplitterWindow', 'CThemedHorSplitterWindow', 'THEME_COLOR_SEPARATOR', 'THEME_COLOR_BORDER')) {
    if($mainFrameHeader -notlike "*$required*") { throw "Main frame does not theme splitter separator $required." }
}
foreach($required in @('isContextAttributeBar', 'ThemeManager::ControlBrush()', 'ThemeManager::DisabledTextColor()', 'ThemeManager::HoverColor()', 'ThemeManager::SelectionTextColor()', 'ThemeManager::ControlColor()', 'ILD_BLEND50', 'CDDS_ITEMPOSTPAINT')) {
    if($toolbarUi -notlike "*$required*") { throw "Toolbar custom draw does not apply semantic colour $required." }
}
foreach($required in @('FlushMenuThemesFn', 'MAKEINTRESOURCEA(136)', 'ForceDark', 'UsesClassicSurfacePalette', 'SetWindowTheme(window, L" ", L" ")', 'ApplyNativeControlPalette(window);')) {
    if($manager -notlike "*$required*") { throw "Theme manager does not refresh native menu and control colours: $required." }
}
foreach($required in @('OnThemeChanged', 'OnThemePaint', 'PaintDarkTitle', 'OnToolbarCustomDraw', 'DocumentTreeViewBarThemeProc', 'DocumentTreeViewBarWindowThemeProc', 'ShowNativeDocumentTreeViewBarPopup', 'TrackPopupMenuEx', 'ThemeManager::WindowColor()', 'ThemeManager::TextColor()', 'ThemeManager::ControlColor()', 'ThemeManager::SeparatorColor()', 'RB_SETBKCOLOR', 'TB_SETCOLORSCHEME')) {
	if($documentTree -notlike "*$required*") { throw "Document Tree does not refresh $required on theme changes." }
}
foreach($required in @('MainMenuBarThemeProc', 'MainMenuBarWindowThemeProc', 'ShowNativeMainMenuPopup', 'ThemeManager::TrackPopupMenu', 'DarkMode_Explorer', 'ApplyMainMenuRebarBandTheme', 'RBBIM_COLORS', 'SetWindowSubclass(m_hWnd, MainMenuBarThemeProc', 'SetWindowSubclass(hWndCmdBar, MainMenuBarWindowThemeProc', 'CDRF_SKIPDEFAULT')) {
	if($mainFrame -notlike "*$required*") { throw "Main menu bar does not refresh its dark rebar surface: $required." }
}
foreach($required in @('ThemeManager::TrackPopupMenu(tp->hMenu', 'WM_COMMAND', 'CCommandBarCtrl stays')) {
	if($mainFrameHeader -notlike "*$required*") { throw "BODY context menu does not use the native themed popup: $required." }
}
if($documentTree -notlike '*SetWindowSubclass(m_hWnd, DocumentTreeViewBarThemeProc*') {
	throw 'Document Tree does not intercept custom drawing of the view selector.'
}
if($documentTreeHeader -notlike '*NOTIFY_CODE_HANDLER(NM_CUSTOMDRAW, OnToolbarCustomDraw)*') {
    throw 'Document Tree does not route toolbar custom draw notifications.'
}
$generalPageHeader = Read-ProjectFile 'src\fbe\settings\ui\SettingsGeneralPage.h'
if($generalPage -notlike '*OnInterfaceThemeChanged*' -or $generalPage -notlike '*ThemeManager::ApplyToAllThreadWindows*' -or
    $generalPage -notlike '*ThemeManager::SetSelectedTheme(m_originalTheme)*' -or $generalPageHeader -notlike '*CBN_SELCHANGE, OnInterfaceThemeChanged*') {
    throw 'Settings interface-theme live preview and Cancel rollback are missing.'
}
Write-Host 'Контракт интерфейсных тем прошёл проверку.'
