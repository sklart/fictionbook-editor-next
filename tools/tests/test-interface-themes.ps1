param([string]$RepositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path)

$ErrorActionPreference = 'Stop'
function Read-ProjectFile([string]$relativePath) {
    Get-Content -Raw -LiteralPath (Join-Path $RepositoryRoot $relativePath)
}

$manager = Read-ProjectFile 'src\fbe\ThemeManager.cpp'
$managerHeader = Read-ProjectFile 'src\fbe\ThemeManager.h'
$settings = Read-ProjectFile 'src\fbe\Settings.cpp'
$serialization = Read-ProjectFile 'src\fbe\settings\SettingsSerialization.cpp'
$generalPage = Read-ProjectFile 'src\fbe\settings\ui\SettingsGeneralPage.cpp'
$mainFrame = Read-ProjectFile 'src\fbe\mainfrm.cpp'
$toolbarUi = Read-ProjectFile 'src\fbe\ui\MainFrameRuntimeUi.inl'
$mainFrameHeader = Read-ProjectFile 'src\fbe\mainfrm.h'
$documentTree = Read-ProjectFile 'src\fbe\DocumentTree.cpp'

foreach($required in @('AppsUseLightTheme', 'g_highContrast', 'highContrastChanged', 'WH_CBT', 'HCBT_ACTIVATE', 'DwmSetWindowAttribute', 'SetWindowTheme', 'EnumThreadWindows', 'WM_FBE_THEMECHANGED')) {
    if($manager -notlike "*$required*") { throw "ThemeManager.cpp does not provide $required." }
}
foreach($required in @('THEME_COLOR_BORDER', 'THEME_COLOR_SEPARATOR', 'THEME_COLOR_SECONDARY_TEXT', 'THEME_COLOR_DISABLED_TEXT', 'THEME_COLOR_SELECTION_BACKGROUND', 'THEME_COLOR_SELECTION_TEXT', 'THEME_COLOR_HOVER', 'THEME_COLOR_PRESSED', 'THEME_COLOR_FOCUS', 'THEME_COLOR_ACCENT', 'THEME_COLOR_ERROR', 'THEME_COLOR_WARNING', 'THEME_COLOR_SUCCESS')) {
    if($managerHeader -notlike "*$required*") { throw "ThemeManager.h does not expose semantic colour $required." }
}
foreach($required in @('SetWindowSubclass', 'BS_GROUPBOX', 'PaintDarkGroupBox', 'TOOLBARCLASSNAMEW', 'WM_CTLCOLORSTATIC', 'WM_CTLCOLORBTN', 'WM_CTLCOLOREDIT', 'WC_TREEVIEWW', 'TVM_SETLINECOLOR', 'WC_LISTVIEWW', 'WC_TABCONTROLW', 'STATUSCLASSNAMEW', 'REBARCLASSNAMEW', 'EM_SETBKGNDCOLOR')) {
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
foreach($required in @('CThemedSplitterWindow', 'CThemedHorSplitterWindow', 'THEME_COLOR_SEPARATOR', 'THEME_COLOR_BORDER')) {
    if($mainFrameHeader -notlike "*$required*") { throw "Main frame does not theme splitter separator $required." }
}
foreach($required in @('isMenuBar', 'ThemeManager::DisabledTextColor()', 'ThemeManager::HoverColor()', 'ThemeManager::SelectionTextColor()', 'ThemeManager::ControlColor()', 'ILD_BLEND50', 'CDDS_ITEMPOSTPAINT')) {
    if($toolbarUi -notlike "*$required*") { throw "Toolbar custom draw does not apply semantic colour $required." }
}
foreach($required in @('FlushMenuThemesFn', 'MAKEINTRESOURCEA(136)', 'ForceDark', 'UsesClassicSurfacePalette', 'SetWindowTheme(window, L" ", L" ")', 'ApplyNativeControlPalette(window);')) {
    if($manager -notlike "*$required*") { throw "Theme manager does not refresh native menu and control colours: $required." }
}
foreach($required in @('OnThemeChanged', 'ThemeManager::WindowColor()', 'ThemeManager::SeparatorColor()', 'RB_SETBKCOLOR')) {
    if($documentTree -notlike "*$required*") { throw "Document Tree does not refresh $required on theme changes." }
}
$generalPageHeader = Read-ProjectFile 'src\fbe\settings\ui\SettingsGeneralPage.h'
if($generalPage -notlike '*OnInterfaceThemeChanged*' -or $generalPage -notlike '*ThemeManager::ApplyToAllThreadWindows*' -or
    $generalPage -notlike '*ThemeManager::SetSelectedTheme(m_originalTheme)*' -or $generalPageHeader -notlike '*CBN_SELCHANGE, OnInterfaceThemeChanged*') {
    throw 'Settings interface-theme live preview and Cancel rollback are missing.'
}
Write-Host 'Контракт интерфейсных тем прошёл проверку.'
