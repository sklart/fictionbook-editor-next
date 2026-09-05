[CmdletBinding()]
param(
    [string]$Configuration = "Release",

    [string]$PlatformToolset = 'v143',

    [string]$BatchOutputDirectory,

    [string]$ArchHandlerOutputDirectory,

    [switch]$SkipUpdateManifest,

    # Table regressions are intentionally opt-in while portable finalization is
    # in progress. They remain available for their dedicated test contour.
    [switch]$RunTableTests,

    # FAST is the default validation contour. FULL adds GUI, production,
    # stress, benchmark and table regressions for a manual/release gate.
    [switch]$FullValidation
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$outputDir = Join-Path $repoRoot "out\$Configuration"
$runtimeXsl = Join-Path $repoRoot 'runtime\html.xsl'
$stagedXsl = Join-Path $outputDir 'html.xsl'
if (-not (Test-Path -LiteralPath $stagedXsl -PathType Leaf)) { throw "Staged runtime is stale: missing html.xsl at $stagedXsl" }
if ((Get-FileHash -LiteralPath $runtimeXsl -Algorithm SHA256).Hash -ne (Get-FileHash -LiteralPath $stagedXsl -Algorithm SHA256).Hash) {
    throw "Staged runtime is stale: html.xsl differs from runtime/html.xsl."
}
$batchOutputDir = if ($BatchOutputDirectory) {
    $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($BatchOutputDirectory)
} else {
    $outputDir
}
$archHandlerOutputDir = if ($ArchHandlerOutputDirectory) {
    $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($ArchHandlerOutputDirectory)
} else {
    Join-Path $repoRoot "out\archhandler\Win32\$Configuration"
}
$batchNames = @("ExportDOCXBatch.exe", "ExportEPUBBatch.exe", "ImportEPUBBatch.exe", "ExportDOCXBatch.pdb", "ExportEPUBBatch.pdb", "ImportEPUBBatch.pdb")
$pluginDllNames = @("ExportHTML.dll", "ExportDOCX.dll", "ExportEPUB.dll", "ImportEPUB.dll", "ImportEPUBLunaSVG.dll")
$pluginSymbolNames = @("ExportHTML.pdb", "ExportDOCX.pdb", "ExportEPUB.pdb", "ImportEPUB.pdb", "ImportEPUBLunaSVG.pdb")
function Get-ReleaseOutputPath([string]$Name) {
    $directory = if ($Name -in $batchNames) { $batchOutputDir } elseif ($Name -in ($pluginDllNames + $pluginSymbolNames)) { Join-Path $outputDir 'Plugins' } else { $outputDir }
    return Join-Path $directory $Name
}
$versionHeader = Get-Content -Raw -LiteralPath (Join-Path $repoRoot "src\version.h")
$versionMatch = [regex]::Match(
    $versionHeader,
    '#define\s+FBE_VERSION_STRING\s+"(?<version>\d+\.\d+\.\d+)"'
)

if (-not $versionMatch.Success) {
    throw "Не найден FBE_VERSION_STRING."
}

$expectedVersion = $versionMatch.Groups["version"].Value
$runTables = $RunTableTests -or $FullValidation

# Fail before the long GUI suite if this invocation is looking at stale output.
& (Join-Path $PSScriptRoot 'build-provenance.ps1') -Action Validate -Kind CommonCore `
    -Configuration $Configuration -CommonDirectory $outputDir
& (Join-Path $PSScriptRoot 'build-provenance.ps1') -Action Validate -Kind Runtime `
    -Configuration $Configuration -ProfileDirectory (Join-Path $repoRoot "out\editor-runtime") `
    -BatchDirectory $batchOutputDir -ArchHandlerDirectory $archHandlerOutputDir
& (Join-Path $repoRoot 'tools\tests\test-development-plugin-layout.ps1') `
    -Configuration $Configuration -OutputDirectory $outputDir -BatchOutputDirectory $batchOutputDir
& (Join-Path $repoRoot 'tools\tests\test-batch-interactive-launch.ps1')

$requiredFiles = @(
    "FBE.exe",
    "FBV.exe",
    "ExportHTML.dll",
    "ExportDOCX.dll",
    "ExportEPUB.dll",
    "ImportEPUB.dll",
    "ImportEPUBLunaSVG.dll",
    "ExportDOCXBatch.exe",
    "ExportEPUBBatch.exe",
    "ImportEPUBBatch.exe",
    "FBShell.dll",
    "Scintilla.dll",
    "Lexilla.dll"
)
$forbiddenFiles = @(
    "pcre.dll",
    "res_rus.dll",
    "res_ukr.dll",
    "res_rus.pdb",
    "res_ukr.pdb"
)

$requiredSymbols = @(
    "FBE.pdb",
    "FBV.pdb",
    "ExportHTML.pdb",
    "ExportDOCX.pdb",
    "ExportEPUB.pdb",
    "ImportEPUB.pdb",
    "ImportEPUBLunaSVG.pdb",
    "ExportDOCXBatch.pdb",
    "ExportEPUBBatch.pdb",
    "ImportEPUBBatch.pdb",
    "FBShell.pdb"
)

& (Join-Path $repoRoot "tools\tests\test-update-manifest-candidate.ps1")
& (Join-Path $repoRoot "tools\tests\test-release-test-catalog.ps1")
& (Join-Path $repoRoot "tools\tests\test-build-provenance.ps1")
& (Join-Path $repoRoot "tools\tests\test-resource-id-safety.ps1")
& (Join-Path $repoRoot "tools\tests\test-source-safety.ps1")
& (Join-Path $repoRoot "tools\tests\test-no-tracked-local-build-artifacts.ps1")
& (Join-Path $repoRoot "tools\tests\test-source-line-number-margin.ps1")
& (Join-Path $repoRoot "tools\tests\test-source-updateui-notification.ps1")
& (Join-Path $repoRoot "tools\tests\test-source-scintilla-modern-features.ps1")
& (Join-Path $repoRoot "tools\tests\test-fbd-support-contract.ps1")
& (Join-Path $repoRoot "tools\tests\test-fb2-schema-metadata.ps1")
& (Join-Path $repoRoot "tools\tests\test-fb2-schema-metadata-culture.ps1")
& (Join-Path $repoRoot "tools\tests\test-fb2-common-boundary.ps1")
& (Join-Path $repoRoot "tools\tests\test-first-party-msbuild-policy.ps1")
& (Join-Path $repoRoot "tools\tests\test-fbe-contract-generation.ps1") -Configuration $Configuration
& (Join-Path $repoRoot "tools\tests\test-fbe-plugin-host-boundary.ps1")
& (Join-Path $repoRoot "tools\tests\test-fbe-source-helpers-boundary.ps1")
& (Join-Path $repoRoot "tools\tests\test-fbe-search-boundary.ps1")
& (Join-Path $repoRoot "tools\tests\test-fbe-settings-background-boundary.ps1")
& (Join-Path $repoRoot "tools\tests\test-package-layout.ps1")
& (Join-Path $repoRoot "tools\tests\test-package-layout-copy.ps1")
& (Join-Path $repoRoot "tools\tests\test-package-layout-integration-stage.ps1")
& (Join-Path $repoRoot "tools\tests\test-package-layout-core-stage.ps1")
& (Join-Path $repoRoot "tools\tests\test-fb2-source-structural-context.ps1")
& (Join-Path $repoRoot "tools\tests\test-fb2-source-autocomplete.ps1")
& (Join-Path $repoRoot "tools\tests\test-source-eol-annotations.ps1")
& (Join-Path $repoRoot "tools\tests\test-source-special-representations.ps1")
& (Join-Path $repoRoot "tools\tests\test-fbe-source-xml-declaration.ps1")
& (Join-Path $repoRoot "tools\tests\test-source-allocate-lines.ps1")
& (Join-Path $repoRoot "tools\tests\test-editor-runtime-fingerprint.ps1")
& (Join-Path $repoRoot "tools\tests\test-editor-background-assets.ps1") -RuntimeDirectory (Join-Path $outputDir "EditorBackgrounds")
& (Join-Path $repoRoot "tools\tests\test-editor-background-settings.ps1") -FbeExe (Join-Path $outputDir "FBE.exe")
& (Join-Path $repoRoot "tools\tests\test-editor-background-regression.ps1")
& (Join-Path $repoRoot "tools\tests\test-editor-background-runtime.ps1") -FbeExe (Join-Path $outputDir "FBE.exe")
& (Join-Path $repoRoot "tools\tests\test-customizable-toolbar-contract.ps1")
& (Join-Path $repoRoot "tools\tests\test-fbe-table-visual-mode.ps1")
& (Join-Path $repoRoot "tools\tests\test-table-toolbar-contract.ps1")
& (Join-Path $repoRoot "tools\tests\test-ui-metrics-contract.ps1")
& (Join-Path $repoRoot "tools\tests\test-fbe-script-document-path-api.ps1")
& (Join-Path $repoRoot "tools\tests\test-fbe-backup-settings.ps1")
& (Join-Path $repoRoot "tools\tests\test-fbe-auto-url-detect.ps1")
& (Join-Path $repoRoot "tools\tests\test-xml-source-themes.ps1")
& (Join-Path $repoRoot "tools\tests\test-xml-source-current-line.ps1")
& (Join-Path $repoRoot "tools\tests\test-fbe-filename-state.ps1")
if ($runTables) {
& (Join-Path $repoRoot "tools\tests\test-fbe-table-toolbar-rendering.ps1") -FbeExe (Join-Path $outputDir "FBE.exe")
& (Join-Path $repoRoot "tools\tests\test-fbe-table-production-roundtrip.ps1") -FbeExe (Join-Path $outputDir "FBE.exe")
& (Join-Path $repoRoot "tools\tests\test-fbe-table-production-roundtrip.ps1") -FbeExe (Join-Path $outputDir "FBE.exe") -Huge
& (Join-Path $repoRoot "tools\tests\test-fbe-table-structural-production.ps1") -FbeExe (Join-Path $outputDir "FBE.exe")
foreach ($commandRouteOperation in @('insert-row-above','insert-row-below','delete-row','insert-column-left','insert-column-right','delete-column')) {
& (Join-Path $repoRoot "tools\tests\test-fbe-table-structural-production.ps1") -FbeExe (Join-Path $outputDir "FBE.exe") -FixtureId plain -Operation $commandRouteOperation -RouteThroughFrame
}
& (Join-Path $repoRoot "tools\tests\test-fbe-table-structural-production.ps1") -FbeExe (Join-Path $outputDir "FBE.exe") -FixtureId plain -Target "1,1" -Operation insert-column-left
& (Join-Path $repoRoot "tools\tests\test-fbe-table-structural-production.ps1") -FbeExe (Join-Path $outputDir "FBE.exe") -FixtureId edge-spans -Target "0,3" -Operation delete-column
& (Join-Path $repoRoot "tools\tests\test-fbe-table-structural-production.ps1") -FbeExe (Join-Path $outputDir "FBE.exe") -FixtureId rowspan -Target "1,0" -Operation insert-row-below
& (Join-Path $repoRoot "tools\tests\test-fbe-table-structural-production.ps1") -FbeExe (Join-Path $outputDir "FBE.exe") -FixtureId combined -Target "1,0" -Operation delete-row
& (Join-Path $repoRoot "tools\tests\test-fbe-table-structural-production.ps1") -FbeExe (Join-Path $outputDir "FBE.exe") -FixtureId colspan -Target "0,0" -Operation delete-column -SecondOperation insert-column-right
& (Join-Path $repoRoot "tools\tests\test-fbe-table-structural-production.ps1") -FbeExe (Join-Path $outputDir "FBE.exe") -FixtureId rowspan -Target "0,0" -Operation delete-row -SecondOperation insert-row-below
& (Join-Path $repoRoot "tools\tests\test-fbe-table-structural-production.ps1") -FbeExe (Join-Path $outputDir "FBE.exe") -FixtureId bulk-10x10 -Target "0,0:9,9" -Operation make-header
& (Join-Path $repoRoot "tools\tests\test-fbe-table-structural-production.ps1") -FbeExe (Join-Path $outputDir "FBE.exe") -FixtureId bulk-header-10x10 -Target "0,0:9,9" -Operation make-normal
& (Join-Path $repoRoot "tools\tests\test-fbe-table-structural-production.ps1") -FbeExe (Join-Path $outputDir "FBE.exe") -FixtureId all-header -Target "0,0" -Operation make-header
& (Join-Path $repoRoot "tools\tests\test-fbe-table-structural-production.ps1") -FbeExe (Join-Path $outputDir "FBE.exe") -FixtureId plain -Target "0,0" -Operation make-normal
& (Join-Path $repoRoot "tools\tests\test-fbe-table-structural-production.ps1") -FbeExe (Join-Path $outputDir "FBE.exe") -FixtureId mixed -Target "0,0:2,2" -Operation make-header
& (Join-Path $repoRoot "tools\tests\test-fbe-table-structural-production.ps1") -FbeExe (Join-Path $outputDir "FBE.exe") -FixtureId mixed -Target "0,0:2,2" -Operation make-normal
& (Join-Path $repoRoot "tools\tests\test-fbe-table-structural-production.ps1") -FbeExe (Join-Path $outputDir "FBE.exe") -FixtureId mixed -Target "1,0:1,2" -Operation make-header
& (Join-Path $repoRoot "tools\tests\test-fbe-table-structural-production.ps1") -FbeExe (Join-Path $outputDir "FBE.exe") -FixtureId mixed -Target "0,0:2,0" -Operation make-normal
& (Join-Path $repoRoot "tools\tests\test-fbe-table-structural-production.ps1") -FbeExe (Join-Path $outputDir "FBE.exe") -FixtureId preserve -Target "0,0" -Operation make-header
& (Join-Path $repoRoot "tools\tests\test-fbe-table-structural-production.ps1") -FbeExe (Join-Path $outputDir "FBE.exe") -FixtureId preserve-header -Target "0,0" -Operation make-normal
& (Join-Path $repoRoot "tools\tests\test-fbe-table-structural-production.ps1") -FbeExe (Join-Path $outputDir "FBE.exe") -FixtureId toggle-preserve -Target "0,0" -Operation toggle-header -RuntimeStyle "width: 37px"
& (Join-Path $repoRoot "tools\tests\test-fbe-table-structural-production.ps1") -FbeExe (Join-Path $outputDir "FBE.exe") -FixtureId toggle-preserve-header -Target "0,0" -Operation toggle-header -RuntimeStyle "width: 37px"
& (Join-Path $repoRoot "tools\tests\test-fbe-table-structural-performance.ps1") -FbeExe (Join-Path $outputDir "FBE.exe")
& (Join-Path $repoRoot "tools\tests\test-fbe-table-failure-safety.ps1") -FbeExe (Join-Path $outputDir "FBE.exe")
& (Join-Path $repoRoot "tools\tests\test-fbe-table-failure-safety.ps1") -FbeExe (Join-Path $outputDir "FBE.exe") -Fault change-colspan-after-normalize
} else {
    Write-Host "Table production checks are not run by default; use -RunTableTests or -FullValidation to enable them."
}
& (Join-Path $repoRoot "tools\tests\test-fbe-test-report-diagnostics.ps1")
& (Join-Path $repoRoot "tools\tests\test-fbe-binary-serialization.ps1")
& (Join-Path $repoRoot "tools\tests\test-fbe-binary-save.ps1")
& (Join-Path $repoRoot "tools\tests\test-fbe-binary-save-runtime.ps1")
& (Join-Path $repoRoot "tools\tests\test-image-import-contract.ps1")
& (Join-Path $repoRoot "tools\tests\test-fbe-main-js-reliability.ps1")
& (Join-Path $repoRoot "tools\tests\test-fbe-main-js-binary-behavior.ps1")
& (Join-Path $repoRoot "tools\tests\test-note-preview-regression.ps1")
& (Join-Path $repoRoot "tools\tests\test-link-navigation.ps1")
& (Join-Path $repoRoot "tools\tests\test-fbe-table-mouse-selection-contract.ps1")
& (Join-Path $repoRoot "tools\tests\test-fbe-binary-editor-localization.ps1")
& (Join-Path $repoRoot "tools\tests\test-image-codec-build-contract.ps1")
$imageImportTestArguments = @{ Configuration = $Configuration }
if ($PlatformToolset) { $imageImportTestArguments.PlatformToolset = $PlatformToolset }
& (Join-Path $repoRoot "tools\tests\test-image-import-native.ps1") @imageImportTestArguments
& (Join-Path $repoRoot "tools\tests\test-archhandler-reset-contract.ps1")
& (Join-Path $repoRoot "tools\tests\test-archhandler-reset-behavior.ps1")
& (Join-Path $repoRoot "tools\tests\test-fb2-check-content-types-base64.ps1")
& (Join-Path $repoRoot "tools\tests\test-fb2recode-cp1251.ps1")
& (Join-Path $repoRoot "tools\tests\test-fb2recode-cancel.ps1")
& (Join-Path $repoRoot "tools\tests\test-save-sections-safe-replacement.ps1")
& (Join-Path $repoRoot "tools\tests\test-hta-legacy-js.ps1")
if (-not $SkipUpdateManifest) {
    & (Join-Path $repoRoot "tools\tests\test-update-manifest.ps1")
}
& (Join-Path $repoRoot "tools\tests\test-spellcheck-dictionaries.ps1") -Configuration $Configuration
& (Join-Path $repoRoot "tools\tests\test-spell-visible-paragraphs.ps1")
$pcre2TestArguments = @{
    Configuration = $Configuration
    UsePreparedPcre2 = $true
}
if ($PlatformToolset) {
    $pcre2TestArguments.PlatformToolset = $PlatformToolset
}
& (Join-Path $repoRoot "tools\tests\test-pcre2.ps1") @pcre2TestArguments
& (Join-Path $repoRoot "tools\tests\test-pcre2-match-loop.ps1") @pcre2TestArguments
& (Join-Path $repoRoot "tools\tests\test-pcre2-wrapper.ps1") @pcre2TestArguments
& (Join-Path $repoRoot "tools\tests\test-pcre2-replace.ps1") @pcre2TestArguments
& (Join-Path $repoRoot "tools\tests\test-pcre2-cache.ps1") @pcre2TestArguments
& (Join-Path $repoRoot "tools\tests\test-export-epub-cyrillic.ps1") -Configuration $Configuration
& (Join-Path $repoRoot 'tools\tests\test-import-epub-batch-dll-abi.ps1') `
    -DllPath (Join-Path $outputDir 'Plugins\ImportEPUB.dll') `
    -BatchPath (Join-Path $batchOutputDir 'ImportEPUBBatch.exe') `
    -SmokeEpubPath (Join-Path $repoRoot 'out\tests\export-epub-cyrillic\fb2-metadata-cyrillic-smoke.epub')
& (Join-Path $repoRoot "tools\tests\test-export-epub-xhtml11.ps1") -Configuration $Configuration
& (Join-Path $repoRoot "tools\tests\test-plugin-mojibake.ps1")
& (Join-Path $repoRoot "tools\tests\test-plugin-static-runtime.ps1")
& (Join-Path $repoRoot "tools\tests\test-plugin-per-user-registration.ps1")
& (Join-Path $repoRoot "tools\tests\test-plugin-manifest-contract.ps1")
& (Join-Path $repoRoot "tools\tests\test-plugin-missing-module.ps1")
& (Join-Path $repoRoot "tools\tests\test-plugin-command-ranges.ps1")
& (Join-Path $repoRoot "tools\tests\test-last-plugin-routing.ps1")
& (Join-Path $repoRoot "tools\tests\test-plugin-v2-abi.ps1") -Configuration $Configuration
& (Join-Path $repoRoot "tools\tests\test-plugin-v2-negotiation-policy.ps1")
& (Join-Path $repoRoot "tools\tests\test-export-html-v2-runtime.ps1") -Configuration $Configuration
& (Join-Path $repoRoot "tools\tests\test-export-epub-v2-runtime.ps1") -Configuration $Configuration
& (Join-Path $repoRoot "tools\tests\test-export-docx-v2-runtime.ps1") -Configuration $Configuration
& (Join-Path $repoRoot "tools\tests\test-import-epub-v2-runtime.ps1") -Configuration $Configuration
# В исходниках ещё есть накопленный исторический набор строк, который будет
# переноситься в JSON-локализации поэтапно. В release-контуре аудит остаётся
# видимым, но не должен блокировать выпуск до фиксации отдельного эталона.
# Строгий режим -FailOnFindings используется в узких regression-fixture.
& (Join-Path $repoRoot "tools\localization\analyze-product-hardcoded-cyrillic.ps1")
& (Join-Path $repoRoot "tools\tests\test-product-hardcoded-cyrillic-audit.ps1")
& (Join-Path $repoRoot "tools\tests\test-release-notes-format.ps1")
& (Join-Path $repoRoot "tools\tests\test-plugin-localization-catalog.ps1")
& (Join-Path $repoRoot "tools\tests\test-fbv-localization-resources.ps1")
& (Join-Path $repoRoot "tools\tests\test-fbe-property-schema-localization.ps1")
& (Join-Path $repoRoot "tools\tests\test-export-html-localization-resources.ps1")
& (Join-Path $repoRoot "tools\tests\test-export-html-standalone.ps1")
& (Join-Path $repoRoot "tools\tests\test-export-html-template-selection.ps1")
& (Join-Path $repoRoot "tools\tests\test-export-html-template-resolver-native.ps1")
& (Join-Path $repoRoot "tools\tests\test-export-html-modes.ps1")
& (Join-Path $repoRoot "tools\tests\test-export-docx-localization-resources.ps1")
& (Join-Path $repoRoot "tools\tests\test-export-epub-localization-resources.ps1")
& (Join-Path $repoRoot "tools\tests\test-import-epub-localization-resources.ps1")
& (Join-Path $repoRoot "tools\tests\test-localization-export.ps1")
& (Join-Path $repoRoot "tools\tests\test-fbe-main-menu-catalog.ps1")
& (Join-Path $repoRoot "tools\tests\test-fbe-main-menu-mnemonics.ps1")
& (Join-Path $repoRoot "tools\tests\test-fbe-runtime-dialog-coverage.ps1")
& (Join-Path $repoRoot "tools\tests\test-fbe-dialog-layout-contract.ps1")
& (Join-Path $repoRoot "tools\tests\test-no-fbe-locale-resource-dll.ps1")
& (Join-Path $repoRoot "tools\tests\test-localization-runtime-contract.ps1")
& (Join-Path $repoRoot "tools\tests\test-runtime-interface-language-contract.ps1")
& (Join-Path $repoRoot "tools\tests\test-fbe-next-isolation.ps1")
& (Join-Path $repoRoot "tools\tests\test-portable-deployment-contract.ps1")
& (Join-Path $repoRoot "tools\tests\test-portable-scripts-infrastructure.ps1")
& (Join-Path $repoRoot "tools\tests\test-runtime-paths-cli.ps1") -FbeExecutable (Join-Path $outputDir "FBE.exe")
& (Join-Path $repoRoot "tools\tests\test-portable-copies-isolation.ps1") -FbeExecutable (Join-Path $outputDir "FBE.exe")
& (Join-Path $repoRoot "tools\tests\test-portable-atomic-persistence.ps1")
& (Join-Path $repoRoot "tools\tests\test-mode-aware-updater.ps1")
& (Join-Path $repoRoot "tools\tests\test-mode-aware-updater-behavior.ps1")
& (Join-Path $repoRoot "tools\tests\test-runtime-lang-export.ps1")
& (Join-Path $repoRoot "tools\tests\test-runtime-lang-output-layout.ps1") -Configuration $Configuration -OutputDirectory $outputDir
& (Join-Path $repoRoot "tools\tests\test-fbe-runtime-lang-overlay.ps1")
& (Join-Path $repoRoot "tools\tests\test-status-bar-contract.ps1")
& (Join-Path $repoRoot "tools\tests\test-status-bar-unicode.ps1")
& (Join-Path $repoRoot "tools\tests\test-status-bar-behavior.ps1")
& (Join-Path $repoRoot "tools\tests\test-search-viewport-position.ps1")
& (Join-Path $repoRoot "tools\tests\test-fbe-body-source-selection-transfer.ps1")
& (Join-Path $repoRoot "tools\tests\test-fbe-body-source-selection-transfer-behavior.ps1")
& (Join-Path $repoRoot "tools\tests\test-fbv-runtime-lang-overlay.ps1")
& (Join-Path $repoRoot "tools\tests\test-export-html-runtime-lang-overlay.ps1")
& (Join-Path $repoRoot "tools\tests\test-import-epub-runtime-lang-overlay.ps1")
& (Join-Path $repoRoot "tools\tests\test-export-epub-runtime-lang-overlay.ps1")
& (Join-Path $repoRoot "tools\tests\test-export-docx-runtime-lang-overlay.ps1")
& (Join-Path $repoRoot "tools\tests\test-language-packs-inventory.ps1")
& (Join-Path $repoRoot "tools\tests\test-nsis-language-pack-plan.ps1")
& (Join-Path $repoRoot "tools\tests\test-nsis-installer-language-fallbacks.ps1")
& (Join-Path $repoRoot "tools\tests\test-nsis-installer-catalog.ps1")
& (Join-Path $repoRoot "tools\tests\test-nsis-components-page-layout.ps1")
& (Join-Path $repoRoot "tools\tests\test-nsis-deployment-modes.ps1")
& (Join-Path $repoRoot "tools\tests\test-nsis-install-scopes.ps1")
& (Join-Path $repoRoot "tools\tests\test-nsis-ownership-safe-uninstall.ps1")
& (Join-Path $repoRoot "tools\tests\test-nsis-uninstall-user-data.ps1")
& (Join-Path $repoRoot "tools\tests\test-bundled-plugin-local-activation.ps1") -Configuration $Configuration -RuntimeDirectory $outputDir
& (Join-Path $repoRoot "tools\tests\test-import-epub-registration.ps1") -Configuration $Configuration

if ($FullValidation) {
    Write-Host 'Running FULL GUI, production, stress and benchmark validation.'
    & (Join-Path $repoRoot "tools\tests\test-fbd-production-roundtrip.ps1") -FbeExe (Join-Path $outputDir "FBE.exe")
    & (Join-Path $repoRoot "tools\tests\test-source-full-process-benchmark.ps1")
    & (Join-Path $repoRoot "tools\tests\test-words-ownerdata-stress.ps1")
    & (Join-Path $repoRoot "tools\tests\test-fbe-selection-container-control-range.ps1")
    & (Join-Path $repoRoot "tools\tests\test-fbe-binary-production-roundtrip.ps1") -FbeExe (Join-Path $outputDir "FBE.exe")
    & (Join-Path $repoRoot "tools\tests\test-fbe-large-binary-production-roundtrip.ps1") -FbeExe (Join-Path $outputDir "FBE.exe")
    & (Join-Path $repoRoot "tools\tests\test-fbe-many-binaries-production-stress.ps1") -FbeExe (Join-Path $outputDir "FBE.exe")
    & (Join-Path $repoRoot "tools\tests\test-fbe-image-import-generated-id-production.ps1") -FbeExe (Join-Path $outputDir "FBE.exe")
    & (Join-Path $repoRoot "tools\tests\test-fbe-spellcheck-local-edit-performance.ps1") -FbeExe (Join-Path $outputDir "FBE.exe")
    & (Join-Path $repoRoot "tools\tests\test-image-import-fbe-roundtrip.ps1") -FbeExe (Join-Path $outputDir "FBE.exe")
    & (Join-Path $repoRoot "tools\tests\test-export-html-images-e2e.ps1") -FbeExe (Join-Path $outputDir "FBE.exe")
    & (Join-Path $repoRoot "tools\tests\test-portable-registry-isolation.ps1") -FbeExecutable (Join-Path $outputDir "FBE.exe")
    & (Join-Path $repoRoot "tools\tests\test-librusec-genres-portable.ps1") -FbeExecutable (Join-Path $outputDir "FBE.exe")
}

# ArchHandler is part of the single release and is tested from its staged output.
$archHandlerTestArguments = @{ PlatformToolset = $PlatformToolset }
$archHandlerTestArguments.HandlerDirectory = $archHandlerOutputDir
& (Join-Path $repoRoot "tools\tests\test-archhandler-pe-contract.ps1") @archHandlerTestArguments
& (Join-Path $repoRoot "tools\tests\test-archhandler-argv.ps1") @archHandlerTestArguments

& (Join-Path $repoRoot "tools\tests\test-scintilla.ps1") `
    -EditorRuntimeDirectory (Join-Path $repoRoot "out\editor-runtime")

# Every binary in the single release must pass the Windows 7 import gate.
$sharedReleaseFiles = @(
    "FBE.exe", "FBV.exe", "Plugins\ExportHTML.dll", "Plugins\ExportDOCX.dll", "Plugins\ExportEPUB.dll",
    "Plugins\ImportEPUB.dll", "Plugins\ImportEPUBLunaSVG.dll", "FBShell.dll"
)
& (Join-Path $repoRoot "tools\tests\check-win7-imports.ps1") `
    -Configuration $Configuration `
    -OutputDirectory $outputDir `
    -IncludeNames $sharedReleaseFiles

& (Join-Path $repoRoot "tools\tests\check-win7-imports.ps1") `
    -Configuration $Configuration `
    -OutputDirectory $batchOutputDir `
    -IncludeNames @("ExportDOCXBatch.exe", "ExportEPUBBatch.exe", "ImportEPUBBatch.exe")

$editorRuntimeDir = Join-Path $repoRoot "out\editor-runtime"
& (Join-Path $repoRoot "tools\tests\check-win7-imports.ps1") `
    -Configuration $Configuration `
    -OutputDirectory $editorRuntimeDir `
    -IncludeNames @("Scintilla.dll", "Lexilla.dll")

& (Join-Path $repoRoot "tools\tests\check-win7-imports.ps1") `
    -Configuration $Configuration `
    -OutputDirectory $archHandlerOutputDir `
    -IncludeNames @("ZipHandler.exe", "RarHandler.exe")

function Test-BinarySecurityFlags {
    param(
        [Parameter(Mandatory)]
        [string]$Path,

        [switch]$RequireControlFlowGuard
    )

    $bytes = [IO.File]::ReadAllBytes($Path)
    if ($bytes.Length -lt 256 -or
        $bytes[0] -ne [byte][char]'M' -or
        $bytes[1] -ne [byte][char]'Z') {
        throw "Некорректный PE-файл: $Path"
    }

    $peOffset = [BitConverter]::ToInt32($bytes, 0x3c)
    if ($peOffset -lt 0 -or $peOffset + 96 -gt $bytes.Length -or
        [Text.Encoding]::ASCII.GetString($bytes, $peOffset, 4) -ne "PE`0`0") {
        throw "Некорректный PE-заголовок: $Path"
    }

    $optionalHeader = $peOffset + 24
    $magic = [BitConverter]::ToUInt16($bytes, $optionalHeader)
    if ($magic -ne 0x10b -and $magic -ne 0x20b) {
        throw "Неподдерживаемый optional header PE в $Path"
    }

    $dllCharacteristics = [BitConverter]::ToUInt16($bytes, $optionalHeader + 70)
    if (($dllCharacteristics -band 0x40) -eq 0) {
        throw "В $Path отсутствует флаг DYNAMIC_BASE (ASLR)."
    }
    if (($dllCharacteristics -band 0x100) -eq 0) {
        throw "В $Path отсутствует флаг NX_COMPAT (DEP)."
    }
    if ($RequireControlFlowGuard -and ($dllCharacteristics -band 0x4000) -eq 0) {
        throw "В $Path отсутствует флаг GUARD_CF (Control Flow Guard)."
    }
}

function Test-PeMachine {
    param([Parameter(Mandatory)][string]$Path, [Parameter(Mandatory)][UInt16]$ExpectedMachine)

    $bytes = [IO.File]::ReadAllBytes($Path)
    $peOffset = [BitConverter]::ToInt32($bytes, 0x3c)
    $machine = [BitConverter]::ToUInt16($bytes, $peOffset + 4)
    if ($machine -ne $ExpectedMachine) {
        throw "$Path имеет PE machine 0x$($machine.ToString('x4')); ожидалось 0x$($ExpectedMachine.ToString('x4'))."
    }
}

$controlFlowGuardFiles = @(
    "FBE.exe",
    "FBV.exe",
    "ExportHTML.dll",
    "ExportDOCX.dll",
    "ExportEPUB.dll",
    "FBShell.dll"
)

foreach ($name in $requiredFiles) {
    $path = Get-ReleaseOutputPath $name

    if (-not (Test-Path -LiteralPath $path)) {
        throw "Отсутствует обязательный результат сборки: $path"
    }

    Test-BinarySecurityFlags -Path $path `
        -RequireControlFlowGuard:($name -in $controlFlowGuardFiles)
}

foreach ($propertyHandler in @(
        @{ Platform = "Win32"; Machine = [UInt16]0x014c },
        @{ Platform = "x64"; Machine = [UInt16]0x8664 }
    )) {
        $directory = Join-Path $repoRoot "out\package\shell-build\$($propertyHandler.Platform)\$Configuration"
        $path = Join-Path $directory "FBShell.dll"
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "Не найдена фактически поставляемая DLL property handler: $path"
        }
        Test-PeMachine -Path $path -ExpectedMachine $propertyHandler.Machine
        Test-BinarySecurityFlags -Path $path -RequireControlFlowGuard
        $info = [Diagnostics.FileVersionInfo]::GetVersionInfo($path)
        if ($info.FileVersion -ne $expectedVersion -or $info.ProductVersion -ne $expectedVersion) {
            throw "$path имеет версии File='$($info.FileVersion)', Product='$($info.ProductVersion)'; ожидалось '$expectedVersion'."
        }
        & (Join-Path $repoRoot "tools\tests\check-win7-imports.ps1") `
            -Configuration $Configuration `
            -OutputDirectory $directory `
            -IncludeNames @("FBShell.dll")
    }

foreach ($name in $forbiddenFiles) {
    $path = Join-Path $outputDir $name
    if (Test-Path -LiteralPath $path) {
        throw "Устаревший runtime-файл не должен попадать в релиз: $path"
    }
}

foreach ($name in $requiredSymbols) {
    $path = Get-ReleaseOutputPath $name
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Отсутствуют обязательные debug symbols: $path"
    }
    if ((Get-Item -LiteralPath $path).Length -eq 0) {
        throw "Файл debug symbols пуст: $path"
    }
}

