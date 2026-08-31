<#
.SYNOPSIS
Проверяет, что FBE Next не использует каталоги данных и реестр старого FBE.

.DESCRIPTION
Сценарий страхует независимую параллельную установку FictionBook Editor и
FictionBook Editor Next: настройки, диагностические файлы, runtime-локаль и
регистрация встроенных плагинов должны иметь собственные имена и COM GUID.
#>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path

function Get-ProjectText([string]$relativePath) {
    return Get-Content -LiteralPath (Join-Path $repoRoot $relativePath) -Raw
}

function Assert-Contains([string]$text, [string]$pattern, [string]$description) {
    if ($text.IndexOf($pattern, [StringComparison]::OrdinalIgnoreCase) -lt 0) {
        throw "${description}: не найдено '$pattern'."
    }
}

function Assert-NotContains([string]$text, [string]$pattern, [string]$description) {
    if ($text.IndexOf($pattern, [StringComparison]::OrdinalIgnoreCase) -ge 0) {
        throw "${description}: обнаружено устаревшее значение '$pattern'."
    }
}

$settings = Get-ProjectText 'src\fbe\Settings.cpp'
Assert-Contains $settings 'FictionBook Editor Next' 'Корневой ключ настроек FBE Next'

$utils = Get-ProjectText 'src\fbe\utils\Utils.cpp'
Assert-Contains $utils 'FBE Next' 'Каталог пользовательских данных FBE Next'

$runtimeLocalization = Get-ProjectText 'src\fbe\RuntimeLocalization.cpp'
$runtimeCommon = Get-ProjectText 'src\common\RuntimeLocalizationCommon.h'
foreach ($text in @($runtimeLocalization, $runtimeCommon)) {
    Assert-Contains $text 'FBE_NEXT_UI_LOCALE' 'Контракт runtime-локали Next'
    Assert-Contains $text 'FBE Next' 'Файл runtime-локали Next'
    Assert-NotContains $text 'FBE_UI_LOCALE' 'Контракт runtime-локали Next'
}

$startupTrace = Get-ProjectText 'src\fbe\StartupTrace.cpp'
Assert-Contains $startupTrace 'FBE_NEXT_TRACE' 'Переменная единого диагностического журнала Next'
Assert-Contains $startupTrace 'fbe-trace-' 'Файл единого диагностического журнала Next'
Assert-NotContains $startupTrace 'FBE_NEXT_STARTUP_TRACE' 'Диагностический журнал Next'
Assert-NotContains $startupTrace 'FBE_NEXT_SELECTION_TRACE' 'Диагностический журнал Next'

$mainFrame = Get-ProjectText 'src\fbe\mainfrm.cpp'
Assert-Contains $mainFrame '_Settings.GetKeyPath() + L"\\Toolbars"' 'Панели инструментов Next'
Assert-NotContains $mainFrame 'Software\\Haali\\FBE\\Plugins' 'Поиск legacy-плагинов'

$expectedGuids = @{
    'ExportHTML' = 'C3098839-EF69-4DE5-B27D-1E80051CA843'
    'ExportDOCX' = '09B5ABFF-177E-4C03-98D0-9EF4E1C9DB56'
    'ExportEPUB' = '36FCFB2D-C3D8-4B81-ABC1-5A09CA846515'
    'ImportEPUB' = '3C19F5A2-2EC8-4EC7-B7A9-F4910B4CDD82'
}

$guidFiles = @{
    'ExportHTML' = 'src\export-html\ExportHTML.idl'
    'ExportDOCX' = 'src\export-docx\ExportDOCX.idl'
    'ExportEPUB' = 'src\export-epub\ExportEPUB.idl'
    'ImportEPUB' = 'src\import-epub\ImportEPUBPlugin.cpp'
}
foreach ($name in $expectedGuids.Keys) {
    Assert-Contains (Get-ProjectText $guidFiles[$name]) $expectedGuids[$name] "GUID плагина $name"
}

$installer = Get-ProjectText 'packaging\nsis\Installer\MakeInstaller.nsi'
Assert-NotContains $installer 'RegDll "$INSTDIR\Plugins\ImportEPUB.dll"' 'Core-установка не зависит от RegDll'
Assert-Contains $installer 'File "${INPUTDIR}\Plugins\plugins.json"' 'Установщик кладёт манифест bundled plug-ins'
Assert-Contains $installer 'File "${INPUTDIR}\Plugins\ImportEPUB.dll"' 'Установщик кладёт bundled DLL в Plugins'
Assert-NotContains $installer 'Section /o "Legacy COM compatibility"' 'Установщик не предлагает устаревшую COM-совместимость'
Assert-Contains $installer 'DeleteRegKey HKEY_CURRENT_USER "SOFTWARE\FBETeam\FictionBook Editor Next"' 'Удаление ключей Next деинсталлятором'
Assert-Contains $installer 'App Paths\FictionBookEditorNext.exe' 'Изолированный ключ App Paths FBE Next'
Assert-NotContains $installer 'DeleteRegKey HKEY_CURRENT_USER "SOFTWARE\FBETeam"' 'Деинсталлятор Next'
Assert-NotContains $installer '$0\FBE\Hotkeys.xml' 'Деинсталлятор Next не удаляет горячие клавиши старого FBE'
Assert-NotContains $installer '$0\FBE\Settings.xml' 'Деинсталлятор Next не удаляет настройки старого FBE'

$packageManifest = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'packaging\package-manifest.json') | ConvertFrom-Json
foreach ($name in @('ExportHTML.dll', 'ExportDOCX.dll', 'ExportEPUB.dll', 'ImportEPUB.dll', 'ImportEPUBLunaSVG.dll')) {
    if ($packageManifest.core.required -notcontains "Plugins\$name") { throw "Core manifest does not require Plugins\\$name." }
    if ($packageManifest.core.required -contains $name) { throw "Core manifest still permits root plugin DLL: $name." }
    if ($packageManifest.core.forbidden -notcontains $name) { throw "Core manifest does not forbid root plugin DLL: $name." }
}
$stageCore = Get-ProjectText 'tools\build\stage-core.ps1'
Assert-Contains $stageCore '$pluginsDestination = Join-Path $stage' 'Staging создаёт Plugins-каталог'
Assert-Contains $stageCore 'Destination $pluginsDestination' 'Staging кладёт DLL плагинов в Plugins'
$repair = Get-ProjectText 'tools\build\repair-local-plugin-registration.ps1'
Assert-Contains $repair "Join-Path `$developmentDirectory 'Plugins'" 'Repair предпочитает packaged Plugins-каталог'

Write-Host 'Изоляция пользовательских данных и реестра FBE Next прошла проверку.'
Write-Host '  Каталог данных: %LOCALAPPDATA%\FBE Next'
Write-Host '  Корневой ключ: HKCU\Software\FBETeam\FictionBook Editor Next'
Write-Host "  Независимых GUID плагинов: $($expectedGuids.Count)"
