<#
.SYNOPSIS
Safely applies a selected dependency update.
Git submodules are updated by moving the submodule HEAD to the latest stable tagged commit; vendored trees use staging + backup.
#>

[CmdletBinding(SupportsShouldProcess=$true,ConfirmImpact='High')]
param(
    [Parameter(Mandatory)][string]$Dependency,
    [string]$SourcePath,
    [string]$DownloadedRoot,
    [string]$BackupRoot,
    [string]$StagingRoot,
    [switch]$Stage,
    [switch]$Force
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'ThirdPartySources.ps1')

$repoRoot = Get-ThirdPartyRepoRoot
$entry = Get-DependencyEntry -Name $Dependency
if (-not $DownloadedRoot) { $DownloadedRoot = Join-Path $repoRoot 'tmp\third-party-updates' }
if (-not $BackupRoot) { $BackupRoot = Join-Path $repoRoot 'tmp\third-party-backups' }
if (-not $StagingRoot) { $StagingRoot = Join-Path $repoRoot 'tmp\third-party-staging' }

function New-SafeTimestamp { return (Get-Date).ToString('yyyyMMdd-HHmmss') }

function Save-SubmoduleApplyMetadata {
    param($DependencyEntry,[string]$PreviousCommit,$Info,[bool]$Staged)
    New-Item -ItemType Directory -Path $BackupRoot -Force | Out-Null
    [pscustomobject]@{
        Dependency=$DependencyEntry.Name; DisplayName=$DependencyEntry.DisplayName; PreviousCommit=$PreviousCommit
        NewCommit=$Info.RemoteCommit; NewVersion=$Info.RemoteVersion; NewTag=$Info.RemoteTag; Staged=$Staged; AppliedAt=(Get-Date).ToString('s')
    } | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $BackupRoot ("{0}-last-apply.json" -f $DependencyEntry.Name)) -Encoding UTF8
}

function Apply-GitSubmoduleUpdate {
    param([Parameter(Mandatory)]$DependencyEntry)

    $info = Get-DependencyUpdateInfo -Dependency $DependencyEntry
    if ($info.Status -eq 'UpToDate' -and -not $Force) {
        Write-Host ("{0} is already current ({1})." -f $DependencyEntry.DisplayName,$info.RemoteVersion)
        return
    }
    if (-not $info.RemoteCommit) { throw "No tagged upstream commit resolved for $($DependencyEntry.DisplayName)." }

    $relativePath = $DependencyEntry.RelativePath -replace '\\','/'
    $localPath = $DependencyEntry.LocalPath
    $operation = "checkout $($info.RemoteTag) ($($info.RemoteCommit)) in submodule $relativePath"
    if (-not $PSCmdlet.ShouldProcess($relativePath,$operation)) { return }

    Invoke-GitCapture -WorkingDirectory $repoRoot -Arguments @('submodule','update','--init','--',$relativePath) | Out-Null
    if (-not (Test-Path -LiteralPath $localPath)) { throw "Submodule directory was not initialized: $localPath" }

    $dirty = @(Invoke-GitCapture -WorkingDirectory $localPath -Arguments @('status','--porcelain'))
    if ($dirty.Count -gt 0 -and -not $Force) {
        throw "Submodule contains local changes: $relativePath. Commit/stash them first or use -Force deliberately."
    }

    $previousCommit = ([string](Invoke-GitCapture -WorkingDirectory $localPath -Arguments @('rev-parse','HEAD') | Select-Object -First 1)).Trim().ToLowerInvariant()
    Invoke-GitCapture -WorkingDirectory $localPath -Arguments @('fetch','--tags','--force','origin') | Out-Null
    Invoke-GitCapture -WorkingDirectory $localPath -Arguments @('checkout','--detach',$info.RemoteCommit) | Out-Null
    $actual = ([string](Invoke-GitCapture -WorkingDirectory $localPath -Arguments @('rev-parse','HEAD') | Select-Object -First 1)).Trim().ToLowerInvariant()
    if ($actual -ne $info.RemoteCommit.ToLowerInvariant()) {
        throw "Submodule verification failed: expected $($info.RemoteCommit), got $actual"
    }

    $staged = $false
    if ($Stage) {
        Invoke-GitCapture -WorkingDirectory $repoRoot -Arguments @('add','--',$relativePath) | Out-Null
        $staged = $true
    }
    Save-SubmoduleApplyMetadata -DependencyEntry $DependencyEntry -PreviousCommit $previousCommit -Info $info -Staged $staged

    Write-Host ("Updated: {0} -> {1} ({2})" -f $DependencyEntry.DisplayName,$info.RemoteVersion,$info.RemoteTag)
    Write-Host ("Previous commit: {0}" -f $previousCommit)
    Write-Host ("New commit:      {0}" -f $actual)
    if (-not $Stage) { Write-Host ("Stage gitlink with: git add -- {0}" -f $relativePath) }
    Write-Host ('Rollback submodule HEAD with: git -C "{0}" checkout --detach {1}' -f $localPath,$previousCommit)
}

function Get-DirectoryVersionFromName {
    param([Parameter(Mandatory)][string]$DirectoryName,[Parameter(Mandatory)][string]$Prefix)
    if ($DirectoryName -notlike "$Prefix-*") { return $null }
    $versionText = $DirectoryName.Substring($Prefix.Length + 1)
    try { return [version]$versionText } catch { return $null }
}

