<#
.SYNOPSIS
Проверяет регистрацию экспортных плагинов без прав администратора.

.DESCRIPTION
Установщик FBE Next не требует повышения прав для обычной установки.
Поэтому COM-классы экспортных плагинов должны записываться в
HKCU\Software\Classes, а NSIS-скрипт обязан собираться Unicode-вариантом.
#>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path

function Get-SourceText([string] $relativePath) {
    return Get-Content -Raw -LiteralPath (Join-Path $repoRoot $relativePath)
}

function Assert-Contains([string] $text, [string] $expected, [string] $description) {
    if ($text.IndexOf($expected, [StringComparison]::Ordinal) -lt 0) {
        throw "${description}: не найдено '$expected'."
    }
}

$html = Get-SourceText 'src\export-html\ExportHTML.cpp'
$epub = Get-SourceText 'src\export-epub\ExportEPUB.cpp'
$docx = Get-SourceText 'src\export-docx\ExportDOCX.cpp'
$installer = Get-SourceText 'packaging\nsis\Installer\MakeInstaller.bat'
$nsisResolver = Get-SourceText 'tools\build\resolve-nsis.ps1'

foreach ($plugin in @(
    @{ Name = 'ExportHTML'; Text = $html },
    @{ Name = 'ExportEPUB'; Text = $epub }
)) {
    Assert-Contains $plugin.Text 'STDAPI DllRegisterServer(void)' "Регистрация $($plugin.Name)"
    Assert-Contains $plugin.Text 'STDAPI DllUnregisterServer(void)' "Снятие регистрации $($plugin.Name)"
    Assert-Contains $plugin.Text 'ATL::AtlSetPerUserRegistration(true);' "Пользовательская COM-регистрация $($plugin.Name)"
}

Assert-Contains $docx 'HKEY_CURRENT_USER,' 'Пользовательская COM-регистрация ExportDOCX'
Assert-Contains $docx 'Software\\Classes\\CLSID' 'Раздел Classes ExportDOCX'
if ($docx.IndexOf('HKEY_CLASSES_ROOT') -ge 0) {
    throw 'ExportDOCX не должен записывать COM-класс в HKEY_CLASSES_ROOT при обычной установке.'
}

Assert-Contains $installer 'resolve-nsis.ps1' 'Единый резолвер NSIS'
Assert-Contains $installer '/X"!addplugindir /x86-unicode %NSIS_INCLUDE_DIR%"' 'Unicode-каталог NSIS plugins'
Assert-Contains $nsisResolver "[version]'3.12'" 'Минимальная версия NSIS'
Assert-Contains $nsisResolver 'FBE_MAKENSIS' 'Явно заданный путь makensis'
Assert-Contains $nsisResolver 'Plugins\x86-unicode\UAC.dll' 'Обязательный Unicode UAC plugin'
if ($installer.IndexOf('NSIS\Unicode\makensis.exe', [StringComparison]::OrdinalIgnoreCase) -ge 0) {
    throw 'Установщик не должен требовать устаревший путь NSIS\\Unicode\\makensis.exe.'
}

Write-Host 'Проверка пользовательской регистрации экспортных плагинов прошла успешно.'
