<#
.SYNOPSIS
Скачивает и распаковывает архивы исходников выбранных сторонних зависимостей.
#>

[CmdletBinding()]
param(
    [ValidateSet("all", "scintilla", "lexilla", "pcre2", "hunspell")]
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

    $client = New-Object System.Net.WebClient
    $client.Headers["User-Agent"] = "FBeditor-third-party-update-download"

    try {
        $client.DownloadFile($Uri, $OutputPath)
    }
    finally {
        $client.Dispose()
    }
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

    if (Test-Path -LiteralPath $targetDirectory) {
        Remove-Item -LiteralPath $targetDirectory -Recurse -Force
    }

    Expand-ZipArchiveToSingleRoot -ZipPath $archivePath -DestinationPath $targetDirectory
    Save-UpdateMetadata -Directory $targetDirectory -Info $info

    Write-Host (Format-ThirdPartyText "0JPQvtGC0L7QstC+OiB7MH0=" $targetDirectory)
}
