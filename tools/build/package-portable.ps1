[CmdletBinding()]
param(
    [string]$Configuration = "Release",

    [switch]$RequireWin32PropertyHandler,
    [switch]$RequireX64ShellExtension
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$sourceDir = Join-Path $repoRoot "out\$Configuration"
$buildFbvVerbMuiScript = Join-Path $PSScriptRoot "build-fbv-verb-mui.ps1"
$propertyHandlerRootDir = Join-Path $repoRoot "out\package\shell-build"
$win32PropertyHandlerSourceDir = Join-Path $propertyHandlerRootDir "Win32\$Configuration"
$x64PropertyHandlerSourceDir = Join-Path $propertyHandlerRootDir "x64\$Configuration"
$stageDir = Join-Path $repoRoot "out\package\FictionBookEditor"

if (Test-Path -LiteralPath $stageDir) {
    Remove-Item -Recurse -Force -LiteralPath $stageDir
}

& $buildFbvVerbMuiScript -Configuration $Configuration

New-Item -ItemType Directory -Path $stageDir | Out-Null
Copy-Item -Path (Join-Path $repoRoot "runtime\*") -Destination $stageDir -Recurse -Force

foreach ($name in @("FBE.exe", "FBV.exe", "FBVVerbResources.dll", "ExportHTML.dll", "ExportDOCX.dll", "ExportEPUB.dll", "ImportEPUB.dll", "ImportEPUBLunaSVG.dll", "ExportDOCXBatch.exe", "ExportEPUBBatch.exe", "ImportEPUBBatch.exe", "res_rus.dll", "res_ukr.dll")) {
    Copy-Item -LiteralPath (Join-Path $sourceDir $name) -Destination (Join-Path $stageDir $name) -Force
}

foreach ($languageName in @("en-US", "ru-RU", "uk-UA", "de-DE", "fr-FR", "es-ES", "it-IT", "pl-PL", "cs-CZ", "bg-BG", "pt-PT", "nl-NL")) {
    $sourceLanguageDir = Join-Path $sourceDir $languageName
    if (-not (Test-Path -LiteralPath $sourceLanguageDir -PathType Container)) {
        throw "Не найден каталог MUI-ресурсов: $sourceLanguageDir"
    }

    $targetLanguageDir = Join-Path $stageDir $languageName
    New-Item -ItemType Directory -Path $targetLanguageDir -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $sourceLanguageDir "FBVVerbResources.dll.mui") `
        -Destination (Join-Path $targetLanguageDir "FBVVerbResources.dll.mui") -Force
}

foreach ($name in @("custom.dic", "Hotkeys.xml", "languages.txt", "root_genres.xml", "Words.xml")) {
    Copy-Item -LiteralPath (Join-Path $repoRoot $name) -Destination $stageDir -Force
}

Copy-Item -LiteralPath (Join-Path $repoRoot "packaging\property-schema\FBE.Sequence.propdesc") `
    -Destination (Join-Path $stageDir "FBE.Sequence.propdesc") -Force

$win32PropertyHandlerPath = Join-Path $win32PropertyHandlerSourceDir "FBShell.dll"
if ($RequireWin32PropertyHandler -and -not (Test-Path -LiteralPath $win32PropertyHandlerPath -PathType Leaf)) {
    throw "Не найден Win32-обработчик свойств FBShell.dll: $win32PropertyHandlerPath"
}

if (Test-Path -LiteralPath $win32PropertyHandlerPath -PathType Leaf) {
    Copy-Item -LiteralPath $win32PropertyHandlerPath -Destination (Join-Path $stageDir "FBShell.dll") -Force
}

$x64PropertyHandlerPath = Join-Path $x64PropertyHandlerSourceDir "FBShell.dll"
if ($RequireX64ShellExtension -and -not (Test-Path -LiteralPath $x64PropertyHandlerPath -PathType Leaf)) {
    throw "Не найден x64-обработчик свойств FBShell.dll: $x64PropertyHandlerPath"
}

if (Test-Path -LiteralPath $x64PropertyHandlerPath -PathType Leaf) {
    Copy-Item -LiteralPath $x64PropertyHandlerPath -Destination (Join-Path $stageDir "FBShell64.dll") -Force
}

$installerToolsDir = Join-Path $stageDir "InstallerTools"
New-Item -ItemType Directory -Path $installerToolsDir -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $repoRoot "tools\build\register-sequence-property-schema.ps1") `
    -Destination (Join-Path $installerToolsDir "register-sequence-property-schema.ps1") -Force
Copy-Item -LiteralPath (Join-Path $repoRoot "tools\build\register-modern-property-handler.ps1") `
    -Destination (Join-Path $installerToolsDir "register-modern-property-handler.ps1") -Force
Copy-Item -LiteralPath (Join-Path $repoRoot "tools\build\unregister-modern-property-handler.ps1") `
    -Destination (Join-Path $installerToolsDir "unregister-modern-property-handler.ps1") -Force

Write-Host "Portable-пакет подготовлен в $stageDir"