foreach ($name in @("FBE.exe", "FBV.exe", "ExportHTML.dll", "ExportDOCX.dll", "ExportEPUB.dll", "ImportEPUB.dll", "ImportEPUBLunaSVG.dll", "ExportDOCXBatch.exe", "ExportEPUBBatch.exe", "ImportEPUBBatch.exe", "FBShell.dll")) {
    $path = Get-ReleaseOutputPath $name
    $info = [Diagnostics.FileVersionInfo]::GetVersionInfo($path)

    if ($info.FileVersion -ne $expectedVersion) {
        throw "$name имеет версию файла '$($info.FileVersion)', ожидалась '$expectedVersion'."
    }

    if ($info.ProductVersion -ne $expectedVersion) {
        throw "$name имеет версию продукта '$($info.ProductVersion)', ожидалась '$expectedVersion'."
    }
}

$requiredFileDescriptions = @{
    "FBE.exe" = "FictionBook Editor Next"
    "FBV.exe" = "FictionBook Validator"
    "ExportHTML.dll" = "FictionBook Editor HTML export plugin"
    "ExportDOCX.dll" = "FictionBook Editor DOCX export plugin"
    "ExportEPUB.dll" = "FictionBook Editor EPUB export plugin"
    "ImportEPUB.dll" = "FictionBook Editor EPUB import plugin"
    "ImportEPUBLunaSVG.dll" = "FictionBook Editor EPUB SVG cover converter"
    "ExportDOCXBatch.exe" = "FictionBook Editor DOCX batch export utility"
    "ExportEPUBBatch.exe" = "FictionBook Editor EPUB batch export utility"
    "ImportEPUBBatch.exe" = "FictionBook Editor EPUB batch import utility"
    "FBShell.dll" = "FictionBook Editor shell property handler"
}

