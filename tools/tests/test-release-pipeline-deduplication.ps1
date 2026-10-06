<# Contract for the single Windows 7+ release pipeline. #>
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
function Text([string]$path) { Get-Content -Raw -LiteralPath (Join-Path $root $path) }
function Require([string]$text, [string]$needle, [string]$where) {
    if (-not $text.Contains($needle)) { throw "$where must contain '$needle'." }
}
function Forbid([string]$text, [string]$needle, [string]$where) {
    if ($text.Contains($needle)) { throw "$where contains obsolete '$needle'." }
}
function Get-BlockEnd([string]$text, [int]$start, [string]$where) {
    $open = $text.IndexOf('{', $start)
    if ($open -lt 0) { throw "$where has no opening block brace." }
    $depth = 0
    for ($index = $open; $index -lt $text.Length; ++$index) {
        if ($text[$index] -eq '{') { ++$depth }
        elseif ($text[$index] -eq '}') {
            --$depth
            if ($depth -eq 0) { return $index }
        }
    }
    throw "$where has no closing block brace."
}
function Get-OccurrenceCount([string]$text, [string]$needle) {
    $count = 0; $position = 0
    while (($position = $text.IndexOf($needle, $position, [StringComparison]::Ordinal)) -ge 0) { ++$count; $position += $needle.Length }
    return $count
}

$workflow = Text '.github\workflows\build.yml'
$build = Text 'tools\build\build.ps1'
$verify = Text 'tools\build\verify-release.ps1'
$release = Text 'tools\build\create-release.ps1'
$artifacts = Text 'tools\build\verify-artifacts.ps1'
$stage = Text 'tools\build\stage-core.ps1'
$fingerprint = Text 'tools\build\editor-runtime-helpers.ps1'
$migration = Text 'tools\build\UpdateVersion.ps1'
$propertyHandler = Text 'tools\build\build-shell-integration.ps1'
$fbvMui = Text 'tools\build\build-fbv-verb-mui.ps1'
$pluginUpgradeSmoke = Text 'tools\tests\test-bundled-plugin-upgrade-uninstall.ps1'

