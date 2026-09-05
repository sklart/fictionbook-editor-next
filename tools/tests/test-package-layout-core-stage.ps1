<# Runs Core staging from isolated compiled-artifact and provenance fixtures. #>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$configuration = 'PackageLayoutFixture'
$fixtureRoot = Join-Path $repoRoot 'out\tests\package-layout-core-fixture'
$common = Join-Path $repoRoot "out\$configuration"
$plugins = Join-Path $common 'Plugins'
$editorRuntime = Join-Path $fixtureRoot 'editor-runtime'
$batch = Join-Path $fixtureRoot 'batch'
$arch = Join-Path $fixtureRoot 'arch'
$provenance = Join-Path $fixtureRoot 'provenance'
$stage = Join-Path $fixtureRoot 'stage'
try {
    foreach ($path in @($common, $plugins, $editorRuntime, $batch, $arch)) {
        New-Item -ItemType Directory -Force -Path $path | Out-Null
    }
    foreach ($name in @('FBE.exe', 'FBV.exe', 'html.xsl')) { Set-Content -LiteralPath (Join-Path $common $name) -Value $name -Encoding ascii }
    foreach ($name in @('ExportHTML.dll', 'ExportDOCX.dll', 'ExportEPUB.dll', 'ImportEPUB.dll', 'ImportEPUBLunaSVG.dll')) { Set-Content -LiteralPath (Join-Path $plugins $name) -Value $name -Encoding ascii }
    foreach ($name in @('Scintilla.dll', 'Lexilla.dll')) { Set-Content -LiteralPath (Join-Path $editorRuntime $name) -Value $name -Encoding ascii }
    foreach ($name in @('ExportDOCXBatch.exe', 'ExportEPUBBatch.exe', 'ImportEPUBBatch.exe')) { Set-Content -LiteralPath (Join-Path $batch $name) -Value $name -Encoding ascii }
    foreach ($name in @('ZipHandler.exe', 'RarHandler.exe')) { Set-Content -LiteralPath (Join-Path $arch $name) -Value $name -Encoding ascii }

    & (Join-Path $repoRoot 'tools\build\build-provenance.ps1') -Action Write -Kind CommonCore -Configuration $configuration -CommonDirectory $common -ProvenanceDirectory $provenance
    & (Join-Path $repoRoot 'tools\build\build-provenance.ps1') -Action Write -Kind Runtime -Configuration $configuration -ProfileDirectory $editorRuntime -BatchDirectory $batch -ArchHandlerDirectory $arch -ProvenanceDirectory $provenance
    & (Join-Path $repoRoot 'tools\build\stage-core.ps1') -Configuration $configuration -OutputDirectory $stage -EditorRuntimeDirectory $editorRuntime -BatchOutputDirectory $batch -ArchHandlerOutputDirectory $arch -ProvenanceDirectory $provenance

    foreach ($relativePath in @('FBE.exe', 'FBV.exe', 'Plugins\ImportEPUB.dll', 'Utilities\ArchHandler\ZipHandler.exe', 'LICENSE', 'genres.librusec.txt', 'THIRD-PARTY-LICENSES\PCRE2.txt')) {
        if (-not (Test-Path -LiteralPath (Join-Path $stage $relativePath) -PathType Leaf)) {
            throw "Layout-driven Core stage omitted: $relativePath"
        }
    }
    Write-Host 'Package layout Core staging passed.'
}
finally {
    foreach ($path in @($fixtureRoot, $common)) {
        if (Test-Path -LiteralPath $path) { Remove-Item -LiteralPath $path -Recurse -Force }
    }
}
