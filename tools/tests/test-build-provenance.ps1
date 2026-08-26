<# Verifies that tampering a common payload is rejected before staging/tests. #>
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$fixture = Join-Path ([IO.Path]::GetTempPath()) "fbe-provenance-$PID"
$common = Join-Path $fixture 'common'
try {
    New-Item -ItemType Directory -Force -Path $common | Out-Null
    foreach ($name in @('FBE.exe','FBV.exe','ExportHTML.dll','ExportDOCX.dll','ExportEPUB.dll','ImportEPUB.dll','ImportEPUBLunaSVG.dll','html.xsl')) {
        Set-Content -LiteralPath (Join-Path $common $name) -Value "fixture:$name" -Encoding ASCII
    }
    $provenance = Join-Path $fixture 'provenance'
    & (Join-Path $root 'tools\build\build-provenance.ps1') -Action Write -Kind CommonCore -CommonDirectory $common -ProvenanceDirectory $provenance
    & (Join-Path $root 'tools\build\build-provenance.ps1') -Action Validate -Kind CommonCore -CommonDirectory $common -ProvenanceDirectory $provenance
    Add-Content -LiteralPath (Join-Path $common 'ImportEPUB.dll') -Value 'tampered' -Encoding ASCII
    try {
        & (Join-Path $root 'tools\build\build-provenance.ps1') -Action Validate -Kind CommonCore -CommonDirectory $common -ProvenanceDirectory $provenance
        throw 'Tampered ImportEPUB.dll was accepted.'
    }
    catch {
        if ($_.Exception.Message -notmatch 'ImportEPUB\.dll') { throw }
    }
    Write-Host 'Build provenance stale ImportEPUB regression passed.'
}
finally {
    Remove-Item -LiteralPath $fixture -Recurse -Force -ErrorAction SilentlyContinue
}