foreach ($needle in @('validate:', 'build:', 'package:', 'publish:', 'Restore universal editor runtime cache', 'Build Release Win32', 'Write Runtime build provenance', 'Verify universal release binaries', 'Create release artifacts without compiling', 'Verify release archives')) { Require $workflow $needle 'workflow' }
Require $workflow 'Smoke bundled plugin upgrade and uninstall' 'workflow installer smoke'
Require $workflow 'test-bundled-plugin-upgrade-uninstall.ps1' 'workflow installer smoke invocation'
foreach ($needle in @('Synthetic plugin upgrade installer', 'Synthetic plugin upgrade uninstaller', 'plugins.json', 'PluginSourceDirectory')) { Require $pluginUpgradeSmoke $needle 'bundled plugin upgrade smoke' }
foreach ($needle in @('CompatibilityTarget', 'EditorRuntimeOnly', 'BatchConvertersOnly', 'Restore Modern editor runtime cache', 'Restore Win7 editor runtime cache', 'Build Modern', 'Build Win7', 'target-batches/Modern', 'target-batches/Win7', 'artifacts/Modern', 'artifacts/Win7')) { Forbid $workflow $needle 'workflow' }
foreach ($needle in @('CompatibilityTarget', 'EditorRuntimeOnly', 'BatchConvertersOnly')) { Forbid $build $needle 'build.ps1' }
foreach ($needle in @('CompatibilityTarget', 'CommonCoreDirectory', 'deployment.ini')) { Forbid $stage $needle 'stage-core.ps1' }
foreach ($needle in @('CompatibilityTarget', 'Modern', 'Win7')) { Forbid $fingerprint $needle 'editor runtime fingerprint' }
foreach ($needle in @('CompatibilityTarget')) { Forbid $release $needle 'create-release.ps1' }
foreach ($needle in @('CompatibilityTarget', 'artifacts\\Modern', 'artifacts\\Win7')) { Forbid $artifacts $needle 'verify-artifacts.ps1' }
$localizationPreflight = Text 'tools\tests\test-localization-preflight.ps1'
Require $workflow 'test-localization-preflight.ps1' 'workflow localization preflight'
Require $verify 'SkipLocalizationPreflight' 'verify-release localization preflight switch'
Require $verify 'SkipEarlyRuntimeSuites' 'verify-release early runtime suite switch'
Require $workflow 'SkipEarlyRuntimeSuites = $true' 'workflow duplicate suite suppression'
foreach ($test in @('test-plugin-localization-catalog.ps1','test-fbv-localization-resources.ps1','test-fbe-property-schema-localization.ps1','test-export-html-localization-resources.ps1','test-export-docx-localization-resources.ps1','test-export-epub-localization-resources.ps1','test-import-epub-localization-resources.ps1','test-localization-export.ps1','test-localization-runtime-contract.ps1','test-runtime-lang-export.ps1','test-fbe-runtime-lang-overlay.ps1','test-fbv-runtime-lang-overlay.ps1','test-export-html-runtime-lang-overlay.ps1','test-export-docx-runtime-lang-overlay.ps1','test-export-epub-runtime-lang-overlay.ps1','test-import-epub-runtime-lang-overlay.ps1','test-search-preset-localization.ps1','test-regex-help-runtime-localization.ps1','test-fbe-binary-editor-localization.ps1','test-product-hardcoded-cyrillic-audit.ps1')) {
    Require $localizationPreflight $test 'localization preflight static suite'
    if ($verify.Contains($test)) { throw "$test must not be rerun by verify-release after preflight." }
}
Require $verify 'test-runtime-lang-output-layout.ps1' 'post-build localization output-layout test'
if ((Get-OccurrenceCount $workflow 'test-search-core-suite.ps1') -ne 1 -or (Get-OccurrenceCount $verify 'test-search-core-suite.ps1') -ne 1) {
    throw 'Search Core must be one explicit build suite and one default local verifier suite.'
}
if ((Get-OccurrenceCount $workflow 'test-regex-help-placement-runtime.ps1') -ne 1 -or (Get-OccurrenceCount $verify 'test-regex-help-placement-runtime.ps1') -ne 1) {
    throw 'Regex Help placement runtime must run once in both CI and the default release contour.'
}
foreach ($test in @('test-search-session.ps1','test-literal-search-mshtml-differential.ps1','test-search-document-adapter-mshtml.ps1','test-search-preset-store.ps1','test-pcre2-match-loop.ps1')) {
    if ($verify.Contains($test)) { throw "$test must be delegated to test-search-core-suite.ps1." }
}Require $verify 'check-win7-imports.ps1' 'verify-release.ps1'
Require $verify 'out\editor-runtime' 'verify-release.ps1'
foreach ($test in @('test-release-test-catalog.ps1', 'test-fb2-common-boundary.ps1', 'test-first-party-msbuild-policy.ps1', 'test-fbe-contract-generation.ps1', 'test-fbe-plugin-host-boundary.ps1', 'test-fbe-source-helpers-boundary.ps1', 'test-fbe-search-boundary.ps1', 'test-fbe-settings-background-boundary.ps1', 'test-package-layout.ps1', 'test-release-artifact-cleanup.ps1', 'test-package-layout-copy.ps1', 'test-package-layout-integration-stage.ps1', 'test-package-layout-core-stage.ps1', 'test-editor-runtime-output-isolation.ps1', 'test-fbe-table-visual-mode.ps1', 'test-fbe-table-structure-boundary.ps1', 'test-table-toolbar-contract.ps1', 'test-fbe-context-attribute-bars.ps1', 'test-fbe-context-attribute-bars-runtime.ps1', 'test-fbe-script-document-path-api.ps1', 'test-fbe-script-document-path-runtime.ps1', 'test-fbe-backup-settings.ps1', 'test-fbe-auto-url-detect.ps1', 'test-xml-source-themes.ps1', 'test-xml-source-current-line.ps1', 'test-fbe-filename-state.ps1', 'test-fbe-source-xml-declaration.ps1', 'test-source-idle-toolbar-localization-contract.ps1', 'test-external-file-check-throttle-contract.ps1', 'test-clipboard-listener-contract.ps1', 'test-ui-dirty-state-contract.ps1', 'test-selection-context-cache-contract.ps1', 'test-mainframe-idle-profiler-contract.ps1', 'test-fbe-spellcheck-scroll-contract.ps1', 'test-description-idle-contract.ps1')) { Require $verify $test 'verify-release FAST contour' }
foreach ($test in @('test-fbe-table-toolbar-rendering.ps1', 'test-fbe-toolbar-alpha-scale.ps1', 'test-fbe-table-production-roundtrip.ps1', 'test-fbe-table-structural-performance.ps1', 'test-fbe-table-failure-safety.ps1', 'test-fbe-spellcheck-local-edit-performance.ps1', 'test-fbe-spellcheck-scroll-runtime.ps1', 'test-fbe-idle-performance-runtime.ps1', 'test-fbe-idle-interaction-performance-runtime.ps1')) { Require $verify $test 'verify-release FULL contour' }
Forbid $verify 'QUARANTINED table-toolbar-rendering failure' 'verify-release.ps1'
Require $verify 'test-nsis-legacy-archhandler-cleanup.ps1' 'verify-release legacy ArchHandler cleanup contract'
Require $verify 'test-fbe-save-as-mru-runtime.ps1' 'verify-release Save As MRU runtime regression'
$tableContourStart = $verify.IndexOf('if ($runTables) {')
$fullContourStart = $verify.IndexOf('if ($FullValidation) {')
if ($tableContourStart -lt 0 -or $fullContourStart -lt 0) { throw 'verify-release.ps1 must retain explicit table and FULL contours.' }
$tableContourEnd = Get-BlockEnd $verify $tableContourStart 'table contour'
$fullContourEnd = Get-BlockEnd $verify $fullContourStart 'FULL contour'
foreach ($test in @('test-fbe-table-visual-mode.ps1', 'test-fbe-table-structure-boundary.ps1', 'test-table-toolbar-contract.ps1', 'test-fbe-script-document-path-api.ps1', 'test-fbe-script-document-path-runtime.ps1', 'test-fbe-backup-settings.ps1', 'test-fbe-auto-url-detect.ps1', 'test-xml-source-themes.ps1', 'test-xml-source-current-line.ps1', 'test-fbe-filename-state.ps1', 'test-fbe-source-xml-declaration.ps1')) {
    $position = $verify.IndexOf($test)
    if ($position -lt 0 -or $position -ge $tableContourStart -or $position -ge $fullContourStart) { throw "$test must remain before both table and FULL contours." }
    if ((Get-OccurrenceCount $verify $test) -ne 1) { throw "$test must be invoked exactly once in FAST." }
}
foreach ($test in @('test-fbe-table-toolbar-rendering.ps1', 'test-fbe-toolbar-alpha-scale.ps1', 'test-fbe-table-production-roundtrip.ps1', 'test-fbe-table-structural-performance.ps1', 'test-fbe-table-failure-safety.ps1')) {
    $position = $verify.IndexOf($test)
    if ($position -le $tableContourStart -or $position -ge $tableContourEnd) { throw "$test must remain strictly inside the table contour." }
}
$spellcheckProductionTest = 'test-fbe-spellcheck-local-edit-performance.ps1'
if ((Get-OccurrenceCount $verify $spellcheckProductionTest) -ne 4) { throw 'Spellcheck local-edit regression must retain one FAST formatting smoke and three FULL variants.' }
$formattingSmoke = $verify.IndexOf('-Mutation formatting -ParagraphCount 750 -VerifyCountIndependence')
if ($formattingSmoke -lt 0 -or $formattingSmoke -ge $fullContourStart) { throw 'Spellcheck formatting smoke must run in FAST before FULL.' }
foreach ($mutation in @('direct', 'script', 'large-paragraph')) {
    $position = $verify.IndexOf("-Mutation $mutation -VerifyCountIndependence")
    if ($position -le $fullContourStart -or $position -ge $fullContourEnd) { throw "Spellcheck $mutation regression must run strictly inside FULL." }
}
$idlePerformanceTest = 'test-fbe-idle-performance-runtime.ps1'
$idlePerformancePosition = $verify.IndexOf($idlePerformanceTest)
if ($idlePerformancePosition -le $fullContourStart -or $idlePerformancePosition -ge $fullContourEnd -or (Get-OccurrenceCount $verify $idlePerformanceTest) -ne 1) { throw 'Idle performance runtime regression must be invoked exactly once strictly inside FULL.' }
$idleInteractionPerformanceTest = 'test-fbe-idle-interaction-performance-runtime.ps1'
$idleInteractionPerformancePosition = $verify.IndexOf($idleInteractionPerformanceTest)
if ($idleInteractionPerformancePosition -le $fullContourStart -or $idleInteractionPerformancePosition -ge $fullContourEnd -or (Get-OccurrenceCount $verify $idleInteractionPerformanceTest) -ne 1) { throw 'Idle interaction performance runtime regression must be invoked exactly once strictly inside FULL.' }
if ((Get-OccurrenceCount $verify 'test-fbe-table-toolbar-rendering.ps1') -ne 1 -or (Get-OccurrenceCount $verify 'test-fbe-toolbar-alpha-scale.ps1') -ne 1) { throw 'Table toolbar rendering and alpha-scale tests must be invoked exactly once.' }
Require $workflow 'Test-FbeLegacy308MigrationRequired' 'workflow migration verification'
Require $release 'Test-FbeLegacy308MigrationRequired' 'create-release migration policy'
Require $migration 'Test-FbeLegacy308MigrationRequired' 'central migration policy'
foreach ($script in @(@{ Text = $release; Name = 'create-release.ps1' }, @{ Text = $artifacts; Name = 'verify-artifacts.ps1' })) {
    Require $script.Text 'Get-FbeAssetVersion' $script.Name
}
Require $workflow 'test-legacy308-migration-policy.ps1' 'workflow migration policy test'
Require $artifacts 'Test-FbeLegacy308MigrationRequired' 'artifact verification migration policy'
foreach ($needle in @('FBE.pdb', 'FBV.pdb', 'FBShell.propertyhandler.win32.pdb', 'FBShell.propertyhandler.x64.pdb')) { Require $artifacts $needle 'symbols verification' }
foreach ($script in @(@{ Text = $release; Name = 'create-release.ps1' }, @{ Text = $verify; Name = 'verify-release.ps1' }, @{ Text = $propertyHandler; Name = 'build-shell-integration.ps1' }, @{ Text = $fbvMui; Name = 'build-fbv-verb-mui.ps1' })) {
    Require $script.Text "PlatformToolset = 'v143'" $script.Name
}
foreach ($script in @(@{ Text = $propertyHandler; Name = 'build-shell-integration.ps1' }, @{ Text = $fbvMui; Name = 'build-fbv-verb-mui.ps1' })) {
    Require $script.Text "VcVarsVersion '14.44'" $script.Name
}
Write-Host 'Unified release pipeline contract passed.'
