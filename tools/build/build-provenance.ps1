<# Validates or records authoritative release payload hashes. #>
[CmdletBinding()]
param(
    [ValidateSet('Write', 'Validate')][string]$Action,
    [ValidateSet('CommonCore', 'Runtime')][string]$Kind,
    [string]$Configuration = 'Release',
    [string]$CommonDirectory,
    [string]$ProfileDirectory,
    [string]$BatchDirectory,
    [string]$ArchHandlerDirectory,
    [string]$PlatformToolset,
    [string]$ProvenanceDirectory
)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$provenanceRoot = if ($ProvenanceDirectory) { $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($ProvenanceDirectory) } else { Join-Path $repoRoot 'out\build-provenance' }
$manifestPath = Join-Path $provenanceRoot ("{0}-{1}.json" -f $Kind, $Configuration)

function Get-FileDigest([string]$Path, [string]$Name) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { throw "Provenance artifact missing: $Name ($Path)" }
    return @{ sha256 = (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash }
}

function Get-SourcePath([string]$Directory, [string]$Name) {
    if (-not $Directory) { throw "Provenance source directory is not set for $Name." }
    return Join-Path $Directory $Name
}

function Get-CommonSourcePath([string]$Directory, [string]$Name) {
    if ($Name -in $pluginNames) { return Join-Path (Join-Path $Directory 'Plugins') $Name }
    return Get-SourcePath $Directory $Name
}

$commonNames = @('FBE.exe','FBV.exe','ExportHTML.dll','ExportDOCX.dll','ExportEPUB.dll','ImportEPUB.dll','ImportEPUBLunaSVG.dll','html.xsl')
$pluginNames = @('ExportHTML.dll','ExportDOCX.dll','ExportEPUB.dll','ImportEPUB.dll','ImportEPUBLunaSVG.dll')
$profileNames = @('Scintilla.dll','Lexilla.dll')
if ($Kind -ne 'CommonCore') { $profileNames += @('ExportDOCXBatch.exe','ExportEPUBBatch.exe','ImportEPUBBatch.exe','Utilities/ArchHandler/ZipHandler.exe','Utilities/ArchHandler/RarHandler.exe') }

if ($Action -eq 'Write') {
    $artifacts = [ordered]@{}
    if ($Kind -eq 'CommonCore') {
        foreach ($name in $commonNames) { $artifacts[$name] = Get-FileDigest (Get-CommonSourcePath $CommonDirectory $name) $name }
    }
    else {
        foreach ($name in $profileNames) {
            $source = if ($name -like '*Batch.exe') { Get-SourcePath $BatchDirectory $name }
                elseif ($name -like 'Utilities/*') { Get-SourcePath $ArchHandlerDirectory (Split-Path $name -Leaf) }
                else { Get-SourcePath $ProfileDirectory $name }
            $artifacts[$name] = Get-FileDigest $source $name
        }
    }
    New-Item -ItemType Directory -Force -Path $provenanceRoot | Out-Null
    $commit = (& git -C $repoRoot rev-parse HEAD).Trim()
    [ordered]@{ schemaVersion=1; kind=$Kind; configuration=$Configuration; platform='Win32'; platformToolset=$PlatformToolset; gitCommit=$commit; artifacts=$artifacts } | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $manifestPath -Encoding UTF8
    Write-Host "Build provenance written: $manifestPath"
    exit 0
}

if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) { throw "Build provenance is missing: $manifestPath. Run the authoritative full build pipeline." }
$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
foreach ($property in $manifest.artifacts.PSObject.Properties) {
    $name = $property.Name
    $source = if ($Kind -eq 'CommonCore') { Get-CommonSourcePath $CommonDirectory $name }
        elseif ($name -like '*Batch.exe') { Get-SourcePath $BatchDirectory $name }
        elseif ($name -like 'Utilities/*') { Get-SourcePath $ArchHandlerDirectory (Split-Path $name -Leaf) }
        else { Get-SourcePath $ProfileDirectory $name }
    $actual = (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash
    if ($actual -ne $property.Value.sha256) {
        $hint = if ($name -eq 'ImportEPUB.dll') { ' ImportEPUB.dll is stale or was rebuilt outside the authoritative common build. Run the full common build pipeline.' } else { '' }
        throw "Build provenance mismatch: $name.$hint"
    }
}
Write-Host "Build provenance validated: $manifestPath"
