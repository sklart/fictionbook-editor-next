[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$installerDir = Join-Path $repoRoot "packaging\nsis\Installer"
$makeInstallerBat = Join-Path $installerDir "MakeInstaller.bat"
$makeInstallerNsi = Join-Path $installerDir "MakeInstaller.nsi"
$languageFallbackGenerator = Join-Path $repoRoot "tools\localization\export-nsis-installer-fallbacks.ps1"
$legacyInputDir = Join-Path $installerDir "Input"

if (Test-Path -LiteralPath $legacyInputDir) {
    throw "Исторический каталог NSIS input больше не должен существовать: $legacyInputDir"
}

$batText = Get-Content -Raw -LiteralPath $makeInstallerBat
if (
    $batText -notmatch 'SET "INPUTDIR=%REPO_ROOT%\\out\\package\\FictionBookEditor"' -and
    $batText -notmatch 'SET "INPUTDIR=\.\.\\\.\.\\\.\.\\out\\package\\FictionBookEditor"'
) {
    throw "MakeInstaller.bat должен использовать out\\package\\FictionBookEditor как INPUTDIR."
}

if ($batText -match 'LegacyInput') {
    throw "MakeInstaller.bat всё ещё ссылается на LegacyInput, хотя этот fallback уже удалён."
}

$nsiText = Get-Content -Raw -LiteralPath $makeInstallerNsi
if ($nsiText -notmatch [regex]::Escape('!define INPUTDIR "..\..\..\out\package\FictionBookEditor"')) {
    throw "MakeInstaller.nsi должен по умолчанию направлять INPUTDIR в out\\package\\FictionBookEditor."
}

if ($nsiText -notmatch [regex]::Escape('!define MUI_STARTMENUPAGE_DEFAULTFOLDER "${PRODUCT_NAME}"')) {
    throw "Папка меню Пуск по умолчанию должна называться FictionBook Editor Next без номера версии."
}

if ($nsiText -notmatch 'FBE_WIN7_BUILD' -or
    $nsiText -notmatch 'Windows 7 compatible') {
    throw "MakeInstaller.nsi должен явно маркировать Win7-compatible установщик."
}

if ($nsiText -notmatch [regex]::Escape('SetOutPath "$INSTDIR\Lang\Shell"') -or
    $nsiText -notmatch [regex]::Escape('File "${INPUTDIR}\Lang\Shell\FBVVerbResources.dll"')) {
    throw "MakeInstaller.nsi должен устанавливать MUI-host shell-команды в Lang\\Shell."
}
if ($nsiText -notmatch [regex]::Escape('SetOutPath "$INSTDIR\Plugins"') -or
    $nsiText -notmatch [regex]::Escape('File "${INPUTDIR}\Plugins\plugins.json"')) {
    throw "MakeInstaller.nsi должен устанавливать манифест bundled plug-ins в Plugins\\plugins.json."
}
if ($nsiText -match [regex]::Escape('Section /o "Legacy COM compatibility"')) {
    throw "MakeInstaller.nsi больше не должен предлагать компонент Legacy COM compatibility."
}
if ($nsiText -notmatch [regex]::Escape('@$INSTDIR\Lang\Shell\FBVVerbResources.dll,-109;v2')) {
    throw "MUIVerb shell-команды должен ссылаться на модуль в Lang\\Shell."
}
if ($nsiText -notmatch [regex]::Escape('SetOutPath "$INSTDIR\Themes"') -or
    $nsiText -notmatch [regex]::Escape('File /r ${INPUTDIR}\Themes\*.*')) {
    throw "MakeInstaller.nsi должен устанавливать поставляемые темы в Themes."
}

foreach ($language in @("English", "Russian", "Ukrainian", "German", "French", "Spanish", "Italian", "Polish", "Portuguese", "Dutch", "Czech", "Bulgarian")) {
    if ($nsiText -notmatch [regex]::Escape('!insertmacro MUI_LANGUAGE "' + $language + '"')) {
        throw "MakeInstaller.nsi должен подключать язык мастера установки: $language."
    }
}

if ($batText -notmatch [regex]::Escape('tools\localization\export-nsis-installer-fallbacks.ps1')) {
    throw "MakeInstaller.bat должен генерировать fallback продуктовых строк для дополнительных языков NSIS."
}
if ($nsiText -notmatch [regex]::Escape('!include "Generated\EuropeanFallback.generated.nsh"')) {
    throw "MakeInstaller.nsi должен подключать generated fallback дополнительных языков NSIS."
}
if (-not (Test-Path -LiteralPath $languageFallbackGenerator)) {
    throw "Не найден генератор fallback продуктовых строк NSIS: $languageFallbackGenerator"
}

$createReleaseText = Get-Content -Raw -LiteralPath (Join-Path $PSScriptRoot "create-release.ps1")
if ($createReleaseText -notmatch [regex]::Escape('/DFBE_WIN7_BUILD=1')) {
    throw "create-release.ps1 должен передавать /DFBE_WIN7_BUILD=1 при сборке Win7-инсталлятора."
}

Write-Host "Проверка структуры NSIS-контура прошла успешно."
