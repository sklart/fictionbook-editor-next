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

Assert-Contains $installer 'NSIS\Unicode\makensis.exe' 'Путь к Unicode NSIS'
Assert-Contains $installer 'IF NOT EXIST "%MAKENSIS%" (' 'Обязательная проверка Unicode NSIS'
if ($installer.IndexOf('SET "MAKENSIS=%ProgramFiles(x86)%\NSIS\makensis.exe"') -ge 0) {
    throw 'Установщик не должен откатываться к не-Unicode makensis.exe.'
}

Write-Host 'Проверка пользовательской регистрации экспортных плагинов прошла успешно.'