foreach ($entry in $requiredFileDescriptions.GetEnumerator()) {
    $path = Get-ReleaseOutputPath $entry.Key
    $info = [Diagnostics.FileVersionInfo]::GetVersionInfo($path)

    if ([string]::IsNullOrWhiteSpace($info.FileDescription)) {
        throw "У $($entry.Key) отсутствует метаданные FileDescription."
    }
    if ($info.FileDescription -ne $entry.Value) {
        throw "$($entry.Key) имеет FileDescription '$($info.FileDescription)', ожидалось '$($entry.Value)'."
    }
    if ([string]::IsNullOrWhiteSpace($info.ProductName)) {
        throw "У $($entry.Key) отсутствует метаданные ProductName."
    }
}

$editorVersions = @{
    "Scintilla.dll" = "5.6.6"
    "Lexilla.dll" = "5.5.3"
}
foreach ($name in $editorVersions.Keys) {
    $path = Join-Path $outputDir $name
    $info = [Diagnostics.FileVersionInfo]::GetVersionInfo($path)
    if ($info.FileVersion -ne $editorVersions[$name]) {
        throw "$name имеет версию файла '$($info.FileVersion)', ожидалась '$($editorVersions[$name])'."
    }
}

$mtCandidates = @(
    (Get-Command mt.exe -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source -First 1),
    (Get-ChildItem "${env:ProgramFiles(x86)}\Windows Kits\10\bin" -Recurse -Filter mt.exe -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -match '\\x86\\mt\.exe$' } |
        Sort-Object FullName -Descending |
        Select-Object -ExpandProperty FullName -First 1)
) | Where-Object { $_ } | Select-Object -First 1

