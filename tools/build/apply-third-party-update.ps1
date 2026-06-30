<#
.SYNOPSIS
Безопасно раскладывает уже скачанное обновление выбранной зависимости в third_party через staging и backup.
#>

[CmdletBinding(SupportsShouldProcess = $true, ConfirmImpact = "High")]
param(
    [Parameter(Mandatory)]
    [ValidateSet("scintilla", "lexilla", "pcre2", "hunspell")]
    [string]$Dependency,

    [string]$SourcePath,

    [string]$DownloadedRoot,

    [string]$BackupRoot,

    [string]$StagingRoot,

    [switch]$Force
)

$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "ThirdPartySources.ps1")

$repoRoot = Get-ThirdPartyRepoRoot
$entry = Get-DependencyEntry -Name $Dependency

if (-not $DownloadedRoot) {
    $DownloadedRoot = Join-Path $repoRoot "tmp\third-party-updates"
}

if (-not $BackupRoot) {
    $BackupRoot = Join-Path $repoRoot "tmp\third-party-backups"
}

if (-not $StagingRoot) {
    $StagingRoot = Join-Path $repoRoot "tmp\third-party-staging"
}

function Get-DirectoryVersionFromName {
    param(
        [Parameter(Mandatory)]
        [string]$DirectoryName,

        [Parameter(Mandatory)]
        [string]$Prefix
    )

    if ($DirectoryName -notlike "$Prefix-*") {
        return $null
    }

    $versionText = $DirectoryName.Substring($Prefix.Length + 1)
    try {
        return [version]$versionText
    }
    catch {
        return $null
    }
}

function Resolve-SourceDirectory {
    param(
        [Parameter(Mandatory)]
        $DependencyEntry,

        [string]$ExplicitPath,

        [Parameter(Mandatory)]
        [string]$DownloadsPath
    )

    if ($ExplicitPath) {
        return (Resolve-Path -LiteralPath $ExplicitPath).Path
    }

    if (-not (Test-Path -LiteralPath $DownloadsPath)) {
        throw "Ne naiden katalog so skachannymi obnovleniyami: $DownloadsPath"
    }

    $candidates =
        Get-ChildItem -LiteralPath $DownloadsPath -Directory |
        Where-Object { $_.Name -like "$($DependencyEntry.Name)-*" } |
        ForEach-Object {
            [pscustomobject]@{
                FullName = $_.FullName
                Name = $_.Name
                Version = Get-DirectoryVersionFromName -DirectoryName $_.Name -Prefix $DependencyEntry.Name
                LastWriteTime = $_.LastWriteTime
            }
        } |
        Where-Object { $_.Version -ne $null } |
        Sort-Object Version, LastWriteTime -Descending

    $best = $candidates | Select-Object -First 1
    if (-not $best) {
        throw "Ne naiden skachannyj katalog dlya $($DependencyEntry.DisplayName) v $DownloadsPath"
    }

    return $best.FullName
}

function Resolve-SourceLayout {
    param(
        [Parameter(Mandatory)]
        $DependencyEntry,

        [Parameter(Mandatory)]
        [string]$CandidatePath
    )

    try {
        Assert-DependencySourceTree -Dependency $DependencyEntry -Path $CandidatePath
        return $CandidatePath
    }
    catch {
        $subdirectories = Get-ChildItem -LiteralPath $CandidatePath -Directory -ErrorAction Stop
        if ($subdirectories.Count -eq 1) {
            $nestedPath = $subdirectories[0].FullName
            Assert-DependencySourceTree -Dependency $DependencyEntry -Path $nestedPath
            return $nestedPath
        }

        throw
    }
}

function New-SafeTimestamp {
    return (Get-Date).ToString("yyyyMMdd-HHmmss")
}

$sourceDirectory = Resolve-SourceDirectory -DependencyEntry $entry -ExplicitPath $SourcePath -DownloadsPath $DownloadedRoot
$sourceDirectory = Resolve-SourceLayout -DependencyEntry $entry -CandidatePath $sourceDirectory

$sourceVersion = Get-LocalDependencyVersion -Dependency ([pscustomobject]@{
    VersionReader = $entry.VersionReader
    LocalPath = $sourceDirectory
})

$targetDirectory = $entry.LocalPath
$currentVersion = $null
if (Test-Path -LiteralPath $targetDirectory) {
    Assert-DependencySourceTree -Dependency $entry -Path $targetDirectory
    $currentVersion = Get-LocalDependencyVersion -Dependency $entry
}

if ($currentVersion -and $currentVersion -eq $sourceVersion -and -not $Force) {
    throw (Format-ThirdPartyText "0JLQtdGA0YHQuNGPIHswfSB7MX0g0YPQttC1INGA0LDQt9C70L7QttC10L3QsCDQsiB0aGlyZF9wYXJ0eS4g0JTQu9GPINC/0L7QstGC0L7RgNC90L7QuSDRgNCw0YHQutC70LDQtNC60Lgg0YPQutCw0LbQuNGC0LUgLUZvcmNlLg==" $entry.DisplayName, $sourceVersion)
}

