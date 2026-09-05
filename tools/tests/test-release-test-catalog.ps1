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
        @{ Catalog = (& $catalogTool -AsJson -Contour FULL | ConvertFrom-Json); ExpectedId = 'release.fbd-production-roundtrip' },
        @{ Catalog = (& $catalogTool -AsJson -Id release.archhandler-pe-contract | ConvertFrom-Json); ExpectedId = 'release.archhandler-pe-contract' }
)) {
    if ($selection.Catalog.tests.id -notcontains $selection.ExpectedId) { throw "Catalog selection omitted $($selection.ExpectedId)." }
}
$fullProfile = (& $catalogTool -AsJson -Contour FULL | ConvertFrom-Json)
if ($fullProfile.tests.id -notcontains 'release.source-safety' -or
    $fullProfile.tests.id -notcontains 'release.fbe-table-production-roundtrip' -or
    $fullProfile.tests.id -notcontains 'release.fbd-production-roundtrip') {
    throw 'FULL profile does not contain its FAST, TABLE and FULL scenarios.'
}
foreach ($id in @(
        'release.fbe-table-production-roundtrip.huge',
        'release.fbe-table-failure-safety.fault-change-colspan-after-normalize',
        'release.fbe-table-structural-production.command-route.insert-row-above',
        'release.fbe-table-structural-production.command-route.delete-column'
    )) {
    if ($catalog.tests.id -notcontains $id) { throw "Catalog omits required scenario: $id" }
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
$pcre2 = $catalog.tests | Where-Object id -eq 'release.pcre2'
if ($pcre2.invocations.command -notmatch '@pcre2TestArguments') {
    throw 'Catalog loses the declared pcre2 splatting invocation.'
}
if ($null -ne $pcre2.fixtures -or $pcre2.timeoutSeconds -ne 'declared-by-test') {
    throw 'Catalog must represent legacy unknown metadata explicitly, not as empty metadata.'
}
Write-Host "Release test catalog passed: $($catalog.tests.Count) scenarios."
