<# Verifies that the authoritative development output mirrors the packaged plugin layout. #>
[CmdletBinding()]
param(
    [string]$Configuration = 'Release',
    [string]$OutputDirectory,
    [string]$BatchOutputDirectory
)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
if (-not $OutputDirectory) { $OutputDirectory = Join-Path $repoRoot "out\$Configuration" }
$OutputDirectory = (Resolve-Path -LiteralPath $OutputDirectory).Path
$BatchOutputDirectory = if ($BatchOutputDirectory) {
    $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($BatchOutputDirectory)
} else { $OutputDirectory }
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

# Plugin projects themselves write Release|Win32 artifacts into Plugins.  The
# final build-script normalization remains a compatibility guard, but must not
# be the only thing preventing an interrupted build from leaving a flat tree.
$pluginProjects = @(
    'src\export-html\ExportHTML.vcxproj',
    'src\export-docx\ExportDOCX.vcxproj',
    'src\export-epub\ExportEPUB.vcxproj',
    'src\import-epub\ImportEPUB.vcxproj',
    'src\import-epub\ImportEPUBLunaSVG.vcxproj'
)
foreach ($project in $pluginProjects) {
    $content = Get-Content -LiteralPath (Join-Path $repoRoot $project) -Raw
    if ($content -notmatch '(?s)Condition="''\$\(Configuration\)\|\$\(Platform\)''==''Release\|Win32''".*?<OutDir>[^<]*out\\\$\(Configuration\)\\Plugins\\</OutDir>') {
        throw "Release plugin project does not output directly to Plugins: $project"
    }
}

# Batch utilities are executable tools, not plugins.  They and their symbols
# deliberately stay in their configured development output directory while
# their DLL probes use the sibling Plugins directory first.
foreach ($name in @('ExportDOCXBatch.exe','ExportEPUBBatch.exe','ImportEPUBBatch.exe','ExportDOCXBatch.pdb','ExportEPUBBatch.pdb','ImportEPUBBatch.pdb')) {
    if (-not (Test-Path -LiteralPath (Join-Path $BatchOutputDirectory $name) -PathType Leaf)) {
        throw "Batch converter artifact is missing from development output directory: $name"
    }
}

Write-Host "Development bundled plugin layout passed: $pluginsDirectory"