New-Item -ItemType Directory -Path $BackupRoot -Force | Out-Null
New-Item -ItemType Directory -Path $StagingRoot -Force | Out-Null

$timestamp = New-SafeTimestamp
$stagingDirectory = Join-Path $StagingRoot ("{0}-{1}-{2}" -f $entry.Name, $sourceVersion, $timestamp)
$backupDirectory = if ($currentVersion) {
    Join-Path $BackupRoot ("{0}-{1}-{2}" -f $entry.Name, $currentVersion, $timestamp)
}
else {
    Join-Path $BackupRoot ("{0}-empty-{1}" -f $entry.Name, $timestamp)
}

$operationDescription = "{0} {1} -> {2}" -f $entry.DisplayName, $sourceVersion, $targetDirectory
if (-not $PSCmdlet.ShouldProcess($targetDirectory, $operationDescription)) {
    return
}

Copy-Item -LiteralPath $sourceDirectory -Destination $stagingDirectory -Recurse -Force
Assert-DependencySourceTree -Dependency $entry -Path $stagingDirectory

$backupCreated = $false
$targetMoved = $false
$deploymentFinished = $false

try {
    if (Test-Path -LiteralPath $targetDirectory) {
        Move-Item -LiteralPath $targetDirectory -Destination $backupDirectory
        $backupCreated = $true
        $targetMoved = $true
    }

    Move-Item -LiteralPath $stagingDirectory -Destination $targetDirectory
    $deploymentFinished = $true
}
catch {
    if (-not $deploymentFinished -and $backupCreated -and -not (Test-Path -LiteralPath $targetDirectory) -and (Test-Path -LiteralPath $backupDirectory)) {
        Move-Item -LiteralPath $backupDirectory -Destination $targetDirectory -ErrorAction SilentlyContinue
    }

    throw
}
finally {
    if ((Test-Path -LiteralPath $stagingDirectory) -and ($deploymentFinished -or $targetMoved)) {
        Remove-Item -LiteralPath $stagingDirectory -Recurse -Force -ErrorAction SilentlyContinue
    }
}

Assert-DependencySourceTree -Dependency $entry -Path $targetDirectory
$finalVersion = Get-LocalDependencyVersion -Dependency $entry

$result = [pscustomobject]@{
    Dependency = $entry.Name
    DisplayName = $entry.DisplayName
    PreviousVersion = $currentVersion
    NewVersion = $finalVersion
    SourcePath = $sourceDirectory
    TargetPath = $targetDirectory
    BackupPath = if ($backupCreated) { $backupDirectory } else { $null }
    AppliedAt = (Get-Date).ToString("s")
}

$result | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $BackupRoot ("{0}-last-apply.json" -f $entry.Name)) -Encoding UTF8

Write-Host (Format-ThirdPartyText "0KDQsNC30LvQvtC20LXQvdC+OiB7MH0gezF9" $entry.DisplayName, $finalVersion)
if ($currentVersion) {
    Write-Host (Format-ThirdPartyText "0J/RgNC10LTRi9C00YPRidCw0Y8g0LLQtdGA0YHQuNGPOiB7MH0=" $currentVersion)
}
Write-Host (Format-ThirdPartyText "0JjRgdGC0L7Rh9C90LjQujogezB9" $sourceDirectory)
Write-Host (Format-ThirdPartyText "0KDQtdC30LXRgNCy0L3QsNGPINC60L7Qv9C40Y86IHswfQ==" $backupDirectory)
Write-Host ""
Write-Host (Get-ThirdPartyText -Base64 "0KDQtdC60L7QvNC10L3QtNGD0LXQvNGL0LUg0YHQu9C10LTRg9GO0YnQuNC1INGI0LDQs9C4Og==")
Write-Host (Get-ThirdPartyText -Base64 "ICAxLiDQn9GA0L7QstC10YDQuNGC0YwgZGlmZiDQsiB0aGlyZF9wYXJ0eSDQuCDRgdCy0Y/Qt9Cw0L3QvdGL0LUgcmVsZWFzZSBub3RlcyB1cHN0cmVhbS4=")
Write-Host (Get-ThirdPartyText -Base64 "ICAyLiDQn9C10YDQtdGB0L7QsdGA0LDRgtGMINC30LDQstC40YHQuNC80L7RgdGC0Ywg0LjQu9C4INCy0LXRgdGMINC/0YDQvtC10LrRgi4=")
Write-Host (Get-ThirdPartyText -Base64 "ICAzLiDQn9GA0L7QudGC0Lgg0YHQvtC+0YLQstC10YLRgdGC0LLRg9GO0YnQuNC1IHNtb2tlL3JlZ3Jlc3Npb24t0L/RgNC+0LLQtdGA0LrQuC4=")
