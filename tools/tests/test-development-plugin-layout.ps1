<# Verifies that the authoritative development output mirrors the packaged plugin layout. #>
[CmdletBinding()]
param(
    [string]$Configuration = 'Release',
    [string]$OutputDirectory
)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
if (-not $OutputDirectory) { $OutputDirectory = Join-Path $repoRoot "out\$Configuration" }
$OutputDirectory = (Resolve-Path -LiteralPath $OutputDirectory).Path
$pluginsDirectory = Join-Path $OutputDirectory 'Plugins'
$pluginDllNames = @('ExportHTML.dll','ExportDOCX.dll','ExportEPUB.dll','ImportEPUB.dll','ImportEPUBLunaSVG.dll')
$pluginSymbolNames = @('ExportHTML.pdb','ExportDOCX.pdb','ExportEPUB.pdb','ImportEPUB.pdb','ImportEPUBLunaSVG.pdb')

foreach ($name in $pluginDllNames + $pluginSymbolNames) {
    $packagedPath = Join-Path $pluginsDirectory $name
    if (-not (Test-Path -LiteralPath $packagedPath -PathType Leaf)) {
        throw "Bundled plugin is missing from development Plugins directory: $packagedPath"
    }
    $legacyPath = Join-Path $OutputDirectory $name
    if (Test-Path -LiteralPath $legacyPath -PathType Leaf) {
        throw "Bundled plugin must not remain in development output root: $legacyPath"
    }
}

# Batch utilities are executable tools, not plugins.  They and their symbols
# deliberately stay in the development output root while their DLL probes use
# the sibling Plugins directory first.
foreach ($name in @('ExportDOCXBatch.exe','ExportEPUBBatch.exe','ImportEPUBBatch.exe','ExportDOCXBatch.pdb','ExportEPUBBatch.pdb','ImportEPUBBatch.pdb')) {
    if (-not (Test-Path -LiteralPath (Join-Path $OutputDirectory $name) -PathType Leaf)) {
        throw "Batch converter artifact is missing from development output root: $name"
    }
}

Write-Host "Development bundled plugin layout passed: $pluginsDirectory"
