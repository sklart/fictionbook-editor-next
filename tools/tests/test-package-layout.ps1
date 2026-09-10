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

foreach ($legacyUserFile in @('Settings.xml', 'Hotkeys.xml', 'Words.xml')) {
    if (@($layout.core.copy | Where-Object { $_.destination -eq $legacyUserFile }).Count -ne 0) {
        throw "Core layout must not copy mutable user state to its root: $legacyUserFile"
    }
}
if (-not (Test-Path -LiteralPath (Join-Path $repoRoot 'runtime\Resources\Words.xml') -PathType Leaf)) {
    throw 'The built-in Words.xml seed must live under runtime\Resources.'
}
if ($manifest.core.runtimeDirectories -notcontains 'Resources') {
    throw 'Core manifest must preserve immutable runtime Resources.'
}
$wordsResource = @($layout.core.copy | Where-Object { $_.sourceRoot -eq 'runtime' -and $_.source -eq 'Resources/Words.xml' -and $_.destination -eq 'Resources/Words.xml' })
if ($wordsResource.Count -ne 1 -or -not $wordsResource[0].required) {
    throw 'Package layout must explicitly stage runtime Resources\Words.xml.'
}
$installerScript = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'packaging\nsis\Installer\MakeInstaller.nsi')
if ($installerScript -notmatch 'File "\$\{INPUTDIR\}\\Resources\\Words\.xml"') {
    throw 'NSIS installer must install the immutable Words.xml seed under Resources.'
}

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
    if ($required -notin $coreDestinations -and $required -notmatch '^(Plugins|dict|Lang|Themes|Scripts|Utilities|EditorBackgrounds|Resources|THIRD-PARTY-LICENSES)\\' -and $required -notmatch '^genres\.') {
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

$buildScript = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'tools\build\build.ps1')
if ($buildScript -notmatch 'Export-BuiltInResources' -or $buildScript -notmatch 'runtime\\Resources\\Words\.xml') {
    throw 'Development build must materialize Resources\\Words.xml beside FBE.exe.'
}
if ($buildScript -notmatch '"defaults\\Words\.xml"') {
    throw 'Development build must remove the obsolete defaults\\Words.xml seed.'
}
if ($buildScript -notmatch '\$removedLegacyWords' -or $buildScript -notmatch 'Get-ChildItem -LiteralPath \$legacyDefaultsDirectory -Force') {
    throw 'Development build must remove only an empty defaults directory after its legacy Words.xml seed.'
}
foreach ($legacyUserFile in @('Settings.xml', 'Hotkeys.xml', 'Words.xml')) {
    if ($buildScript -notmatch ('"' + [regex]::Escape($legacyUserFile) + '"')) {
        throw "Development build no longer removes obsolete root user state: $legacyUserFile"
    }
}

Write-Host 'Package layout contract passed.'
