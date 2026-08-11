# Собирает portable-каталог FictionBook Editor Next из runtime-файлов, бинарников сборки и вспомогательных ресурсов.
[CmdletBinding()]
param(
    [string]$Configuration = "Release",

    # Явный источник Scintilla/Lexilla конкретного варианта совместимости.
    # При отсутствии параметра сохраняется локальное историческое поведение.
    [string]$EditorRuntimeDirectory = "",

    # Явный источник EXE/PDB batch-конвертеров конкретного target.
    [string]$BatchOutputDirectory = "",

    [string]$ArchHandlerOutputDirectory = "",

    [string]$PackageDirectory = "",

    [switch]$RequireWin32PropertyHandler,
    [switch]$RequireX64ShellExtension,

    # Повторная сборка MUI не нужна, если общий релизный этап уже её выполнил.
    [switch]$SkipFbvVerbMuiBuild
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$sourceDir = Join-Path $repoRoot "out\$Configuration"
$editorRuntimeSourceDir = if ([string]::IsNullOrWhiteSpace($EditorRuntimeDirectory)) {
    Join-Path $repoRoot "runtime"
} else {
    $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($EditorRuntimeDirectory)
}
$batchOutputSourceDir = if ([string]::IsNullOrWhiteSpace($BatchOutputDirectory)) {
    $sourceDir
} else {
    $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($BatchOutputDirectory)
}
$buildFbvVerbMuiScript = Join-Path $PSScriptRoot "build-fbv-verb-mui.ps1"
$archHandlerBuildDir = if ([string]::IsNullOrWhiteSpace($ArchHandlerOutputDirectory)) { Join-Path $repoRoot "out\archhandler\Win32\$Configuration" } else { $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($ArchHandlerOutputDirectory) }
$propertyHandlerRootDir = Join-Path $repoRoot "out\package\shell-build"
$win32PropertyHandlerSourceDir = Join-Path $propertyHandlerRootDir "Win32\$Configuration"
$x64PropertyHandlerSourceDir = Join-Path $propertyHandlerRootDir "x64\$Configuration"
$stageDir = if ([string]::IsNullOrWhiteSpace($PackageDirectory)) {
    Join-Path $repoRoot "out\package\FictionBookEditor"
} else {
    $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($PackageDirectory)
}

if (Test-Path -LiteralPath $stageDir) {
    Remove-Item -Recurse -Force -LiteralPath $stageDir
}

if (-not $SkipFbvVerbMuiBuild) {
    & $buildFbvVerbMuiScript -Configuration $Configuration
}

foreach ($name in @("Scintilla.dll", "Lexilla.dll")) {
    $editorRuntimePath = Join-Path $editorRuntimeSourceDir $name
    if (-not (Test-Path -LiteralPath $editorRuntimePath -PathType Leaf)) {
        throw "Не найдена целевая DLL редактора: $editorRuntimePath"
    }
}

New-Item -ItemType Directory -Path $stageDir | Out-Null
Copy-Item -Path (Join-Path $repoRoot "runtime\*") -Destination $stageDir -Recurse -Force
foreach($name in @('ZipHandler.exe', 'RarHandler.exe')) {
    if (-not (Test-Path -LiteralPath (Join-Path $archHandlerBuildDir $name) -PathType Leaf)) { throw "Не найден заранее собранный ArchHandler artifact: $(Join-Path $archHandlerBuildDir $name)" }
    Copy-Item -LiteralPath (Join-Path $archHandlerBuildDir $name) -Destination (Join-Path $stageDir "Utilities\ArchHandler\$name") -Force
}

# Theme attribution is centralised in the package root.  Remove a stale empty
# directory left by older worktrees before materialising the release layout.
$legacyThemeLicenseDir = Join-Path $stageDir "Themes\licenses"
if (Test-Path -LiteralPath $legacyThemeLicenseDir) {
    Remove-Item -LiteralPath $legacyThemeLicenseDir -Recurse -Force
}

# Release packages must carry the project license and the notices for every
# bundled dependency.  Keep the source-of-truth documents at the repository
# root, then materialize the individual upstream license texts here.
Copy-Item -LiteralPath (Join-Path $repoRoot "runtime\gpl-3.0.txt") -Destination (Join-Path $stageDir "LICENSE") -Force
# LICENSE is the canonical English GPL text in a distributable package.  The
# runtime source copy is needed by the localized installer page, but carrying
# both files in the portable package would be a byte-for-byte duplicate.
$legacyEnglishGplPath = Join-Path $stageDir "gpl-3.0.txt"
if (Test-Path -LiteralPath $legacyEnglishGplPath -PathType Leaf) {
    Remove-Item -LiteralPath $legacyEnglishGplPath -Force
}
Copy-Item -LiteralPath (Join-Path $repoRoot "NOTICE") -Destination (Join-Path $stageDir "NOTICE") -Force
Copy-Item -LiteralPath (Join-Path $repoRoot "THIRD-PARTY-NOTICES.md") -Destination (Join-Path $stageDir "THIRD-PARTY-NOTICES.md") -Force
$thirdPartyLicenseDir = Join-Path $stageDir "THIRD-PARTY-LICENSES"
New-Item -ItemType Directory -Path $thirdPartyLicenseDir -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $repoRoot "THIRD-PARTY-LICENSES\README.md") -Destination $thirdPartyLicenseDir -Force
Copy-Item -LiteralPath (Join-Path $repoRoot "THIRD-PARTY-LICENSES\WTL-MS-PL.txt") -Destination $thirdPartyLicenseDir -Force
$thirdPartyLicenseFiles = @{
    "Scintilla-Lexilla.txt" = "third_party\scintilla\License.txt"
    "PCRE2.txt" = "third_party\pcre2\LICENCE.md"
    "Hunspell.txt" = "third_party\hunspell\license.hunspell"
    "Hunspell-MySpell.txt" = "third_party\hunspell\license.myspell"
    "libwebp.txt" = "third_party\libwebp\COPYING"
    "OpenJPEG.txt" = "third_party\openjpeg\LICENSE"
    "libheif.txt" = "third_party\libheif\COPYING"
    "libde265.txt" = "third_party\libde265\COPYING"
    "libaom.txt" = "third_party\aom\LICENSE"
    "libaom-PATENTS.txt" = "third_party\aom\PATENTS"
    "LunaSVG.txt" = "src\import-epub\thirdparty\lunasvg\LICENSE"
    "PlutoVG.txt" = "src\import-epub\thirdparty\lunasvg\plutovg\LICENSE"
    "Theme-palettes-MIT.txt" = "THIRD-PARTY-LICENSES\Theme-palettes-MIT.txt"
    "UAC.txt" = "third_party\uac\License.txt"
}
foreach ($entry in $thirdPartyLicenseFiles.GetEnumerator()) {
    Copy-Item -LiteralPath (Join-Path $repoRoot $entry.Value) -Destination (Join-Path $thirdPartyLicenseDir $entry.Key) -Force
}