function Resolve-SourceDirectory {
    param($DependencyEntry,[string]$ExplicitPath,[string]$DownloadsPath)
    if ($ExplicitPath) { return (Resolve-Path -LiteralPath $ExplicitPath).Path }
    if (-not (Test-Path -LiteralPath $DownloadsPath)) { throw "Downloaded updates directory not found: $DownloadsPath" }

    $best = Get-ChildItem -LiteralPath $DownloadsPath -Directory |
        Where-Object { $_.Name -like "$($DependencyEntry.Name)-*" } |
        ForEach-Object { [pscustomobject]@{ FullName=$_.FullName; Version=(Get-DirectoryVersionFromName -DirectoryName $_.Name -Prefix $DependencyEntry.Name); LastWriteTime=$_.LastWriteTime } } |
        Where-Object { $_.Version -ne $null } |
        Sort-Object Version,LastWriteTime -Descending |
        Select-Object -First 1

    if (-not $best) { throw "No downloaded source directory found for $($DependencyEntry.DisplayName) in $DownloadsPath" }
    return $best.FullName
}

function Resolve-SourceLayout {
    param($DependencyEntry,[string]$CandidatePath)
    try {
        Assert-DependencySourceTree -Dependency $DependencyEntry -Path $CandidatePath
        return $CandidatePath
    }
    catch {
        if ($DependencyEntry.SourceSubdirectory) {
            $nested = Join-Path $CandidatePath $DependencyEntry.SourceSubdirectory
            if (Test-Path -LiteralPath $nested) {
                Assert-DependencySourceTree -Dependency $DependencyEntry -Path $nested
                return $nested
            }
        }
        $subdirs = @(Get-ChildItem -LiteralPath $CandidatePath -Directory -ErrorAction Stop)
        if ($subdirs.Count -eq 1) {
            Assert-DependencySourceTree -Dependency $DependencyEntry -Path $subdirs[0].FullName
            return $subdirs[0].FullName
        }
        throw
    }
}

function Apply-ReplaceTreeUpdate {
    param([Parameter(Mandatory)]$DependencyEntry)

    $sourceDirectory = Resolve-SourceDirectory -DependencyEntry $DependencyEntry -ExplicitPath $SourcePath -DownloadsPath $DownloadedRoot
    $sourceDirectory = Resolve-SourceLayout -DependencyEntry $DependencyEntry -CandidatePath $sourceDirectory

    $sourceEntry = [pscustomobject]@{ Kind=$DependencyEntry.Kind; VersionReader=$DependencyEntry.VersionReader; LocalPath=$sourceDirectory }
    $sourceVersion = Get-LocalDependencyVersion -Dependency $sourceEntry
    $targetDirectory = $DependencyEntry.LocalPath
    $currentVersion = $null
    if (Test-Path -LiteralPath $targetDirectory) {
        Assert-DependencySourceTree -Dependency $DependencyEntry -Path $targetDirectory
        $currentVersion = Get-LocalDependencyVersion -Dependency $DependencyEntry
    }
    if ($currentVersion -and $currentVersion -eq $sourceVersion -and -not $Force) {
        throw "$($DependencyEntry.DisplayName) $sourceVersion is already installed. Use -Force only for intentional re-apply."
    }

    New-Item -ItemType Directory -Path $BackupRoot -Force | Out-Null
    New-Item -ItemType Directory -Path $StagingRoot -Force | Out-Null
    $timestamp = New-SafeTimestamp
    $stagingDirectory = Join-Path $StagingRoot ("{0}-{1}-{2}" -f $DependencyEntry.Name,$sourceVersion,$timestamp)
    $backupDirectory = if ($currentVersion) { Join-Path $BackupRoot ("{0}-{1}-{2}" -f $DependencyEntry.Name,$currentVersion,$timestamp) } else { Join-Path $BackupRoot ("{0}-empty-{1}" -f $DependencyEntry.Name,$timestamp) }

    if (-not $PSCmdlet.ShouldProcess($targetDirectory,("{0} {1} -> {2}" -f $DependencyEntry.DisplayName,$sourceVersion,$targetDirectory))) { return }

    Copy-Item -LiteralPath $sourceDirectory -Destination $stagingDirectory -Recurse -Force
    Assert-DependencySourceTree -Dependency $DependencyEntry -Path $stagingDirectory

    $backupCreated = $false
    $deployed = $false
    try {
        if (Test-Path -LiteralPath $targetDirectory) {
            Move-Item -LiteralPath $targetDirectory -Destination $backupDirectory
            $backupCreated = $true
        }
        Move-Item -LiteralPath $stagingDirectory -Destination $targetDirectory
        $deployed = $true
    }
    catch {
        if (-not $deployed -and $backupCreated -and -not (Test-Path -LiteralPath $targetDirectory) -and (Test-Path -LiteralPath $backupDirectory)) {
            Move-Item -LiteralPath $backupDirectory -Destination $targetDirectory -ErrorAction SilentlyContinue
        }
        throw
    }
    finally {
        if (Test-Path -LiteralPath $stagingDirectory) { Remove-Item -LiteralPath $stagingDirectory -Recurse -Force -ErrorAction SilentlyContinue }
    }

    Assert-DependencySourceTree -Dependency $DependencyEntry -Path $targetDirectory
    $finalVersion = Get-LocalDependencyVersion -Dependency $DependencyEntry
    Write-Host ("Installed: {0} {1}" -f $DependencyEntry.DisplayName,$finalVersion)
    if ($currentVersion) { Write-Host "Previous version: $currentVersion" }
    Write-Host "Backup: $backupDirectory"
}

if ($entry.UpdateMode -eq 'GitSubmodule') { Apply-GitSubmoduleUpdate -DependencyEntry $entry }
elseif ($entry.UpdateMode -eq 'ReplaceTree') { Apply-ReplaceTreeUpdate -DependencyEntry $entry }
else { throw "Automatic apply is not supported for $($entry.DisplayName) (mode: $($entry.UpdateMode))." }
