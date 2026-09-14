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

foreach($required in @('AppsUseLightTheme', 'DwmSetWindowAttribute', 'SetWindowTheme', 'EnumThreadWindows', 'WM_FBE_THEMECHANGED')) {
    if($manager -notlike "*$required*") { throw "ThemeManager.cpp does not provide $required." }
}
if($managerHeader -notlike '*INTERFACE_THEME_AUTOMATIC*' -or $managerHeader -notlike '*WindowBrush*') {
    throw 'Theme manager must expose Automatic and shared colour resources.'
}
if($serialization -notlike '*InterfaceTheme*' -or $serialization -notlike '*ThemeManager::SetSelectedTheme*') {
    throw 'Interface theme setting is not persisted and restored.'
}
if($generalPage -notlike '*IDC_INTERFACE_THEME*' -or $generalPage -notlike '*Automatic — follow Windows*') {
    throw 'General settings page does not expose Interface theme.'
}
if($settings -notlike '*ThemeManager::IsDark()*') { throw 'Source Automatic must follow effective FBE theme.' }
if($settings -like '*GetXmlSrcThemeColor*AppsUseLightTheme*') { throw 'Source Automatic must not read Windows theme directly.' }
if($mainFrame -notlike '*ThemeManager::RefreshSystemTheme()*' -or $mainFrame -notlike '*OnThemeChanged*') {
    throw 'Main frame does not dynamically refresh theme changes.'
}
Write-Host 'Контракт интерфейсных тем прошёл проверку.'
