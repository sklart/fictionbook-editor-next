<#
.SYNOPSIS
Скачивает и распаковывает архивы исходников выбранных сторонних зависимостей.
#>

[CmdletBinding()]
param(
    [ValidateSet("all", "scintilla", "lexilla", "pcre2", "hunspell", "wtl")]
    [string[]]$Dependency = @("all"),

    [string]$DestinationRoot,

    [switch]$Force,

    [switch]$AllowCurrentVersion
)

$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "ThirdPartySources.ps1")

$repoRoot = Get-ThirdPartyRepoRoot
if (-not $DestinationRoot) {
    $DestinationRoot = Join-Path $repoRoot "tmp\third-party-updates"
}

function Resolve-DependenciesToDownload {
    param(
        [string[]]$Selected
    )

    if ($Selected -contains "all") {
        return Get-DependencyCatalog
    }

    return $Selected | ForEach-Object { Get-DependencyEntry -Name $_ }
}

function Save-UpdateMetadata {
    param(
        [Parameter(Mandatory)]
        [string]$Directory,

        [Parameter(Mandatory)]
        $Info
    )

    $metadata = [pscustomobject]@{
        Name = $Info.Name
        DisplayName = $Info.DisplayName
        Repository = $Info.Repository
        LocalVersion = $Info.LocalVersion
        RemoteVersion = $Info.RemoteVersion
        RemoteTag = $Info.RemoteTag
        RemoteZipUrl = $Info.RemoteZipUrl
        RemoteSource = $Info.RemoteSource
        DownloadedAt = (Get-Date).ToString("s")
    }

    $metadata | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $Directory "download-metadata.json") -Encoding UTF8
}

