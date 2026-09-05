<# Ensures the machine-readable release catalog is complete and listable. #>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$catalogTool = Join-Path $PSScriptRoot 'get-release-test-catalog.ps1'
& $catalogTool -Validate | Out-Null
$catalog = (& $catalogTool -AsJson | ConvertFrom-Json)
if ($catalog.schemaVersion -ne 1 -or $catalog.generatedFrom -ne 'tools/build/verify-release.ps1') { throw 'Release test catalog metadata is invalid.' }
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
Write-Host "Release test catalog passed: $($catalog.tests.Count) scenarios."
