<# Verifies that tampering a common payload is rejected before staging/tests. #>
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$fixture = Join-Path ([IO.Path]::GetTempPath()) "fbe-provenance-$PID"
$common = Join-Path $fixture 'common'
$runtime = Join-Path $fixture 'runtime'
$batch = Join-Path $fixture 'batch'
$arch = Join-Path $fixture 'arch'
$pluginNames = @('ExportHTML.dll','ExportDOCX.dll','ExportEPUB.dll','ImportEPUB.dll','ImportEPUBLunaSVG.dll')
try {
    New-Item -ItemType Directory -Force -Path $common | Out-Null
    $commonPlugins = Join-Path $common 'Plugins'
    New-Item -ItemType Directory -Force -Path $commonPlugins | Out-Null
    foreach ($name in @('FBE.exe','FBV.exe','html.xsl') + $pluginNames) {
        $path = if ($name -in $pluginNames) { Join-Path $commonPlugins $name } else { Join-Path $common $name }
        Set-Content -LiteralPath $path -Value "fixture:$name" -Encoding ASCII
    }
    $provenance = Join-Path $fixture 'provenance'
    & (Join-Path $root 'tools\build\build-provenance.ps1') -Action Write -Kind CommonCore -CommonDirectory $common -ProvenanceDirectory $provenance
    & (Join-Path $root 'tools\build\build-provenance.ps1') -Action Validate -Kind CommonCore -CommonDirectory $common -ProvenanceDirectory $provenance
    Add-Content -LiteralPath (Join-Path $commonPlugins 'ImportEPUB.dll') -Value 'tampered' -Encoding ASCII
    try {
        & (Join-Path $root 'tools\build\build-provenance.ps1') -Action Validate -Kind CommonCore -CommonDirectory $common -ProvenanceDirectory $provenance
        throw 'Tampered ImportEPUB.dll was accepted.'
    }
    catch {
        if ($_.Exception.Message -notmatch 'ImportEPUB\.dll') { throw }
    }
    Write-Host 'Build provenance stale ImportEPUB regression passed.'
    New-Item -ItemType Directory -Force -Path $runtime,$batch,$arch | Out-Null
    foreach ($name in @('Scintilla.dll','Lexilla.dll')) { Set-Content -LiteralPath (Join-Path $runtime $name) -Value "fixture:$name" -Encoding ASCII }
    foreach ($name in @('ExportDOCXBatch.exe','ExportEPUBBatch.exe','ImportEPUBBatch.exe')) { Set-Content -LiteralPath (Join-Path $batch $name) -Value "fixture:$name" -Encoding ASCII }
    foreach ($name in @('ZipHandler.exe','RarHandler.exe')) { Set-Content -LiteralPath (Join-Path $arch $name) -Value "fixture:$name" -Encoding ASCII }
    & (Join-Path $root 'tools\build\build-provenance.ps1') -Action Write -Kind Runtime -ProfileDirectory $runtime -BatchDirectory $batch -ArchHandlerDirectory $arch -ProvenanceDirectory $provenance
    & (Join-Path $root 'tools\build\build-provenance.ps1') -Action Validate -Kind Runtime -ProfileDirectory $runtime -BatchDirectory $batch -ArchHandlerDirectory $arch -ProvenanceDirectory $provenance
    Add-Content -LiteralPath (Join-Path $runtime 'Scintilla.dll') -Value 'tampered' -Encoding ASCII
    try {
        & (Join-Path $root 'tools\build\build-provenance.ps1') -Action Validate -Kind Runtime -ProfileDirectory $runtime -BatchDirectory $batch -ArchHandlerDirectory $arch -ProvenanceDirectory $provenance
        throw 'Tampered Scintilla.dll was accepted.'
    }
    catch {
        if ($_.Exception.Message -notmatch 'Scintilla\.dll') { throw }
    }
    Write-Host 'Runtime provenance tamper regression passed.'
}
finally {
    Remove-Item -LiteralPath $fixture -Recurse -Force -ErrorAction SilentlyContinue
}