function Download-File {
    param(
        [Parameter(Mandatory)]
        [string]$Uri,

        [Parameter(Mandatory)]
        [string]$OutputPath
    )

    Enable-ModernTls

    Invoke-WebRequest `
        -Uri $Uri `
        -OutFile $OutputPath `
        -Headers @{ "User-Agent" = "Mozilla/5.0 (compatible; FBeditor-third-party-update-download)" } `
        -MaximumRedirection 10 `
        -UseBasicParsing
}

function Test-ZipArchiveSignature {
    param(
        [Parameter(Mandatory)]
        [string]$Path
    )

    if (-not (Test-Path -LiteralPath $Path)) {
        return $false
    }

    $stream = [IO.File]::OpenRead($Path)
    try {
        if ($stream.Length -lt 4) {
            return $false
        }

        return ($stream.ReadByte() -eq 0x50) -and
            ($stream.ReadByte() -eq 0x4B)
    }
    finally {
        $stream.Dispose()
    }
}

function Resolve-LocalArchiveFallback {
    param(
        [Parameter(Mandatory)]
        $DependencyEntry,

        [Parameter(Mandatory)]
        $Info
    )

    if ($DependencyEntry.Name -ne "wtl") {
        return $null
    }

    $candidates = @()
    if ($env:FBE_WTL_ARCHIVE) {
        $candidates += $env:FBE_WTL_ARCHIVE
    }

    $candidates += (Join-Path (Split-Path $repoRoot -Parent) $Info.RemoteTag)

    foreach ($candidate in $candidates) {
        if ((Test-Path -LiteralPath $candidate) -and (Test-ZipArchiveSignature -Path $candidate)) {
            return $candidate
        }
    }

    return $null
}

function Expand-DependencyArchive {
    param(
        [Parameter(Mandatory)]
        $DependencyEntry,

        [Parameter(Mandatory)]
        [string]$ZipPath,

        [Parameter(Mandatory)]
        [string]$DestinationPath
    )

    if ($DependencyEntry.SourceSubdirectory) {
        if (Test-Path -LiteralPath $DestinationPath) {
            Remove-Item -LiteralPath $DestinationPath -Recurse -Force
        }

        New-Item -ItemType Directory -Path $DestinationPath -Force | Out-Null
        Expand-Archive -LiteralPath $ZipPath -DestinationPath $DestinationPath -Force
        return
    }

    Expand-ZipArchiveToSingleRoot -ZipPath $ZipPath -DestinationPath $DestinationPath
}

$dependencies = Resolve-DependenciesToDownload -Selected $Dependency
New-Item -ItemType Directory -Path $DestinationRoot -Force | Out-Null

foreach ($entry in $dependencies) {
    $info = Get-DependencyUpdateInfo -Dependency $entry

    if ($info.Status -eq "UpToDate" -and -not $AllowCurrentVersion) {
        Write-Host (Format-ThirdPartyText "0J/RgNC+0L/Rg9GB0LrQsNGOIHswfTog0LvQvtC60LDQu9GM0L3QsNGPINCy0LXRgNGB0LjRjyB7MX0g0YPQttC1INGB0L7QstC/0LDQtNCw0LXRgiDRgSDRg9C00LDQu9GR0L3QvdC+0Lku" $info.DisplayName, $info.LocalVersion)
        continue
    }

    if ($info.Status -eq "LocalNewer" -and -not $AllowCurrentVersion) {
        Write-Host (Format-ThirdPartyText "0J/RgNC+0L/Rg9GB0LrQsNGOIHswfTog0LvQvtC60LDQu9GM0L3QsNGPINCy0LXRgNGB0LjRjyB7MX0g0L3QvtCy0LXQtSDRg9C00LDQu9GR0L3QvdC+0LkgezJ9Lg==" $info.DisplayName, $info.LocalVersion, $info.RemoteVersion)
        continue
    }

    $targetDirectory = Join-Path $DestinationRoot ("{0}-{1}" -f $info.Name, $info.RemoteVersion)
    $archivePath = Join-Path $DestinationRoot ("{0}-{1}.zip" -f $info.Name, $info.RemoteVersion)

    if ((Test-Path -LiteralPath $targetDirectory) -and -not $Force) {
        Write-Host (Format-ThirdPartyText "0JrQsNGC0LDQu9C+0LMg0YPQttC1INGB0YPRidC10YHRgtCy0YPQtdGCLCDQv9GA0L7Qv9GD0YHQutCw0Y4g0YHQutCw0YfQuNCy0LDQvdC40LU6IHswfQ==" $targetDirectory)
        continue
    }

    if (Test-Path -LiteralPath $archivePath) {
        Remove-Item -LiteralPath $archivePath -Force
    }

    Write-Host (Format-ThirdPartyText "0KHQutCw0YfQuNCy0LDRjiB7MH0gezF9Li4u" $info.DisplayName, $info.RemoteVersion)
    Download-File -Uri $info.RemoteZipUrl -OutputPath $archivePath
    if (-not (Test-ZipArchiveSignature -Path $archivePath)) {
        $fallbackArchive = Resolve-LocalArchiveFallback -DependencyEntry $entry -Info $info
        if ($fallbackArchive) {
            Write-Warning ("SourceForge вернул не ZIP-архив; использую локально скачанный архив: {0}" -f $fallbackArchive)
            Copy-Item -LiteralPath $fallbackArchive -Destination $archivePath -Force
        }
    }

    if (-not (Test-ZipArchiveSignature -Path $archivePath)) {
        throw ("Скачанный файл не является ZIP-архивом: {0}. Для WTL можно указать локальный архив через переменную FBE_WTL_ARCHIVE." -f $archivePath)
    }

    if (Test-Path -LiteralPath $targetDirectory) {
        Remove-Item -LiteralPath $targetDirectory -Recurse -Force
    }

    Expand-DependencyArchive -DependencyEntry $entry -ZipPath $archivePath -DestinationPath $targetDirectory
    Save-UpdateMetadata -Directory $targetDirectory -Info $info

    Write-Host (Format-ThirdPartyText "0JPQvtGC0L7QstC+OiB7MH0=" $targetDirectory)
}
