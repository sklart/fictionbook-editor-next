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

$workflow = Text '.github\workflows\build.yml'
$build = Text 'tools\build\build.ps1'
$verify = Text 'tools\build\verify-release.ps1'
$release = Text 'tools\build\create-release.ps1'
$artifacts = Text 'tools\build\verify-artifacts.ps1'
$stage = Text 'tools\build\stage-core.ps1'
$fingerprint = Text 'tools\build\editor-runtime-helpers.ps1'
$migration = Text 'tools\build\UpdateVersion.ps1'
$archHandler = Text 'tools\build\build-archhandler.ps1'
$propertyHandler = Text 'tools\build\build-shell-integration.ps1'
$fbvMui = Text 'tools\build\build-fbv-verb-mui.ps1'
$pluginUpgradeSmoke = Text 'tools\tests\test-bundled-plugin-upgrade-uninstall.ps1'

foreach ($needle in @('validate:', 'build:', 'package:', 'publish:', 'Restore universal editor runtime cache', 'Build Release Win32', 'Build ArchHandler', 'Write Runtime build provenance', 'Verify universal release binaries', 'Create release artifacts without compiling', 'Verify release archives')) { Require $workflow $needle 'workflow' }
Require $workflow 'Verify ArchHandler PE contract' 'workflow ArchHandler PE validation'
Require $workflow 'Smoke bundled plugin upgrade and uninstall' 'workflow installer smoke'
Require $workflow 'test-bundled-plugin-upgrade-uninstall.ps1' 'workflow installer smoke invocation'
foreach ($needle in @('Synthetic plugin upgrade installer', 'Synthetic plugin upgrade uninstaller', 'plugins.json', 'PluginSourceDirectory')) { Require $pluginUpgradeSmoke $needle 'bundled plugin upgrade smoke' }
foreach ($needle in @('CompatibilityTarget', 'EditorRuntimeOnly', 'BatchConvertersOnly', 'Restore Modern editor runtime cache', 'Restore Win7 editor runtime cache', 'Build Modern', 'Build Win7', 'target-batches/Modern', 'target-batches/Win7', 'archhandler/Modern', 'archhandler/Win7', 'artifacts/Modern', 'artifacts/Win7')) { Forbid $workflow $needle 'workflow' }
foreach ($needle in @('CompatibilityTarget', 'EditorRuntimeOnly', 'BatchConvertersOnly')) { Forbid $build $needle 'build.ps1' }
foreach ($needle in @('CompatibilityTarget', 'CommonCoreDirectory', 'deployment.ini')) { Forbid $stage $needle 'stage-core.ps1' }
foreach ($needle in @('CompatibilityTarget', 'Modern', 'Win7')) { Forbid $fingerprint $needle 'editor runtime fingerprint' }
foreach ($needle in @('CompatibilityTarget')) { Forbid $release $needle 'create-release.ps1' }
foreach ($needle in @('CompatibilityTarget', 'artifacts\\Modern', 'artifacts\\Win7')) { Forbid $artifacts $needle 'verify-artifacts.ps1' }
Require $verify 'check-win7-imports.ps1' 'verify-release.ps1'
Require $verify 'out\editor-runtime' 'verify-release.ps1'
Require $verify 'out\archhandler\Win32' 'verify-release.ps1'
foreach ($test in @('test-fbe-table-visual-mode.ps1', 'test-table-toolbar-contract.ps1', 'test-fbe-script-document-path-api.ps1', 'test-fbe-backup-settings.ps1', 'test-fbe-auto-url-detect.ps1', 'test-xml-source-themes.ps1', 'test-xml-source-current-line.ps1', 'test-fbe-filename-state.ps1', 'test-fbe-source-xml-declaration.ps1')) { Require $verify $test 'verify-release FAST contour' }
foreach ($test in @('test-fbe-table-toolbar-rendering.ps1', 'test-fbe-table-production-roundtrip.ps1', 'test-fbe-table-structural-performance.ps1', 'test-fbe-table-failure-safety.ps1', 'test-fbe-spellcheck-local-edit-performance.ps1')) { Require $verify $test 'verify-release FULL contour' }
Forbid $verify 'QUARANTINED table-toolbar-rendering failure' 'verify-release.ps1'
Require $verify 'test-archhandler-pe-contract.ps1' 'verify-release ArchHandler contract'
$tableContourStart = $verify.IndexOf('if ($runTables)')
$fullContourStart = $verify.IndexOf('if ($FullValidation)')
if ($tableContourStart -lt 0 -or $fullContourStart -lt 0) { throw 'verify-release.ps1 must retain explicit table and FULL contours.' }
foreach ($test in @('test-fbe-table-visual-mode.ps1', 'test-table-toolbar-contract.ps1', 'test-fbe-script-document-path-api.ps1', 'test-fbe-backup-settings.ps1', 'test-fbe-auto-url-detect.ps1', 'test-xml-source-themes.ps1', 'test-xml-source-current-line.ps1', 'test-fbe-filename-state.ps1', 'test-fbe-source-xml-declaration.ps1')) {
    if ($verify.IndexOf($test) -ge $tableContourStart -or $verify.IndexOf($test) -ge $fullContourStart) { throw "$test must remain in the FAST portion of verify-release.ps1." }
}
foreach ($test in @('test-fbe-table-toolbar-rendering.ps1', 'test-fbe-table-production-roundtrip.ps1', 'test-fbe-table-structural-performance.ps1', 'test-fbe-table-failure-safety.ps1')) {
    if ($verify.IndexOf($test) -lt $tableContourStart) { throw "$test must remain inside the table contour." }
}
if ($verify.IndexOf('test-fbe-spellcheck-local-edit-performance.ps1') -lt $fullContourStart) { throw 'Spellcheck local-edit production regression must remain FULL-only.' }
Require $workflow 'Test-FbeLegacy308MigrationRequired' 'workflow migration verification'
Require $release 'Test-FbeLegacy308MigrationRequired' 'create-release migration policy'
Require $migration 'Test-FbeLegacy308MigrationRequired' 'central migration policy'
foreach ($script in @(@{ Text = $release; Name = 'create-release.ps1' }, @{ Text = $artifacts; Name = 'verify-artifacts.ps1' })) {
    Require $script.Text 'Get-FbeAssetVersion' $script.Name
}
Require $workflow 'test-legacy308-migration-policy.ps1' 'workflow migration policy test'
Require $artifacts 'Test-FbeLegacy308MigrationRequired' 'artifact verification migration policy'
foreach ($needle in @('FBE.pdb', 'FBV.pdb', 'FBShell.propertyhandler.win32.pdb', 'FBShell.propertyhandler.x64.pdb')) { Require $artifacts $needle 'symbols verification' }
foreach ($script in @(@{ Text = $release; Name = 'create-release.ps1' }, @{ Text = $verify; Name = 'verify-release.ps1' }, @{ Text = $archHandler; Name = 'build-archhandler.ps1' }, @{ Text = $propertyHandler; Name = 'build-shell-integration.ps1' }, @{ Text = $fbvMui; Name = 'build-fbv-verb-mui.ps1' })) {
    Require $script.Text "PlatformToolset = 'v143'" $script.Name
}
foreach ($script in @(@{ Text = $archHandler; Name = 'build-archhandler.ps1' }, @{ Text = $propertyHandler; Name = 'build-shell-integration.ps1' }, @{ Text = $fbvMui; Name = 'build-fbv-verb-mui.ps1' })) {
    Require $script.Text "VcVarsVersion '14.44'" $script.Name
}
Write-Host 'Unified release pipeline contract passed.'
