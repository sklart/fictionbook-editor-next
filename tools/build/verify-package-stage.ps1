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
    "FBE.exe",
    "FBV.exe",
    "FBVVerbResources.dll",
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
    "res_rus.dll",
    "res_ukr.dll",
    "Scintilla.dll",
    "Lexilla.dll"
)

$forbiddenFiles = @(
    "pcre.dll",
    "SciLexer.dll"
)

foreach ($name in $requiredFiles) {
    $path = Join-Path $StageDirectory $name
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "В staging-каталоге отсутствует обязательный файл: $path"
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
    $path = Join-Path $StageDirectory "$languageName\FBVVerbResources.dll.mui"
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "В staging-каталоге отсутствует обязательный MUI-файл: $path"
    }
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
