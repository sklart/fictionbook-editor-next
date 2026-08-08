# Проверяет, что Modern/Win7 release-поток повторно собирает DLL редактора и
# пакетные конвертеры, а остальные общие компоненты и MUI/property handler
# используются явно.
[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path

function Get-Text {
    param([string]$RelativePath)
    return Get-Content -Raw -LiteralPath (Join-Path $repoRoot $RelativePath)
}

function Assert-Contains {
    param([string]$Text, [string]$Expected, [string]$Description)
    if (-not $Text.Contains($Expected)) {
        throw "${Description}: не найдено '$Expected'."
    }
}

function Assert-NotContains {
    param([string]$Text, [string]$Unexpected, [string]$Description)
    if ($Text.Contains($Unexpected)) {
        throw "${Description}: найдено устаревшее '$Unexpected'."
    }
}

$workflow = Get-Text ".github\workflows\build.yml"
$build = Get-Text "tools\build\build.ps1"
$scintilla = Get-Text "tools\build\build-scintilla.ps1"
$release = Get-Text "tools\build\create-release.ps1"
$portable = Get-Text "tools\build\package-portable.ps1"
$artifacts = Get-Text "tools\build\verify-artifacts.ps1"
$verifyRelease = Get-Text "tools\build\verify-release.ps1"
$syncVersion = Get-Text "tools\version\sync-version.ps1"
$lunaSvg = Get-Text "src\import-epub\thirdparty\lunasvg\lunasvg.vcxproj"
$plutoVg = Get-Text "src\import-epub\thirdparty\lunasvg\plutovg.vcxproj"
$lunaAdapter = Get-Text "src\import-epub\ImportEPUBLunaSVG.vcxproj"

Assert-Contains $workflow "-EditorRuntimeOnly" "Win7-этап workflow"
Assert-Contains $workflow "-BatchConvertersOnly" "Win7-этап пакетных конвертеров workflow"
Assert-Contains $workflow "-SkipDependencies" "Win7 batch-этап workflow"
Assert-Contains $workflow "-ReuseEditorRuntime" "безопасное повторное использование runtime-кэша"
Assert-Contains $workflow "concurrency:" "workflow"
Assert-Contains $workflow "contents: read" "минимальные права workflow"
Assert-Contains $workflow "validate:" "отдельная job валидации"
Assert-Contains $workflow "build:" "отдельная job сборки"
Assert-Contains $workflow "package:" "отдельная job упаковки"
Assert-Contains $workflow "publish:" "отдельная job публикации"
Assert-Contains $workflow "actions/upload-artifact@v4" "передача результатов между jobs"
Assert-Contains $workflow "actions/download-artifact@v4" "получение результатов между jobs"
Assert-Contains $workflow "out/target-batches" "передача целевых batch-конвертеров между jobs"
Assert-Contains $workflow "Restore Modern batch converters" "восстановление Modern batch-конвертеров"
Assert-Contains $workflow "Restore Win7 batch converters" "восстановление Win7 batch-конвертеров"
Assert-Contains $workflow "Verify Modern binaries on pull requests" "основные проверки PR"
Assert-Contains $workflow "-SkipPropertyHandlerBuild" "Win7-релиз workflow"
Assert-Contains $workflow "-SkipFbvVerbMuiBuild" "Win7-релиз workflow"
Assert-Contains $build "[switch]`$EditorRuntimeOnly" "build.ps1"
Assert-Contains $build "Собраны только целевые DLL редактора" "build.ps1"
Assert-Contains $build "[switch]`$BatchConvertersOnly" "build.ps1"
Assert-Contains $build "[switch]`$SkipDependencies" "build.ps1"
Assert-Contains $build "Assert-PreparedDependencies" "build.ps1"
Assert-Contains $build "[switch]`$ForceRebuildRequiredProjects" "build.ps1"
Assert-Contains $build "Собраны только пакетные конвертеры" "build.ps1"
Assert-Contains $scintilla "[string]`$OutputDirectory" "build-scintilla.ps1"
Assert-Contains $scintilla "out\editor-runtime" "build-scintilla.ps1"
Assert-Contains $scintilla "[switch]`$ReusePreparedRuntime" "build-scintilla.ps1"
Assert-Contains $release "[switch]`$SkipPropertyHandlerBuild" "create-release.ps1"
Assert-Contains $release "[switch]`$SkipFbvVerbMuiBuild" "create-release.ps1"
Assert-Contains $release "[switch]`$Prerelease" "create-release.ps1"
Assert-Contains $release "-EditorRuntimeDirectory `$editorRuntimeDirectory" "create-release.ps1"
Assert-Contains $release "[switch]`$SkipCommonChecks" "create-release.ps1"
Assert-Contains $release "out\artifacts\{0}" "изолированные артефакты release"
Assert-NotContains $release '& (Join-Path $repoRoot "tools\tests\test-spellcheck-dictionaries.ps1")' "create-release.ps1"
Assert-Contains $portable "[string]`$EditorRuntimeDirectory" "package-portable.ps1"
Assert-Contains $portable "[switch]`$SkipFbvVerbMuiBuild" "package-portable.ps1"
Assert-Contains $portable '"Scintilla.dll", "Lexilla.dll"' "package-portable.ps1"
Assert-Contains $artifacts "Get-ZipEntrySha256" "verify-artifacts.ps1"
Assert-Contains $artifacts "Общий файл" "verify-artifacts.ps1"
Assert-Contains $artifacts "Win7-вариант не был применён" "verify-artifacts.ps1"
Assert-Contains $artifacts "Проверка изолированных Modern и Win7 артефактов" "verify-artifacts.ps1"
Assert-Contains $syncVersion "Write-Utf8FileIfChanged" "sync-version.ps1"
Assert-Contains $syncVersion "Generated-файл уже синхронизирован" "sync-version.ps1"
Assert-Contains $lunaSvg "build\lib\lunasvg" "lunasvg.vcxproj"
Assert-Contains $plutoVg "build\lib\lunasvg" "plutovg.vcxproj"
Assert-Contains $lunaAdapter "build\lib\lunasvg\Win32" "ImportEPUBLunaSVG.vcxproj"
Assert-NotContains $lunaAdapter "thirdparty\lunasvg\lib\Win32" "ImportEPUBLunaSVG.vcxproj"
Assert-Contains $artifacts 'foreach ($name in @("ExportDOCXBatch.exe", "ExportEPUBBatch.exe", "ImportEPUBBatch.exe"))' "verify-artifacts.ps1"
Assert-Contains $verifyRelease 'analyze-product-hardcoded-cyrillic.ps1")' "verify-release.ps1"
Assert-Contains $verifyRelease "[switch]`$SkipCommonChecks" "verify-release.ps1"
Assert-Contains $verifyRelease '-EditorRuntimeDirectory (Join-Path $repoRoot "out\editor-runtime\$CompatibilityTarget")' "verify-release.ps1"
if ($verifyRelease -match 'analyze-product-hardcoded-cyrillic\.ps1"\)\s+-FailOnFindings') {
    throw "Релизный контур не должен блокироваться накопленным набором кириллических строк; строгая проверка допустима только для отдельных фикстур."
}

$win7Imports = Get-Text "tools\tests\check-win7-imports.ps1"
Assert-Contains $win7Imports '"CreateFile2"' "check-win7-imports.ps1"

$runtimeOnlyIndex = $build.IndexOf("if (`$EditorRuntimeOnly)")
$pcre2Index = $build.IndexOf("Подготовка PCRE2")
if ($runtimeOnlyIndex -lt 0 -or $pcre2Index -lt 0 -or $runtimeOnlyIndex -gt $pcre2Index) {
    throw "Режим EditorRuntimeOnly обязан завершаться до сборки PCRE2 и общих компонентов."
}

$batchIndex = $build.IndexOf("if (`$BatchConvertersOnly)")
$batchReturnIndex = $build.IndexOf('    return', $batchIndex)
if ($batchIndex -lt 0 -or $batchReturnIndex -lt 0 -or
    $build.Substring($batchIndex, $batchReturnIndex - $batchIndex).Contains('build-scintilla.ps1')) {
    throw "BatchConvertersOnly не должен запускать build-scintilla.ps1 до выхода из режима batch-конвертеров."
}

Assert-NotContains $build "/t:Rebuild" "build.ps1: CI-граф не должен содержать безусловный Rebuild"

Write-Host "Проверка исключения повторных сборок Modern/Win7 прошла успешно."
