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
$fbDoc = Read-ProjectFile 'src\fbe\FBDoc.cpp'
$settingsEditorPage = Read-ProjectFile 'src\fbe\settings\ui\SettingsEditorPage.cpp'
$settingsEditorPageHeader = Read-ProjectFile 'src\fbe\settings\ui\SettingsEditorPage.h'
$modelessDialog = Read-ProjectFile 'src\fbe\ModelessDialog.h'
$aboutBox = Read-ProjectFile 'src\fbe\AboutBox.cpp'
$scriptVisuals = Read-ProjectFile 'src\fbe\scripts\ScriptVisualResources.cpp'
$colorButton = Read-ProjectFile 'src\fbe\extras\ColorButton.cpp'
$utils = Read-ProjectFile 'src\fbe\utils\Utils.cpp'
$utilsHeader = Read-ProjectFile 'src\fbe\utils\utils.h'

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
foreach($required in @('RegisterNativeMenuBitmap', 'NativeMenuBitmap', 'RegisterOwnedNativeMenuBitmap', 'CreateAlphaBitmap', 'IDB_TABLE_INSERT_ROW_ABOVE', 'IDB_TABLE_MAKE_NORMAL_CELLS', 'ApplyMainRebarTheme', 'RBS_BANDBORDERS', 'RBBS_CHILDEDGE', 'RBBIM_COLORS')) {
	if($mainFrame -notlike "*$required*") { throw "Native dark menus or rebar bands do not apply the required dark path: $required." }
}
foreach($required in @('RBBS_NOGRIPPER', 'FBE persists toolbar visibility/order', 'MainRebarThemeProc', 'RB_GETRECT', 'THEME_COLOR_BORDER', 'StatusBarThemeProc', 'SB_GETPARTS', 'WM_GETFONT', 'UiMetrics::DialogFont()', 'GetTextMetrics', 'ScaleForDpi', 'SecondaryTextColor()', 'SBARS_SIZEGRIP', 'THEME_COLOR_SEPARATOR', 'SetDCPenColor', 'ThemeManager::SeparatorColor()')) {
	if($mainFrame -notlike "*$required*") { throw "Rebar/status bar Dark surface lacks the required native-theme handling: $required." }
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
foreach($required in @('ThemedMessageDialog', 'FBEThemedMessageDialog', 'MB_SYSTEMMODAL', 'MB_SERVICE_NOTIFICATION', 'UiMetrics::DialogFont()', 'UiMetrics::DpiForWindow(m_owner)', 'AdjustWindowRectExForDpi', 'AdjustWindowRectEx', 'EnsureClientArea', 'GetClientRect', 'MB_DEFMASK', 'BM_CLICK', 'ThemeManager::ApplyToWindow(m_window)', 'ThemeManager::WindowBrush()', 'ThemeManager::ControlBrush()', 'THEME_COLOR_SEPARATOR', 'VK_ESCAPE', 'VK_RETURN', 'IsDialogMessageW')) {
	if($manager -notlike "*$required*") { throw "Theme manager does not provide the themed FBE-owned message dialog: $required." }
}
if($managerHeader -notlike '*int MessageBox(HWND owner, LPCWSTR message, LPCWSTR caption, UINT type)*') {
	throw 'Theme manager does not expose the FBE-owned message dialog wrapper.'
}
foreach($required in @('ThemeManager::MessageBox(::GetActiveWindow(),str,title,type)', 'ThemeManager::MessageBox(::GetActiveWindow(), err, cpt, MB_OK|MB_ICONERROR)')) {
	if($utils -notlike "*$required*") { throw "Common FBE error/confirmation messages bypass the shared themed wrapper: $required." }
}
if($utilsHeader -notlike '*MessageBox(HWND owner, const TCHAR *message, const TCHAR *title, UINT type)*') {
	throw 'Direct FBE-owned messages do not have the owner-preserving themed wrapper.'
}
foreach($source in @(
	' src\fbe\FBE.cpp', ' src\fbe\FBDoc.cpp', ' src\fbe\FBEview.cpp', ' src\fbe\mainfrm.cpp',
	' src\fbe\settings\ui\SettingsAdvancedPage.cpp', ' src\fbe\settings\ui\SettingsSourcePage.cpp',
	' src\fbe\settings\ui\SettingsSpellingPage.cpp', ' src\fbe\Speller.cpp')) {
	$directMessageSource = Read-ProjectFile $source.Trim()
	if($directMessageSource -match '(?<!U)::MessageBox\(') { throw "FBE-owned message dialog bypasses the shared themed wrapper: $($source.Trim())." }
}
foreach($required in @('RegisterNativeMenuBitmap', 'UnregisterNativeMenuBitmap', 'ApplyNativeMenuBitmaps', 'MIIM_BITMAP', 'TrackPopupMenuEx')) {
	if($manager -notlike "*$required*") { throw "Theme manager does not attach registered native popup bitmaps: $required." }
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
foreach($required in @('ContextAttributeBoxThemeProc', 'WS_EX_CLIENTEDGE', 'GWL_EXSTYLE', 'SWP_FRAMECHANGED', 'THEME_COLOR_BORDER', 'ApplyBoxTheme')) {
	if($contextAttributeBars -notlike "*$required*") { throw "Context attribute boxes do not replace the Dark client edge with a themed border: $required." }
}
foreach($required in @('ApplyRuntimeTableTheme', 'fbe-runtime-dark-table-theme', 'table.table th', 'table.table td', 'ThemeManager::ControlColor()', 'ThemeManager::BorderColor()', 'sheet->cssText')) {
	if($fbDoc -notlike "*$required*") { throw "BODY table headers do not receive the runtime-only Dark table theme: $required." }
}
foreach($required in @('ApplyRuntimeScrollbarTheme', 'fbe-runtime-dark-scrollbar-theme', 'scrollbar-face-color', 'scrollbar-track-color', 'scrollbar-arrow-color', 'html,body', 'sheet->cssText = L""')) {
	if($fbDoc -notlike "*$required*") { throw "BODY scrollbar runtime override is incomplete: $required." }
}
if($fbDoc -like '*runtime\main.css*' -or $fbDoc -notlike '*never BODY*') { throw 'BODY table theme must remain a runtime MSHTML override and must not replace BODY colours.' }
foreach($required in @('AutomaticBodyColor', 'ThemeManager::WindowColor()', 'ThemeManager::TextColor()', 'm_background.SetDefaultColor', 'm_foreground.SetDefaultColor')) {
	if($settingsEditorPage -notlike "*$required*") { throw "Automatic BODY colour buttons do not show their effective Dark value: $required." }
}
if($settingsEditorPage -notlike '*OnThemeChanged*' -or $settingsEditorPageHeader -notlike '*MESSAGE_HANDLER(WM_FBE_THEMECHANGED, OnThemeChanged)*') {
	throw 'Automatic BODY colour buttons do not refresh while Settings previews a live theme change.'
}
foreach($required in @('ThemeManager::ControlBrush()', 'ThemeManager::TextColor()', 'ThemeManager::DisabledTextColor()', 'IsWindowEnabled(m_hWnd)')) {
	if($contextAttributeControls -notlike "*$required*") { throw "Context attribute captions do not apply the theme palette: $required." }
}
foreach($required in @('m_contextAttributeBars.ApplyTheme()', 'ApplyContextAttributeRebarBandTheme', 'RBBIM_COLORS', 'ThemeManager::ControlColor()')) {
	if($mainFrame -notlike "*$required*") { throw "Main frame does not refresh context attribute bar bands: $required." }
}
foreach($required in @('CThemedSplitterWindow', 'CThemedHorSplitterWindow', 'ThemeManager::ControlBrush()', 'THEME_COLOR_BORDER', 'single horizontal structural border')) {
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
