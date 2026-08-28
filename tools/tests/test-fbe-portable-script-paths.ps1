<#
.SYNOPSIS
Проверяет обработку Unicode-путей к пользовательским скриптам portable-версии.
#>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path

function Get-ProjectText([string]$relativePath) {
    return Get-Content -LiteralPath (Join-Path $repoRoot $relativePath) -Raw
}

function Assert-Contains([string]$text, [string]$value, [string]$description) {
    if ($text.IndexOf($value, [StringComparison]::Ordinal) -lt 0) {
        throw "${description}: не найдено '$value'."
    }
}

function Assert-NotContains([string]$text, [string]$value, [string]$description) {
    if ($text.IndexOf($value, [StringComparison]::Ordinal) -ge 0) {
        throw "${description}: обнаружено устаревшее значение '$value'."
    }
}

$scriptLoader = Get-ProjectText 'src\fbe\script.cpp'
Assert-Contains $scriptLoader '::CreateFileW(xfilename' 'Unicode-открытие пользовательского скрипта'
Assert-Contains $scriptLoader 'CString modulePath' 'Динамическое получение пути portable-версии'
Assert-NotContains $scriptLoader 'xfilename[MAX_PATH]' 'Загрузка пользовательских скриптов'
Assert-NotContains $scriptLoader 'strlcatW(' 'Загрузка пользовательских скриптов'

$settings = Get-ProjectText 'src\fbe\Settings.cpp'
Assert-Contains $settings 'CompareNoCase(GetDefaultScriptsFolder())' 'Сравнение пути Scripts без изменения регистра'
Assert-NotContains $settings 'path.MakeLower();' 'Путь стандартного каталога Scripts'
Assert-Contains $settings 'GetDefaultScriptsFolderStored' 'Отдельное portable-friendly хранимое значение Scripts'
Assert-Contains $settings 'm_scripts_folder		= GetDefaultScriptsFolderStored();' 'Значение Scripts по умолчанию хранится относительно каталога FBE'

$settingsDialog = Get-ProjectText 'src\fbe\SettingsAdvancedPage.cpp'
Assert-NotContains $settingsDialog 'folderPath.MakeLower();' 'Выбор пользовательского каталога Scripts'
Assert-Contains $settingsDialog '_Settings.GetDefaultScriptsFolderStored()' 'Default folder не превращается в абсолютный путь в Settings'
Assert-Contains $settingsDialog 'm_initialScriptsFolder.CompareNoCase(_Settings.GetResolvedScriptsFolder())' 'Перезапуск сравнивает канонические resolved-пути'

$mainFrame = Get-ProjectText 'src\fbe\mainfrm.cpp'
Assert-Contains $mainFrame 'Каталог пользовательских скриптов' 'Диагностический журнал Scripts'
Assert-Contains $mainFrame 'Найдено пользовательских скриптов' 'Диагностический журнал Scripts'

Write-Host 'Проверка Unicode-путей пользовательских скриптов portable-версии пройдена.'
