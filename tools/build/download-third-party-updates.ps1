<#
.SYNOPSIS
Downloads and expands archive-based dependency updates for inspection.
Git submodules are intentionally not downloaded here: apply-third-party-update.ps1 updates their gitlink safely.
#>

[CmdletBinding()]
param(
    [string[]]$Dependency = @('all'),
    [string]$DestinationRoot,
    [switch]$Force,
    [switch]$AllowCurrentVersion
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'ThirdPartySources.ps1')

$repoRoot = Get-ThirdPartyRepoRoot
if (-not $DestinationRoot) { $DestinationRoot = Join-Path $repoRoot 'tmp\third-party-updates' }

function Resolve-DependenciesToDownload {
    param([string[]]$Selected)
    if ($Selected -contains 'all') { return Get-DependencyCatalog }
    return $Selected | ForEach-Object { Get-DependencyEntry -Name $_ }
}

function Download-File {
    param([Parameter(Mandatory)][string]$Uri,[Parameter(Mandatory)][string]$OutputPath)
    Enable-ModernTls
    Invoke-WebRequest -Uri $Uri -OutFile $OutputPath -Headers @{ 'User-Agent'='Mozilla/5.0 (compatible; FBeditor-third-party-update-download)' } -MaximumRedirection 10 -UseBasicParsing
}

function Test-ZipArchiveSignature {
    param([Parameter(Mandatory)][string]$Path)
    if (-not (Test-Path -LiteralPath $Path)) { return $false }
    $stream = [IO.File]::OpenRead($Path)
    try {
        if ($stream.Length -lt 4) { return $false }
        return ($stream.ReadByte() -eq 0x50) -and ($stream.ReadByte() -eq 0x4B)
    }
    finally { $stream.Dispose() }
}

function Resolve-LocalArchiveFallback {
    param([Parameter(Mandatory)]$DependencyEntry,[Parameter(Mandatory)]$Info)
    if ($DependencyEntry.Name -ne 'wtl') { return $null }
    $candidates = @()
    if ($env:FBE_WTL_ARCHIVE) { $candidates += $env:FBE_WTL_ARCHIVE }
    $candidates += (Join-Path (Split-Path $repoRoot -Parent) $Info.RemoteTag)
    foreach ($candidate in $candidates) {
        if ((Test-Path -LiteralPath $candidate) -and (Test-ZipArchiveSignature -Path $candidate)) { return $candidate }
    }
    return $null
}

function Expand-DependencyArchive {
    param([Parameter(Mandatory)]$DependencyEntry,[Parameter(Mandatory)][string]$ZipPath,[Parameter(Mandatory)][string]$DestinationPath)
    if ($DependencyEntry.SourceSubdirectory) {
        if (Test-Path -LiteralPath $DestinationPath) { Remove-Item -LiteralPath $DestinationPath -Recurse -Force }
        New-Item -ItemType Directory -Path $DestinationPath -Force | Out-Null
        Expand-Archive -LiteralPath $ZipPath -DestinationPath $DestinationPath -Force
        return
    }
    Expand-ZipArchiveToSingleRoot -ZipPath $ZipPath -DestinationPath $DestinationPath
}

function Save-UpdateMetadata {
    param([Parameter(Mandatory)][string]$Directory,[Parameter(Mandatory)]$Info)
    [pscustomobject]@{
        Name=$Info.Name; DisplayName=$Info.DisplayName; Repository=$Info.Repository; LocalVersion=$Info.LocalVersion
        RemoteVersion=$Info.RemoteVersion; RemoteTag=$Info.RemoteTag; RemoteCommit=$Info.RemoteCommit
        RemoteZipUrl=$Info.RemoteZipUrl; RemoteSource=$Info.RemoteSource; DownloadedAt=(Get-Date).ToString('s')
    } | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $Directory 'download-metadata.json') -Encoding UTF8
}

$dependencies = Resolve-DependenciesToDownload -Selected $Dependency
New-Item -ItemType Directory -Path $DestinationRoot -Force | Out-Null

foreach ($entry in $dependencies) {
    $info = Get-DependencyUpdateInfo -Dependency $entry

    if ($entry.UpdateMode -eq 'GitSubmodule') {
        Write-Host ("{0}: git submodule; separate archive download is not required. Use apply-third-party-update.ps1 -Dependency {1}." -f $entry.DisplayName,$entry.Name)
        continue
    }

    if ($entry.UpdateMode -ne 'ReplaceTree') {
        Write-Host ("{0}: automatic download is disabled for update mode {1}; checker only." -f $entry.DisplayName,$entry.UpdateMode)
        continue
    }

    if ($info.Status -eq 'NotInstalled' -and -not $info.RemoteZipUrl) {
        Write-Host ("{0}: not installed and no archive URL configured; skipping." -f $entry.DisplayName)
        continue
    }

    if ($info.Status -eq 'UpToDate' -and -not $AllowCurrentVersion) {
        Write-Host ("{0}: local version {1} is already current." -f $entry.DisplayName,$info.LocalVersion)
        continue
    }
    if ($info.Status -eq 'LocalNewer' -and -not $AllowCurrentVersion) {
        Write-Host ("{0}: local version {1} is newer than upstream {2}; skipping." -f $entry.DisplayName,$info.LocalVersion,$info.RemoteVersion)
        continue
    }
    if (-not $info.RemoteZipUrl) { throw "No archive URL configured for $($entry.DisplayName)." }

    $targetDirectory = Join-Path $DestinationRoot ("{0}-{1}" -f $info.Name,$info.RemoteVersion)
    $archivePath = Join-Path $DestinationRoot ("{0}-{1}.zip" -f $info.Name,$info.RemoteVersion)
    if ((Test-Path -LiteralPath $targetDirectory) -and -not $Force) {
        Write-Host "Already exists: $targetDirectory"
        continue
    }
    if (Test-Path -LiteralPath $archivePath) { Remove-Item -LiteralPath $archivePath -Force }

    Write-Host ("Downloading {0} {1}..." -f $info.DisplayName,$info.RemoteVersion)
    Download-File -Uri $info.RemoteZipUrl -OutputPath $archivePath
    if (-not (Test-ZipArchiveSignature -Path $archivePath)) {
        $fallback = Resolve-LocalArchiveFallback -DependencyEntry $entry -Info $info
        if ($fallback) { Copy-Item -LiteralPath $fallback -Destination $archivePath -Force }
    }
    if (-not (Test-ZipArchiveSignature -Path $archivePath)) { throw "Downloaded file is not a ZIP archive: $archivePath" }

    if (Test-Path -LiteralPath $targetDirectory) { Remove-Item -LiteralPath $targetDirectory -Recurse -Force }
    Expand-DependencyArchive -DependencyEntry $entry -ZipPath $archivePath -DestinationPath $targetDirectory
    Save-UpdateMetadata -Directory $targetDirectory -Info $info
    Write-Host "Ready for inspection: $targetDirectory"
}
