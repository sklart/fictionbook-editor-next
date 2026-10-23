<# Ensures the machine-readable release catalog is complete and listable. #>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$catalogTool = Join-Path $PSScriptRoot 'get-release-test-catalog.ps1'
& $catalogTool -Validate | Out-Null
$catalog = (& $catalogTool -AsJson | ConvertFrom-Json)
if ($catalog.schemaVersion -ne 2 -or $catalog.generatedFrom -ne 'tools/build/verify-release.ps1') { throw 'Release test catalog metadata is invalid.' }
foreach ($entry in @($catalog.tests)) {
    foreach ($property in @('id', 'path', 'component', 'contours', 'invocations', 'required', 'isolation')) {
        if ($null -eq $entry.PSObject.Properties[$property]) { throw "Catalog entry omits ${property}: $($entry.id)" }
    }
}
foreach ($selection in @(
        @{ Catalog = (& $catalogTool -AsJson -Contour TABLE | ConvertFrom-Json); ExpectedId = 'release.fbe-table-production-roundtrip' },
        @{ Catalog = (& $catalogTool -AsJson -Contour FULL | ConvertFrom-Json); ExpectedId = 'release.fbd-production-roundtrip' }
)) {
    if ($selection.Catalog.tests.id -notcontains $selection.ExpectedId) { throw "Catalog selection omitted $($selection.ExpectedId)." }
}
$fullProfile = (& $catalogTool -AsJson -Contour FULL | ConvertFrom-Json)
$fastProfile = (& $catalogTool -AsJson -Contour FAST | ConvertFrom-Json)
foreach ($id in @('release.fbe-file-close-runtime', 'release.fbe-new-document-safety-runtime', 'release.fbe-scripting-stage3c', 'release.fbe-scripting-stage3d', 'release.fbe-scripting-examples', 'release.fbe-scripting-clipboard-ole-ci')) {
    if ($fullProfile.tests.id -notcontains $id -or $fastProfile.tests.id -contains $id) {
        throw "Required CI runtime matrix must appear in FULL: $id"
    }
}
$requiredSuites = @(& (Join-Path $PSScriptRoot 'get-required-ci-runtime-suites.ps1'))
if ($requiredSuites.Count -ne 4 -or @($requiredSuites | Where-Object RequiresGitHubHosted).Count -ne 1) {
    throw 'Required runtime matrix/isolated OLE constraints are incomplete.'
}
foreach ($id in @('release.plugin-settings-runtime', 'release.plugin-options-tabs-runtime')) {
    $entry = @($fullProfile.tests | Where-Object id -eq $id)
    if ($entry.Count -ne 1 -or -not $entry[0].required -or $entry[0].contours -notcontains 'FULL' -or $fastProfile.tests.id -contains $id) {
        throw "Plug-in window/settings runtime must be required in FULL: $id"
    }
}
$pluginTabs = $fullProfile.tests | Where-Object id -eq 'release.plugin-options-tabs-runtime'
if ($pluginTabs.invocations.Count -ne 2 -or
    @($pluginTabs.invocations | Where-Object command -match '-Mode portable').Count -ne 1 -or
    @($pluginTabs.invocations | Where-Object command -match '-Mode installed').Count -ne 1) {
    throw 'FULL must execute plug-in tabs in both Portable and Installed modes exactly once.'
}
$pluginSettings = $fullProfile.tests | Where-Object id -eq 'release.plugin-settings-runtime'
$registryIsolation = $fullProfile.tests | Where-Object id -eq 'release.portable-registry-isolation'
if ($pluginSettings.invocations.Count -ne 1 -or $registryIsolation.invocations.command -notmatch '-SkipPluginSettingsRuntime') {
    throw 'FULL must run the complete plug-in settings regression exactly once.'
}
if ($fullProfile.tests.id -notcontains 'release.source-safety' -or
    $fullProfile.tests.id -notcontains 'release.fbe-table-production-roundtrip' -or
    $fullProfile.tests.id -notcontains 'release.fbd-production-roundtrip') {
    throw 'FULL profile does not contain its FAST, TABLE and FULL scenarios.'
}
foreach ($id in @(
        'release.fbe-status-bar-long-paint-runtime',
        'release.fbe-themed-message-runtime',
        'release.fbe-theme-application-runtime',
        'release.fbe-body-theme-runtime',
		'release.fbe-xml-source-theme-v1-runtime',
        'release.settings-editor-page-runtime',
        'release.fbe-save-decision-runtime',
        'release.fbe-table-production-roundtrip.huge',
        'release.fbe-table-failure-safety.fault-change-colspan-after-normalize',
        'release.fbe-table-structural-production.command-route.insert-row-above',
        'release.fbe-table-structural-production.command-route.delete-column'
    )) {
    if ($catalog.tests.id -notcontains $id) { throw "Catalog omits required scenario: $id" }
}
foreach ($id in @(
        'release.fbe-table-structural-production.edge-spans.target-0-3.operation-delete-column',
        'release.fbe-table-structural-production.combined.target-1-0.operation-delete-row',
        'release.fbe-table-structural-production.colspan.target-0-0.operation-delete-column.then-insert-column-right',
        'release.fbe-table-structural-production.bulk-10x10.target-0-0-9-9.operation-make-header',
        'release.fbe-table-structural-production.toggle-preserve.target-0-0.operation-toggle-header'
    )) {
    if ($catalog.tests.id -notcontains $id) { throw "Catalog omits parameterized table scenario: $id" }
}
if (@($catalog.tests | Where-Object { $_.id -like 'release.fbe-table-structural-production.command-route.*' }).Count -ne 6) {
    throw 'Catalog does not expand all six table command-route operations.'
}
$huge = $catalog.tests | Where-Object id -eq 'release.fbe-table-production-roundtrip.huge'
if ($huge.invocations.command -notmatch '-Huge') { throw 'Catalog loses the -Huge argument of the table stress scenario.' }
$importAbi = $catalog.tests | Where-Object id -eq 'release.import-epub-batch-dll-abi'
if ($importAbi.invocations.command -notmatch '-DllPath' -or $importAbi.invocations.command -notmatch '-BatchPath' -or $importAbi.invocations.command -notmatch '-SmokeEpubPath') {
    throw 'Catalog loses continued arguments of the ImportEPUB ABI scenario.'
}
$pcre2Tests = @($catalog.tests | Where-Object { $_.id -in @('release.pcre2-wrapper', 'release.pcre2-replace', 'release.pcre2-cache') })
if ($pcre2Tests.Count -ne 3 -or @($pcre2Tests | Where-Object { $_.invocations.command -notmatch '@pcre2TestArguments' }).Count -ne 0) {
    throw 'Catalog loses a declared pcre2 splatting invocation.'
}
if (@($pcre2Tests | Where-Object { $null -ne $_.fixtures -or $_.timeoutSeconds -ne 'declared-by-test' }).Count -ne 0) {
    throw 'Catalog must represent legacy unknown metadata explicitly, not as empty metadata.'
}
Write-Host "Release test catalog passed: $($catalog.tests.Count) scenarios."
