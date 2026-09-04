<# Verifies that Plugin Host no longer depends on per-user COM registration. #>
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

$pluginHost = Get-SourceText 'src\fbe\PluginManager.cpp'
if ($pluginHost -match 'CoCreateInstance|RegEnumKeyEx|DiscoverLegacyPlugins') { throw 'Plugin Host must not use registry COM activation or discovery.' }

Assert-Contains $installer 'resolve-nsis.ps1' 'Единый резолвер NSIS'
Assert-Contains $installer '/X"!addplugindir /x86-unicode %NSIS_INCLUDE_DIR%"' 'Unicode-каталог NSIS plugins'
Assert-Contains $nsisResolver "[version]'3.12'" 'Минимальная версия NSIS'
Assert-Contains $nsisResolver 'FBE_MAKENSIS' 'Явно заданный путь makensis'
Assert-Contains $nsisResolver 'Plugins\x86-unicode\UAC.dll' 'Обязательный Unicode UAC plugin'
if ($installer.IndexOf('NSIS\Unicode\makensis.exe', [StringComparison]::OrdinalIgnoreCase) -ge 0) {
    throw 'Установщик не должен требовать устаревший путь NSIS\\Unicode\\makensis.exe.'
}

Write-Host 'Plugin Host does not depend on per-user plugin registration.'