# Runtime-каталог содержит удобные локальные копии DLL. Для релиза заменяем их
# явным вариантом, чтобы пакет не зависел от предыдущей исторической сборки.
foreach ($name in @("Scintilla.dll", "Lexilla.dll")) {
    Copy-Item -LiteralPath (Join-Path $editorRuntimeSourceDir $name) `
        -Destination (Join-Path $stageDir $name) -Force
}

foreach ($legacyRootResource in @("res_rus.dll", "res_ukr.dll")) {
    $legacyRootPath = Join-Path $stageDir $legacyRootResource
    if (Test-Path -LiteralPath $legacyRootPath -PathType Leaf) {
        Remove-Item -LiteralPath $legacyRootPath -Force
    }
}

& (Join-Path $repoRoot "tools\localization\export-runtime-lang.ps1") `
    -RepositoryRoot $repoRoot `
    -OutputDirectory (Join-Path $stageDir "Lang") `
    -Clean

foreach ($name in @("FBE.exe", "FBV.exe", "ExportHTML.dll", "ExportDOCX.dll", "ExportEPUB.dll", "ImportEPUB.dll", "ImportEPUBLunaSVG.dll")) {
    Copy-Item -LiteralPath (Join-Path $sourceDir $name) -Destination (Join-Path $stageDir $name) -Force
}
foreach ($name in @("ExportDOCXBatch.exe", "ExportEPUBBatch.exe", "ImportEPUBBatch.exe")) {
    Copy-Item -LiteralPath (Join-Path $batchOutputSourceDir $name) -Destination (Join-Path $stageDir $name) -Force
}

$localizedResourceDlls = @{
    "ru-RU" = "res_rus.dll"
    "uk-UA" = "res_ukr.dll"
}
foreach ($locale in $localizedResourceDlls.Keys) {
    $targetLanguageDir = Join-Path $stageDir "Lang\\$locale"
    New-Item -ItemType Directory -Path $targetLanguageDir -Force | Out-Null
    $dllName = $localizedResourceDlls[$locale]
    Copy-Item -LiteralPath (Join-Path $sourceDir "Lang\\$locale\\$dllName") -Destination (Join-Path $targetLanguageDir $dllName) -Force
}

$shellLocalizationSourceDir = Join-Path $sourceDir "Lang\Shell"
$shellLocalizationTargetDir = Join-Path $stageDir "Lang\Shell"
New-Item -ItemType Directory -Path $shellLocalizationTargetDir -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $shellLocalizationSourceDir "FBVVerbResources.dll") `
    -Destination (Join-Path $shellLocalizationTargetDir "FBVVerbResources.dll") -Force

foreach ($languageName in @("en-US", "ru-RU", "uk-UA", "de-DE", "fr-FR", "es-ES", "it-IT", "pl-PL", "cs-CZ", "bg-BG", "pt-PT", "nl-NL")) {
    $sourceLanguageDir = Join-Path $shellLocalizationSourceDir $languageName
    if (-not (Test-Path -LiteralPath $sourceLanguageDir -PathType Container)) {
        throw "Не найден каталог MUI-ресурсов: $sourceLanguageDir"
    }

    $targetLanguageDir = Join-Path $shellLocalizationTargetDir $languageName
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

& (Join-Path $repoRoot "tools\tests\test-runtime-lang-package.ps1") -PackageDirectory $stageDir

Write-Host "Portable-пакет подготовлен в $stageDir"
