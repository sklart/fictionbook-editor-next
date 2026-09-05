<# Validates the authoritative source-to-destination packaging map. #>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$layoutPath = Join-Path $repoRoot 'packaging\layout.json'
$manifestPath = Join-Path $repoRoot 'packaging\package-manifest.json'
foreach ($path in @($layoutPath, $manifestPath)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Packaging input is missing: $path" }
}
$layout = Get-Content -Raw -LiteralPath $layoutPath | ConvertFrom-Json
$manifest = Get-Content -Raw -LiteralPath $manifestPath | ConvertFrom-Json
if ($layout.schemaVersion -ne 1) { throw "Unsupported package layout schema: $($layout.schemaVersion)" }

foreach ($kind in @('core', 'integration')) {
    $section = $layout.$kind
    if ($null -eq $section -or @($section.copy).Count -eq 0) { throw "Package layout has no copy entries for $kind." }
    $destinations = @{}
    foreach ($entry in @($section.copy)) {
        foreach ($property in @('sourceRoot', 'kind')) {
            if ([string]::IsNullOrWhiteSpace([string]$entry.$property)) { throw "$kind layout entry omits $property." }
        }
        if (-not $entry.contents -and ([string]::IsNullOrWhiteSpace([string]$entry.source) -or [string]::IsNullOrWhiteSpace([string]$entry.destination))) {
            throw "$kind file layout entry must specify both source and destination."
        }
        if ($entry.kind -notin @('compiledArtifact', 'maintainedResource', 'maintainedDefault', 'thirdPartyLicense', 'installerTool')) {
            throw "$kind layout entry has an unknown source type: $($entry.kind)"
        }
        if (-not $entry.contents) {
            $key = $entry.destination.Replace('/', '\').ToLowerInvariant()
            if ($destinations.ContainsKey($key)) { throw "$kind layout has two producers for $($entry.destination)." }
            $destinations[$key] = $true
        }
    }
}

$coreDestinations = @($layout.core.copy | Where-Object { -not $_.contents } | ForEach-Object { $_.destination.Replace('/', '\') })
$coreDestinations += @($layout.core.aliases | ForEach-Object { $_.destination.Replace('/', '\') })
foreach ($required in @($manifest.core.required)) {
    if ($required -notin $coreDestinations -and $required -notmatch '^(Plugins|dict|Lang|Themes|Scripts|Utilities|EditorBackgrounds|THIRD-PARTY-LICENSES)\\' -and $required -notmatch '^genres\.') {
        throw "Core manifest item is not represented by a package-layout entry: $required"
    }
}
foreach ($required in @($manifest.integration.required)) {
    if ($required -notin @($layout.integration.copy | ForEach-Object { $_.destination.Replace('/', '\') }) -and $required -notmatch '^Lang\\Shell\\') {
        throw "Integration manifest item is not represented by a package-layout entry: $required"
    }
}

foreach ($script in @('tools\build\stage-core.ps1', 'tools\build\stage-integration.ps1')) {
    $scriptText = Get-Content -Raw -LiteralPath (Join-Path $repoRoot $script)
    if ($scriptText -notmatch 'Get-FbePackageLayout' -or $scriptText -notmatch 'Copy-FbePackageLayoutEntries') {
        throw "Staging script does not consume the package layout: $script"
    }
}

Write-Host 'Package layout contract passed.'