if (-not $mtCandidates) {
    throw "Не найден mt.exe; невозможно проверить встроенный manifest FBE.exe."
}

$manifestPath = Join-Path ([IO.Path]::GetTempPath()) "FBE-$PID.manifest"
try {
    & $mtCandidates -nologo "-inputresource:$outputDir\FBE.exe;#1" "-out:$manifestPath"
    if ($LASTEXITCODE -ne 0) {
        throw "mt.exe не смог извлечь manifest из FBE.exe."
    }

    [xml]$manifest = Get-Content -Raw -LiteralPath $manifestPath
    $dpiAware = $manifest.SelectSingleNode(
        "//*[local-name()='dpiAware' and namespace-uri()='http://schemas.microsoft.com/SMI/2005/WindowsSettings']"
    )
    $dpiAwareness = $manifest.SelectSingleNode(
        "//*[local-name()='dpiAwareness' and namespace-uri()='http://schemas.microsoft.com/SMI/2016/WindowsSettings']"
    )

    if (-not $dpiAware -or $dpiAware.InnerText.Trim() -ne "true/pm") {
        throw "FBE.exe не помечен legacy-fallback для per-monitor DPI."
    }
    if (-not $dpiAwareness -or $dpiAwareness.InnerText.Trim() -ne "PerMonitorV2,PerMonitor") {
        throw "FBE.exe не объявляет Per-Monitor V2 DPI awareness."
    }
}
finally {
    Remove-Item -LiteralPath $manifestPath -Force -ErrorAction SilentlyContinue
}

Write-Host "Проверка релиза для версии $expectedVersion прошла успешно."
