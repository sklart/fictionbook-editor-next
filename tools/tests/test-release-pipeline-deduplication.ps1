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
$editorRuntimeHelpers = Get-Text "tools\build\editor-runtime-helpers.ps1"
$release = Get-Text "tools\build\create-release.ps1"
$portable = Get-Text "tools\build\package-portable.ps1"
$stageCore = Get-Text "tools\build\stage-core.ps1"
$stageIntegration = Get-Text "tools\build\stage-integration.ps1"
$manifest = Get-Text "packaging\package-manifest.json"
$artifacts = Get-Text "tools\build\verify-artifacts.ps1"
$verifyRelease = Get-Text "tools\build\verify-release.ps1"
$archHandlerBuild = Get-Text "tools\build\build-archhandler.ps1"
$archHandlerArgv = Get-Text "tools\tests\test-archhandler-argv.ps1"
$fbeView = Get-Text "src\fbe\FBEview.cpp"
$pcre2Build = Get-Text "tools\build\build-pcre2.ps1"
$hunspellBuild = Get-Text "tools\build\build-hunspell.ps1"
$muiBuild = Get-Text "tools\build\build-fbv-verb-mui.ps1"
$importVsEnvironment = Get-Text "tools\build\Import-VsDevEnvironment.ps1"
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
if ($workflow -notmatch '(?s)package:.*?actions/checkout@v5\s+with:\s+submodules: recursive') {
    throw "package job обязан checkout'ить submodules: portable package копирует лицензии third_party."
}
Assert-Contains $workflow "actions/upload-artifact@v4" "передача результатов между jobs"
Assert-Contains $workflow "actions/download-artifact@v4" "получение результатов между jobs"
Assert-Contains $workflow "Restore PCRE2 cache" "независимый PCRE2 cache"
Assert-Contains $workflow "Restore Hunspell cache" "независимый Hunspell cache"
Assert-Contains $workflow "Restore Modern editor runtime cache" "независимый Modern runtime cache"
Assert-Contains $workflow "Restore Win7 editor runtime cache" "независимый Win7 runtime cache"
Assert-Contains $workflow "-PlatformToolset v143" "явный toolset MUI"
Assert-Contains $workflow "SkipVersionSync = `$true" "Modern CI не повторяет sync-version"
Assert-Contains $workflow "-SkipVersionSync" "Win7 CI не повторяет sync-version"
Assert-NotContains $workflow "Restore runtime dictionaries for release verification" "package: устаревшее восстановление словарей"
Assert-Contains $workflow "pcre2-release-win32-v143-vs2022" "ключ PCRE2 cache включает toolset/generator"
Assert-Contains $workflow "-hunspell-release-win32-v143-mt-" "ключ Hunspell cache включает toolset/properties"
Assert-Contains $workflow "ReusePreparedPcre2 = `$true" "PCRE2 reuse на exact cache hit"
Assert-Contains $workflow "github.event_name != 'pull_request'" "compiled artifact не загружается для PR"
Assert-Contains $workflow "-BatchOutputDirectory out/target-batches/Modern" "явный Modern batch output в CI"
Assert-Contains $workflow "-BatchOutputDirectory out/target-batches/Win7" "явный Win7 batch output в CI"
Assert-Contains $workflow "Verify Modern binaries" "Modern verification"
Assert-Contains $workflow "Verify Win7 binaries" "Win7 verification"
Assert-Contains $workflow "-SkipCommonChecks -BatchOutputDirectory out/target-batches/Win7" "Win7 verification без дублирования общих тестов"
Assert-Contains $workflow "-WarningsAsErrors" "Win7 batch warnings-as-errors"
Assert-NotContains $workflow "Copy-Item out/target-batches/Modern/* -Destination out/Release" "workflow: переключение Modern batch в общий output"
Assert-NotContains $workflow "Copy-Item out/target-batches/Win7/* -Destination out/Release" "workflow: переключение Win7 batch в общий output"
Assert-Contains $workflow "Build Win32 property handler" "подготовка Win32 property handler в build job"
Assert-Contains $workflow "Build x64 property handler" "подготовка x64 property handler в build job"
Assert-Contains $workflow "Build FBV Verb MUI" "подготовка FBV Verb MUI в build job"
Assert-Contains $workflow "Build Modern ArchHandler" "сборка Modern ArchHandler в build job"
Assert-Contains $workflow "Build Win7 ArchHandler" "сборка Win7 ArchHandler в build job"
Assert-Contains $workflow "-OutputDirectory out/archhandler/Modern/Win32/Release" "target-specific Modern ArchHandler"
Assert-Contains $workflow "-OutputDirectory out/archhandler/Win7/Win32/Release" "target-specific Win7 ArchHandler"
Assert-Contains $workflow "-ArchHandlerOutputDirectory out/archhandler/Modern/Win32/Release" "проверка Modern поставляемого ArchHandler"
Assert-Contains $workflow "-ArchHandlerOutputDirectory out/archhandler/Win7/Win32/Release" "проверка Win7 поставляемого ArchHandler"
Assert-Contains $workflow "out/archhandler" "передача ArchHandler между build и package jobs"
Assert-Contains $workflow "verify-nsis-layout.ps1" "однократная проверка NSIS в validate"
Assert-Contains $workflow "SkipArtifactVerification = `$true" "финальная artifact verification только после двух profile"
Assert-Contains $workflow "-SkipPropertyHandlerBuild" "Win7-релиз workflow"
Assert-Contains $workflow "-SkipFbvVerbMuiBuild" "Win7-релиз workflow"
Assert-Contains $build "[switch]`$EditorRuntimeOnly" "build.ps1"
Assert-Contains $build "Собраны только целевые DLL редактора" "build.ps1"
Assert-Contains $build "[switch]`$BatchConvertersOnly" "build.ps1"
Assert-Contains $build "[switch]`$SkipDependencies" "build.ps1"
Assert-Contains $build "Assert-PreparedDependencies" "build.ps1"
Assert-Contains $build "[switch]`$ForceRebuildRequiredProjects" "build.ps1"
Assert-Contains $build "[string]`$BatchOutputDirectory" "build.ps1: target-specific batch output"
Assert-Contains $build "[switch]`$ReusePreparedPcre2" "build.ps1: PCRE2 reuse"
Assert-Contains $build "[switch]`$SkipVersionSync" "build.ps1: CI-mode version sync"
Assert-Contains $build "-PlatformToolset `$PlatformToolset" "build.ps1: Scintilla получает toolset"
Assert-Contains $build "-PrepareOnly" "build.ps1: Hunspell preparation без отдельной компиляции"
Assert-Contains $build "Собраны только пакетные конвертеры" "build.ps1"
Assert-Contains $scintilla "[string]`$OutputDirectory" "build-scintilla.ps1"
Assert-Contains $scintilla "out\editor-runtime" "build-scintilla.ps1"
Assert-Contains $scintilla "[switch]`$ReusePreparedRuntime" "build-scintilla.ps1"
Assert-Contains $scintilla "[string]`$PlatformToolset" "build-scintilla.ps1: PlatformToolset"
Assert-Contains $scintilla "fbe-editor-runtime-fingerprint.json" "build-scintilla.ps1: runtime fingerprint"
Assert-Contains $scintilla "Test-PreparedRuntimeFingerprint" "build-scintilla.ps1: fingerprint validation"
Assert-Contains $scintilla "editor-runtime-helpers.ps1" "build-scintilla.ps1: editor-runtime helpers"
Assert-Contains $scintilla "Assert-LexillaSubmoduleCheckout" "build-scintilla.ps1: Lexilla gitlink validation"
Assert-Contains $editorRuntimeHelpers "Get-EditorDependencyVersion" "editor-runtime helpers: current editor dependency versions"
Assert-Contains $editorRuntimeHelpers '$Fingerprint.scintillaVersion -ne $ScintillaVersion' "editor-runtime helpers: Scintilla version fingerprint"
Assert-Contains $editorRuntimeHelpers '$Fingerprint.lexillaVersion -ne $LexillaVersion' "editor-runtime helpers: Lexilla version fingerprint"
Assert-Contains $scintilla "validated editor runtime cache hit" "build-scintilla.ps1: validated reuse log"
Assert-Contains $scintilla "nmake.exe=" "build-scintilla.ps1: toolchain diagnostics"
Assert-Contains $scintilla '"[17.0,18.0)"' "build-scintilla.ps1: v143 фиксирован на VS 2022"
Assert-Contains $release "[switch]`$SkipPropertyHandlerBuild" "create-release.ps1"
Assert-Contains $release "[switch]`$SkipFbvVerbMuiBuild" "create-release.ps1"
Assert-Contains $release "[switch]`$Prerelease" "create-release.ps1"
Assert-Contains $release "-EditorRuntimeDirectory `$editorRuntimeDirectory" "create-release.ps1"
Assert-Contains $release "[switch]`$SkipCommonChecks" "create-release.ps1"
Assert-Contains $release "[switch]`$SkipArtifactVerification" "create-release.ps1: финальная verification управляется CI"
Assert-Contains $release "[switch]`$SkipReleaseVerification" "create-release.ps1: package не повторяет compile-dependent verification"
Assert-Contains $release "[switch]`$SkipVersionSync" "create-release.ps1: CI не повторяет sync-version"
Assert-Contains $release "stage-core.ps1" "create-release.ps1: явный Core stage"
Assert-Contains $release "stage-integration.ps1" "create-release.ps1: явный Integration stage"
Assert-Contains $release "-CoreDirectory `$coreDir" "create-release.ps1: portable получает готовый Core"
Assert-Contains $release "-IntegrationDirectory `$integrationDir" "create-release.ps1: installer получает готовую Integration"
Assert-Contains $release "-SkipBuild запрещает native-компиляцию" "create-release.ps1: строгая семантика SkipBuild"
Assert-Contains $release "out\artifacts\{0}" "изолированные артефакты release"
Assert-NotContains $release '& (Join-Path $repoRoot "tools\tests\test-spellcheck-dictionaries.ps1")' "create-release.ps1"
Assert-Contains $portable "[Parameter(Mandatory)][string]`$CoreDirectory" "package-portable.ps1: mandatory Core input"
Assert-NotContains $portable "build-fbv-verb-mui.ps1" "package-portable.ps1: packaging не строит MUI"
Assert-NotContains $portable "build-experimental-property-handler.ps1" "package-portable.ps1: packaging не строит property handler"
Assert-Contains $stageCore "Core stage prepared" "stage-core.ps1"
Assert-Contains $stageIntegration "Integration stage prepared" "stage-integration.ps1"
Assert-Contains $manifest '"core"' "package manifest: Core"
Assert-Contains $manifest '"integration"' "package manifest: Integration"
Assert-Contains $manifest '"portable"' "package manifest: Portable"
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
Assert-Contains $verifyRelease "test-editor-runtime-fingerprint.ps1" "verify-release.ps1: behavioral editor-runtime fingerprint regression"
Assert-Contains $verifyRelease "test-fbe-selection-container-control-range.ps1" "verify-release.ps1: MSHTML ControlRange selection regression"
Assert-Contains $verifyRelease "test-fbe-table-structural-performance.ps1" "verify-release.ps1: benchmark logical grid"
Assert-Contains $verifyRelease '-FixtureId edge-spans -Target "0,3"' "verify-release.ps1: targeted last colspan"
Assert-Contains $verifyRelease "-SecondOperation insert-column-right" "verify-release.ps1: span second operation"
Assert-Contains $verifyRelease '-FixtureId bulk-10x10 -Target "0,0:9,9" -Operation make-header' "verify-release.ps1: 100-cell Make Header"
Assert-Contains $verifyRelease '-FixtureId bulk-header-10x10 -Target "0,0:9,9" -Operation make-normal' "verify-release.ps1: 100-cell Make Normal"
Assert-Contains $verifyRelease '-FixtureId all-header -Target "0,0" -Operation make-header' "verify-release.ps1: идемпотентный Make Header"
Assert-Contains $verifyRelease '-FixtureId plain -Target "0,0" -Operation make-normal' "verify-release.ps1: идемпотентный Make Normal"
Assert-Contains $verifyRelease '-FixtureId mixed -Target "0,0:2,2" -Operation make-header' "verify-release.ps1: mixed Make Header"
Assert-Contains $verifyRelease '-FixtureId mixed -Target "0,0:2,2" -Operation make-normal' "verify-release.ps1: mixed Make Normal"
Assert-Contains $verifyRelease '-FixtureId mixed -Target "1,0:1,2" -Operation make-header' "verify-release.ps1: Make Header для целой строки"
Assert-Contains $verifyRelease '-FixtureId mixed -Target "0,0:2,0" -Operation make-normal' "verify-release.ps1: Make Normal для целого столбца"
Assert-Contains $verifyRelease '-FixtureId preserve -Target "0,0" -Operation make-header' "verify-release.ps1: preservation TD to TH"
Assert-Contains $verifyRelease '-FixtureId preserve-header -Target "0,0" -Operation make-normal' "verify-release.ps1: preservation TH to TD"
Assert-Contains $verifyRelease '-FixtureId toggle-preserve -Target "0,0" -Operation toggle-header -RuntimeStyle "width: 37px"' "verify-release.ps1: single TD to TH runtime style"
Assert-Contains $verifyRelease '-FixtureId toggle-preserve-header -Target "0,0" -Operation toggle-header -RuntimeStyle "width: 37px"' "verify-release.ps1: single TH to TD runtime style"
Assert-Contains $verifyRelease "test-fbe-test-report-diagnostics.ps1" "verify-release.ps1: честный HRESULT в тестовых отчётах"
Assert-Contains $verifyRelease "test-fb2-schema-metadata-culture.ps1" "verify-release.ps1: culture-независимые метаданные FB2"
Assert-Contains $verifyRelease "test-image-codec-build-contract.ps1" "verify-release.ps1: image codec VS/CMake contract"
Assert-Contains $verifyRelease "test-source-scintilla-modern-features.ps1" "verify-release.ps1: modern Scintilla Source contract"
Assert-Contains $verifyRelease "test-fb2-source-autocomplete.ps1" "verify-release.ps1: behavioral FB2 autocomplete"
Assert-Contains $verifyRelease "test-fb2-source-structural-context.ps1" "verify-release.ps1: behavioral FB2 structural context"
Assert-Contains $verifyRelease "test-source-eol-annotations.ps1" "verify-release.ps1: Source EOL validation annotations"
Assert-Contains $verifyRelease "test-source-special-representations.ps1" "verify-release.ps1: Source special-character representations"
Assert-Contains $verifyRelease "test-source-allocate-lines.ps1" "verify-release.ps1: Source line-index preallocation"
Assert-Contains $verifyRelease "[string]`$BatchOutputDirectory" "verify-release.ps1: target-specific batch output"
Assert-Contains $verifyRelease "UsePreparedPcre2 = `$true" "verify-release.ps1: prepared PCRE2 smoke tests"
Assert-Contains $verifyRelease "фактически поставляемая DLL property handler" "verify-release.ps1: поставляемые FBShell DLL"
Assert-Contains $verifyRelease "Machine = [UInt16]0x8664" "verify-release.ps1: x64 FBShell architecture"
Assert-NotContains $verifyRelease "test-release-pipeline-deduplication.ps1" "verify-release.ps1: structural regression выполняется только в validate"
Assert-Contains $verifyRelease '-EditorRuntimeDirectory (Join-Path $repoRoot "out\editor-runtime\$CompatibilityTarget")' "verify-release.ps1"
Assert-Contains $archHandlerArgv "Get-Command cl.exe" "ArchHandler argv test: toolchain для receiver"
Assert-Contains $archHandlerArgv "Import-VsDevEnvironment.ps1" "ArchHandler argv test: загрузка toolchain"
Assert-Contains $archHandlerBuild "/utf-8" "ArchHandler: Unicode-сообщения собираются с UTF-8"
Assert-Contains $verifyRelease "target-specific executable" "verify-release.ps1: ArchHandler вне common checks"
Assert-Contains $verifyRelease 'IncludeNames @("ZipHandler.exe", "RarHandler.exe")' "verify-release.ps1: Win7 imports ArchHandler"

