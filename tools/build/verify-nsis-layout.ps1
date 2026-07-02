[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$installerDir = Join-Path $repoRoot "packaging\nsis\Installer"
$makeInstallerBat = Join-Path $installerDir "MakeInstaller.bat"
$makeInstallerNsi = Join-Path $installerDir "MakeInstaller.nsi"
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

$createReleaseText = Get-Content -Raw -LiteralPath (Join-Path $PSScriptRoot "create-release.ps1")
if ($createReleaseText -notmatch [regex]::Escape('/DFBE_WIN7_BUILD=1')) {
    throw "create-release.ps1 должен передавать /DFBE_WIN7_BUILD=1 при сборке Win7-инсталлятора."
}

Write-Host "Проверка структуры NSIS-контура прошла успешно."
