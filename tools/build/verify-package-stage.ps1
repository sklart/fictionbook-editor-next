[CmdletBinding()]
param(
    [string]$StageDirectory = ""
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
if ([string]::IsNullOrWhiteSpace($StageDirectory)) {
    $StageDirectory = Join-Path $repoRoot "out\package\FictionBookEditor"
}

$StageDirectory = (Resolve-Path -LiteralPath $StageDirectory).Path

$requiredFiles = @(
    "LICENSE",
    "NOTICE",
    "THIRD-PARTY-NOTICES.md",
    "FBE.exe",
    "FBV.exe",
    "Lang\Shell\FBVVerbResources.dll",
    "ExportHTML.dll",
    "ExportDOCX.dll",
    "ExportEPUB.dll",
    "ImportEPUB.dll",
    "ImportEPUBLunaSVG.dll",
    "ExportDOCXBatch.exe",
    "ExportEPUBBatch.exe",
    "ImportEPUBBatch.exe",
    "FBShell.dll",
    "FBShell64.dll",
    "FBE.Sequence.propdesc",
    "Lang\\ru-RU\\res_rus.dll",
    "Lang\\uk-UA\\res_ukr.dll",
    "Scintilla.dll",
    "Lexilla.dll"
)

$requiredThirdPartyLicenseFiles = @(
    "README.md",
    "WTL-MS-PL.txt",
    "Scintilla-Lexilla.txt",
    "PCRE2.txt",
    "Hunspell.txt",
    "Hunspell-MySpell.txt",
    "libwebp.txt",
    "OpenJPEG.txt",
    "libheif.txt",
    "libde265.txt",
    "libaom.txt",
    "libaom-PATENTS.txt",
    "LunaSVG.txt",
    "PlutoVG.txt",
    "Theme-palettes-MIT.txt",
    "UAC.txt"
)

$forbiddenFiles = @(
    "pcre.dll",
    "SciLexer.dll",
    "res_rus.dll",
    "res_ukr.dll",
    "gpl-3.0.txt",
    "copying.txt",
    "ThirdPartyNotices.txt"
)

foreach ($name in $requiredFiles) {
    $path = Join-Path $StageDirectory $name
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "В staging-каталоге отсутствует обязательный файл: $path"
    }
}

foreach ($name in $requiredThirdPartyLicenseFiles) {
    $path = Join-Path $StageDirectory "THIRD-PARTY-LICENSES\$name"
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "В staging-каталоге отсутствует обязательный текст лицензии: $path"
    }
}

$requiredInstallerTools = @(
    "register-sequence-property-schema.ps1",
    "register-modern-property-handler.ps1",
    "unregister-modern-property-handler.ps1"
)

foreach ($name in $requiredInstallerTools) {
    $path = Join-Path $StageDirectory "InstallerTools\$name"
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "В staging-каталоге отсутствует обязательный helper-скрипт: $path"
    }
}

foreach ($languageName in @("en-US", "ru-RU", "uk-UA", "de-DE", "fr-FR", "es-ES", "it-IT", "pl-PL", "cs-CZ", "bg-BG", "pt-PT", "nl-NL")) {
    $path = Join-Path $StageDirectory "Lang\Shell\$languageName\FBVVerbResources.dll.mui"
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "В staging-каталоге отсутствует обязательный MUI-файл: $path"
    }
}

$requiredThemeFiles = @(
    "Themes\README.md",
    "Themes\codeoss-dark-plus.fbetheme",
    "Themes\dracula.fbetheme",
    "Themes\github-light-default.fbetheme",
    "Themes\github-dark-default.fbetheme",
    "Themes\catppuccin-latte.fbetheme",
    "Themes\catppuccin-mocha.fbetheme"
)
foreach ($name in $requiredThemeFiles) {
    $path = Join-Path $StageDirectory $name
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "В staging-каталоге отсутствует обязательный файл темы: $path"
    }
}
$themeFiles = @(Get-ChildItem -LiteralPath (Join-Path $StageDirectory "Themes") -Filter "*.fbetheme" -File)
if ($themeFiles.Count -ne 21) {
    throw "В staging-каталоге должно быть 21 поставляемая внешняя тема, найдено: $($themeFiles.Count)."
}
if (Test-Path -LiteralPath (Join-Path $StageDirectory "Themes\licenses")) {
    throw "В staging-каталоге не должно быть дублирующего каталога Themes\\licenses."
}
foreach ($name in $forbiddenFiles) {
    $path = Join-Path $StageDirectory $name
    if (Test-Path -LiteralPath $path -PathType Leaf) {
        throw "В staging-каталоге не должно быть устаревшего файла: $path"
    }
}

Write-Host "Проверка staging-каталога пакета прошла успешно."
Write-Host "  Stage: $StageDirectory"
foreach ($name in $requiredFiles) {
    Write-Host "  OK: $name"
}