$commonStart = $verifyRelease.IndexOf('if (-not $SkipCommonChecks) {')
$commonEnd = $verifyRelease.IndexOf("`n}`r`n", $commonStart)
$archHandlerTest = $verifyRelease.IndexOf('test-archhandler-argv.ps1')
if ($commonStart -lt 0 -or $commonEnd -lt 0 -or $archHandlerTest -le $commonEnd) {
    throw "Exact ArchHandler argv test должен выполняться вне блока SkipCommonChecks."
}
Assert-NotContains $fbeView "GetTestLogicalTableColumn" "FBEview.cpp: test-only column override в production handler"
Assert-Contains $fbeView "DeleteTableLogicalColumnForTest" "FBEview.cpp: isolated test harness helper"
Assert-Contains $fbeView "DeleteTableLogicalColumn(this, grid" "FBEview.cpp: общий алгоритм удаления logical column"
Assert-Contains $fbeView "kTableCellReplacementAttributes" "FBEview.cpp: общий набор attributes для TD/TH"
Assert-NotContains $fbeView 'const wchar_t* const attributes[] = { L"id", L"fbstyle"' "FBEview.cpp: single TD/TH не использует неполный attributes list"
Assert-NotContains $fbeView "if (iswspace(ch)) continue;" "FBEview.cpp: content hash не игнорирует пробелы"
Assert-NotContains $fbeView "towlower(ch)" "FBEview.cpp: content hash не игнорирует регистр"
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
Assert-Contains $pcre2Build "[switch]`$ReusePreparedPcre2" "build-pcre2.ps1: режим validated reuse"
Assert-Contains $pcre2Build '"-T", $PlatformToolset' "build-pcre2.ps1: CMake получает явный toolset"
Assert-Contains $pcre2Build "fbe-pcre2-fingerprint.json" "build-pcre2.ps1: fingerprint кэша"
Assert-Contains $pcre2Build "exact validated cache hit" "build-pcre2.ps1: CMake пропускается при exact hit"
Assert-Contains $pcre2Build '"[17.0,18.0)"' "build-pcre2.ps1: v143 фиксирован на VS 2022"
Assert-Contains $hunspellBuild "[switch]`$PrepareOnly" "build-hunspell.ps1: PrepareOnly"
Assert-Contains $muiBuild "[string]`$PlatformToolset" "build-fbv-verb-mui.ps1: PlatformToolset"
Assert-Contains $muiBuild "FBV Verb MUI toolchain" "build-fbv-verb-mui.ps1: toolchain diagnostics"
Assert-Contains $importVsEnvironment "[string]`$PlatformToolset" "Import-VsDevEnvironment.ps1: PlatformToolset"
Assert-Contains $importVsEnvironment '"[17.0,18.0)"' "Import-VsDevEnvironment.ps1: v143 фиксирован на VS 2022"

foreach ($batchProjectPath in @(
    "src\export-docx\ExportDOCXBatch.vcxproj",
    "src\export-epub\ExportEPUBBatch.vcxproj",
    "src\import-epub\ImportEPUBBatch.vcxproj"
)) {
    Assert-Contains (Get-Text $batchProjectPath) "BatchOutputDirectory" "${batchProjectPath}: target-specific OutDir"
}

foreach ($pcre2TestPath in @("tools\tests\test-pcre2.ps1", "tools\tests\test-pcre2-wrapper.ps1", "tools\tests\test-pcre2-replace.ps1")) {
    Assert-Contains (Get-Text $pcre2TestPath) "[switch]`$UsePreparedPcre2" "${pcre2TestPath}: prepared PCRE2 mode"
    Assert-Contains (Get-Text $pcre2TestPath) "-PlatformToolset `$PlatformToolset" "${pcre2TestPath}: compiler environment соответствует toolset"
}

Write-Host "Проверка исключения повторных сборок Modern/Win7 прошла успешно."
