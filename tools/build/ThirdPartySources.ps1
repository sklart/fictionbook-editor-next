<#
.SYNOPSIS
Common helpers for checking, downloading and applying third-party dependency updates.

.DESCRIPTION
The catalog supports three dependency types:
  * ReplaceTree   - vendored source tree replaced from an upstream archive;
  * GitSubmodule  - repository gitlink is compared with the latest stable upstream tag;
  * VendoredTree  - vendored source tree checked in place; updates are applied manually when FBE-specific files must be preserved.
  * OptionalTree  - optional vendored dependency; absence is reported as NotInstalled.
#>

[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'

# Avoid querying the same upstream twice in one checker run (latest tag + local gitlink tag).
$script:GitRemoteTagCache = @{}

function Get-ThirdPartyText {
    param([Parameter(Mandatory)][string]$Base64)
    return [Text.Encoding]::UTF8.GetString([Convert]::FromBase64String($Base64))
}

function Format-ThirdPartyText {
    param(
        [Parameter(Mandatory)][string]$Base64,
        [Parameter(ValueFromRemainingArguments = $true)][object[]]$Arguments
    )

    $text = Get-ThirdPartyText -Base64 $Base64
    if (-not $Arguments -or $Arguments.Count -eq 0) { return $text }
    return $text -f $Arguments
}

function Get-DependencyStatusDisplay {
    param([Parameter(Mandatory)][string]$Status)

    switch ($Status) {
        'UpdateAvailable' { return 'Доступно обновление' }
        'UpToDate'        { return 'Актуально' }
        'LocalNewer'      { return 'Локальная версия новее' }
        'NeedsReview'     { return 'Требует проверки' }
        'NotInstalled'    { return 'Не установлен' }
        'Error'           { return 'Ошибка' }
        default           { return $Status }
    }
}

function Get-ThirdPartyRepoRoot {
    return (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
}

function Enable-ModernTls {
    try {
        $current = [Net.ServicePointManager]::SecurityProtocol
        $tls12 = [Net.SecurityProtocolType]::Tls12
        if (($current -band $tls12) -eq 0) {
            [Net.ServicePointManager]::SecurityProtocol = $current -bor $tls12
        }
    }
    catch {
        # PowerShell 7+ may not need this setting.
    }
}

function Get-RemoteTextContent {
    param([Parameter(Mandatory)][string]$Uri)

    Enable-ModernTls
    $client = New-Object System.Net.WebClient
    $client.Headers['User-Agent'] = 'Mozilla/5.0 (compatible; FBeditor-third-party-update-check)'
    try { return $client.DownloadString($Uri) }
    finally { $client.Dispose() }
}

function Invoke-GitCapture {
    param(
        [Parameter(Mandatory)][string[]]$Arguments,
        [string]$WorkingDirectory
    )

    if ($WorkingDirectory) {
        $output = & git -C $WorkingDirectory @Arguments 2>&1
    }
    else {
        $output = & git @Arguments 2>&1
    }

    if ($LASTEXITCODE -ne 0) {
        throw ('git {0} failed: {1}' -f ($Arguments -join ' '), (($output | Out-String).Trim()))
    }

    return @($output)
}

function Convert-ScintillaStyleVersion {
    param([Parameter(Mandatory)][string]$RawVersion)

    $trimmed = $RawVersion.Trim()
    if ($trimmed -notmatch '^\d{3,}$') {
        throw "Unexpected Scintilla/Lexilla version format: $RawVersion"
    }

    if ($trimmed.Length -eq 3) {
        return '{0}.{1}.{2}' -f $trimmed.Substring(0, 1), $trimmed.Substring(1, 1), $trimmed.Substring(2, 1)
    }

    return '{0}.{1}.{2}' -f $trimmed.Substring(0, 1), $trimmed.Substring(1, 1), $trimmed.Substring(2)
}

function Get-CMakeProjectVersion {
    param(
        [Parameter(Mandatory)][string]$CMakeListsPath,
        [Parameter(Mandatory)][string]$ProjectName
    )

    if (-not (Test-Path -LiteralPath $CMakeListsPath)) {
        throw "CMakeLists.txt not found: $CMakeListsPath"
    }

    $content = Get-Content -LiteralPath $CMakeListsPath -Raw
    $escaped = [regex]::Escape($ProjectName)
    $match = [regex]::Match(
        $content,
        "(?is)project\s*\(\s*$escaped\b.*?\bVERSION\s+([0-9]+(?:\.[0-9]+){1,3})"
    )

    if ($match.Success) {
        return $match.Groups[1].Value
    }

    # LunaSVG and PlutoVG keep the numeric components in CMake variables and
    # build project(... VERSION ${...}) from them. Support that common layout.
    $prefix = $ProjectName.ToUpperInvariant()
    $parts = @()
    foreach ($suffix in @('MAJOR','MINOR','MICRO')) {
        $partMatch = [regex]::Match($content, "(?im)^\s*set\(\s*${prefix}_VERSION_${suffix}\s+(\d+)\s*\)")
        if (-not $partMatch.Success) {
            throw "Could not read $ProjectName version from $CMakeListsPath"
        }
        $parts += $partMatch.Groups[1].Value
    }

    return ($parts -join '.')
}

function Get-GitRemoteTagRecords {
    param([Parameter(Mandatory)][string]$RepositoryUrl)

    if ($script:GitRemoteTagCache.ContainsKey($RepositoryUrl)) {
        return @($script:GitRemoteTagCache[$RepositoryUrl])
    }

    $lines = Invoke-GitCapture -Arguments @('ls-remote', '--tags', $RepositoryUrl)
    $records = @{}

    foreach ($lineObject in $lines) {
        $line = [string]$lineObject
        $match = [regex]::Match($line, '^([0-9a-fA-F]{40,64})\s+refs/tags/(.+?)(\^\{\})?$')
        if (-not $match.Success) { continue }

        $commit = $match.Groups[1].Value.ToLowerInvariant()
        $tag = $match.Groups[2].Value
        $peeled = $match.Groups[3].Success

        if (-not $records.ContainsKey($tag) -or $peeled) {
            $records[$tag] = [pscustomobject]@{
                Tag = $tag
                Commit = $commit
                Peeled = $peeled
            }
        }
    }

    $result = @($records.Values)
    $script:GitRemoteTagCache[$RepositoryUrl] = $result
    return $result
}

function Get-VersionTextFromTagMatch {
    param([Parameter(Mandatory)][System.Text.RegularExpressions.Match]$Match)

    if (-not $Match.Success -or $Match.Groups.Count -lt 2) { return $null }

    $parts = @()
    for ($index = 1; $index -lt $Match.Groups.Count; $index++) {
        $group = $Match.Groups[$index]
        if ($group.Success -and -not [string]::IsNullOrWhiteSpace($group.Value)) {
            $parts += $group.Value
        }
    }

    if ($parts.Count -eq 0) { return $null }
    if ($parts.Count -eq 1) { return $parts[0] }

    return ($parts -join '.')
}

function Get-LatestStableGitRelease {
    param(
        [Parameter(Mandatory)][string]$RepositoryUrl,
        [Parameter(Mandatory)][string]$TagPattern,
        [string]$ZipUrlTemplate
    )

    $versions = foreach ($record in (Get-GitRemoteTagRecords -RepositoryUrl $RepositoryUrl)) {
        $match = [regex]::Match($record.Tag, $TagPattern)
        if (-not $match.Success) { continue }

        $versionText = Get-VersionTextFromTagMatch -Match $match
        if (-not $versionText) { continue }
        try { $comparison = [version]$versionText }
        catch { continue }

        [pscustomobject]@{
            Tag = $record.Tag
            Version = $versionText
            Comparison = $comparison
            Commit = $record.Commit
        }
    }

    $best = $versions | Sort-Object Comparison -Descending | Select-Object -First 1
    if (-not $best) {
        throw "No stable release tags found: $RepositoryUrl"
    }

    $zipUrl = $null
    if ($ZipUrlTemplate) {
        $zipUrl = $ZipUrlTemplate -f $best.Tag
    }

    return [pscustomobject]@{
        Tag = $best.Tag
        Version = $best.Version
        Commit = $best.Commit
        ZipUrl = $zipUrl
        Source = 'git ls-remote'
    }
}

function Get-GitLinkCommit {
    param([Parameter(Mandatory)][string]$RelativePath)

    $repoRoot = Get-ThirdPartyRepoRoot
    $gitPath = $RelativePath -replace '\\', '/'

    # Read the gitlink from the index rather than HEAD.
    # This makes staged submodule updates visible before they are committed,
    # while still working in CI without initializing the submodule.
    $lines = Invoke-GitCapture -WorkingDirectory $repoRoot -Arguments @(
        'ls-files', '--stage', '--', $gitPath
    )

    $line = ($lines | Select-Object -First 1)
    if (-not $line) { return $null }

    $match = [regex]::Match(
        [string]$line,
        '^160000\s+([0-9a-fA-F]{40,64})\s+\d+\s+'
    )

    if (-not $match.Success) {
        throw "Path is not a git submodule in the index: $RelativePath"
    }

    return $match.Groups[1].Value.ToLowerInvariant()
}

function Get-StableTagForCommit {
    param(
        [Parameter(Mandatory)][string]$RepositoryUrl,
        [Parameter(Mandatory)][string]$TagPattern,
        [Parameter(Mandatory)][string]$Commit
    )

    $matches = foreach ($record in (Get-GitRemoteTagRecords -RepositoryUrl $RepositoryUrl)) {
        if ($record.Commit -ne $Commit.ToLowerInvariant()) { continue }
        $match = [regex]::Match($record.Tag, $TagPattern)
        if (-not $match.Success) { continue }

        $versionText = Get-VersionTextFromTagMatch -Match $match
        if (-not $versionText) { continue }
        try { $comparison = [version]$versionText }
        catch { continue }

        [pscustomobject]@{
            Tag = $record.Tag
            Version = $versionText
            Comparison = $comparison
        }
    }

    return $matches | Sort-Object Comparison -Descending | Select-Object -First 1
}

function New-GitSubmoduleDependency {
    param(
        [Parameter(Mandatory)][string]$Name,
        [Parameter(Mandatory)][string]$DisplayName,
        [Parameter(Mandatory)][string]$RelativePath,
        [Parameter(Mandatory)][string]$RepositoryUrl,
        [Parameter(Mandatory)][string]$TagPattern,
        [string[]]$ValidationPaths = @()
    )

    $repoRoot = Get-ThirdPartyRepoRoot
    return [pscustomobject]@{
        Name = $Name
        DisplayName = $DisplayName
        Repository = $RepositoryUrl
        RepositoryUrl = $RepositoryUrl
        LocalPath = Join-Path $repoRoot $RelativePath
        RelativePath = $RelativePath
        ValidationPaths = $ValidationPaths
        Kind = 'GitSubmodule'
        UpdateMode = 'GitSubmodule'
        Optional = $false
        TagPattern = $TagPattern
        ZipUrlTemplate = $null
        VersionReader = $null
        RemoteInfoReader = {
            param($entry)
            Get-LatestStableGitRelease -RepositoryUrl $entry.RepositoryUrl -TagPattern $entry.TagPattern
        }
    }
}

function Get-DependencyCatalog {
    $repoRoot = Get-ThirdPartyRepoRoot

    return @(
        [pscustomobject]@{
            Name = 'scintilla'
            DisplayName = 'Scintilla'
            Repository = 'https://www.scintilla.org/'
            RepositoryUrl = $null
            LocalPath = Join-Path $repoRoot 'third_party\scintilla'
            RelativePath = 'third_party\scintilla'
            ValidationPaths = @('version.txt', 'win32\scintilla.mak', 'include\Scintilla.h')
            Kind = 'ReplaceTree'
            UpdateMode = 'ReplaceTree'
            Optional = $false
            VersionReader = {
                param($entry)
                $versionFile = Join-Path $entry.LocalPath 'version.txt'
                if (-not (Test-Path -LiteralPath $versionFile)) { throw "version.txt not found: $versionFile" }
                Convert-ScintillaStyleVersion -RawVersion (Get-Content -LiteralPath $versionFile -Raw)
            }
            RemoteInfoReader = {
                param($entry)
                $content = Get-RemoteTextContent -Uri 'https://www.scintilla.org/ScintillaDownload.html'
                $releaseMatch = [regex]::Match($content, 'Release\s+(\d+\.\d+\.\d+)')
                $zipMatch = [regex]::Match($content, 'https://www\.scintilla\.org/scintilla\d+\.zip')
                if (-not $releaseMatch.Success -or -not $zipMatch.Success) { throw 'Could not parse Scintilla download page.' }
                [pscustomobject]@{ Tag=$releaseMatch.Groups[1].Value; Version=$releaseMatch.Groups[1].Value; Commit=$null; ZipUrl=$zipMatch.Value; Source='scintilla.org' }
            }
        }
        (New-GitSubmoduleDependency -Name 'lexilla' -DisplayName 'Lexilla' -RelativePath 'third_party\lexilla' -RepositoryUrl 'https://github.com/ScintillaOrg/lexilla.git' -TagPattern '^rel-(\d+)-(\d+)-(\d+)$' -ValidationPaths @('version.txt','src\lexilla.mak','include\Lexilla.h'))
        (New-GitSubmoduleDependency -Name 'pcre2' -DisplayName 'PCRE2' -RelativePath 'third_party\pcre2' -RepositoryUrl 'https://github.com/PCRE2Project/pcre2.git' -TagPattern '^pcre2-(\d+\.\d+)$' -ValidationPaths @('CMakeLists.txt','src\pcre2.h.generic'))
        (New-GitSubmoduleDependency -Name 'hunspell' -DisplayName 'Hunspell' -RelativePath 'third_party\hunspell' -RepositoryUrl 'https://github.com/hunspell/hunspell.git' -TagPattern '^v?(\d+\.\d+\.\d+)$' -ValidationPaths @('configure.ac','src\hunspell\hunspell.cxx'))
        [pscustomobject]@{
            Name = 'wtl'
            DisplayName = 'WTL'
            Repository = 'https://sourceforge.net/projects/wtl/'
            RepositoryUrl = $null
            LocalPath = Join-Path $repoRoot 'third_party\wtl'
            RelativePath = 'third_party\wtl'
            SourceSubdirectory = 'Include'
            ValidationPaths = @('atlapp.h','atlctrls.h','atlframe.h','atlres.h')
            Kind = 'ReplaceTree'
            UpdateMode = 'ReplaceTree'
            Optional = $false
            VersionReader = {
                param($entry)
                $headerFile = Join-Path $entry.LocalPath 'atlapp.h'
                if (-not (Test-Path -LiteralPath $headerFile)) { throw "atlapp.h not found: $headerFile" }
                $content = Get-Content -LiteralPath $headerFile
                $versionLine = $content | Where-Object { $_ -match '^\s*#define\s+_WTL_VER\s+0x([0-9A-Fa-f]+)' } | Select-Object -First 1
                if (-not $versionLine) { throw "Could not read WTL version from $headerFile" }
                $match = [regex]::Match($versionLine, '0x([0-9A-Fa-f]{4})')
                if (-not $match.Success) { throw "Unexpected _WTL_VER format in $headerFile" }
                $digits = $match.Groups[1].Value
                $major = [int]$digits.Substring(0,2)
                $minor = [int]$digits.Substring(2,1)
                $patch = [int]$digits.Substring(3,1)
                '{0}.{1}.{2}' -f $major,$minor,$patch
            }
            RemoteInfoReader = {
                param($entry)
                $content = Get-RemoteTextContent -Uri 'https://sourceforge.net/projects/wtl/rss?path=/WTL%2010'
                $matches = [regex]::Matches($content, 'WTL(\d{2})_(\d{2})_Release\.zip')
                if ($matches.Count -eq 0) { throw 'Could not find WTL release archive on SourceForge.' }
                $versions = foreach ($match in $matches) {
                    $major = [int]$match.Groups[1].Value
                    $patch = [int]$match.Groups[2].Value
                    $version = '{0}.0.{1}' -f $major,$patch
                    [pscustomobject]@{ Tag=$match.Value; Version=$version; Comparison=[version]$version }
                }
                $best = $versions | Sort-Object Comparison -Descending | Select-Object -First 1
                [pscustomobject]@{ Tag=$best.Tag; Version=$best.Version; Commit=$null; ZipUrl="https://sourceforge.net/projects/wtl/files/WTL%2010/$($best.Tag)/download"; Source='SourceForge' }
            }
        }

        (New-GitSubmoduleDependency -Name 'libheif' -DisplayName 'libheif' -RelativePath 'third_party\libheif' -RepositoryUrl 'https://github.com/strukturag/libheif.git' -TagPattern '^v?(\d+\.\d+\.\d+)$' -ValidationPaths @('CMakeLists.txt','libheif'))
        (New-GitSubmoduleDependency -Name 'libde265' -DisplayName 'libde265' -RelativePath 'third_party\libde265' -RepositoryUrl 'https://github.com/strukturag/libde265.git' -TagPattern '^v?(\d+\.\d+\.\d+)$' -ValidationPaths @('CMakeLists.txt','libde265'))
        (New-GitSubmoduleDependency -Name 'aom' -DisplayName 'libaom (AOM)' -RelativePath 'third_party\aom' -RepositoryUrl 'https://aomedia.googlesource.com/aom' -TagPattern '^v?(\d+\.\d+\.\d+)$' -ValidationPaths @('CMakeLists.txt','aom'))
        (New-GitSubmoduleDependency -Name 'libwebp' -DisplayName 'libwebp' -RelativePath 'third_party\libwebp' -RepositoryUrl 'https://chromium.googlesource.com/webm/libwebp' -TagPattern '^v?(\d+\.\d+\.\d+)$' -ValidationPaths @('CMakeLists.txt','src'))
        (New-GitSubmoduleDependency -Name 'openjpeg' -DisplayName 'OpenJPEG' -RelativePath 'third_party\openjpeg' -RepositoryUrl 'https://github.com/uclouvain/openjpeg.git' -TagPattern '^v?(\d+\.\d+\.\d+)$' -ValidationPaths @('CMakeLists.txt','src'))

        [pscustomobject]@{
            Name = 'lunasvg'
            DisplayName = 'LunaSVG'
            Repository = 'sammycage/lunasvg'
            RepositoryUrl = 'https://github.com/sammycage/lunasvg.git'
            LocalPath = Join-Path $repoRoot 'src\import-epub\thirdparty\lunasvg'
            RelativePath = 'src\import-epub\thirdparty\lunasvg'
            ValidationPaths = @('CMakeLists.txt','include\lunasvg.h','source\lunasvg.cpp','lunasvg.vcxproj','plutovg.vcxproj')
            Kind = 'VendoredTree'
            UpdateMode = 'Manual'
            Optional = $false
            TagPattern = '^v?(\d+\.\d+\.\d+)$'
            VersionReader = {
                param($entry)
                Get-CMakeProjectVersion -CMakeListsPath (Join-Path $entry.LocalPath 'CMakeLists.txt') -ProjectName 'lunasvg'
            }
            RemoteInfoReader = {
                param($entry)
                Get-LatestStableGitRelease -RepositoryUrl $entry.RepositoryUrl -TagPattern $entry.TagPattern -ZipUrlTemplate 'https://github.com/sammycage/lunasvg/archive/refs/tags/{0}.zip'
            }
        }
        [pscustomobject]@{
            Name = 'plutovg'
            DisplayName = 'PlutoVG (vendored by LunaSVG)'
            Repository = 'sammycage/plutovg'
            RepositoryUrl = 'https://github.com/sammycage/plutovg.git'
            LocalPath = Join-Path $repoRoot 'src\import-epub\thirdparty\lunasvg\plutovg'
            RelativePath = 'src\import-epub\thirdparty\lunasvg\plutovg'
            ValidationPaths = @('CMakeLists.txt','include\plutovg.h','source\plutovg-canvas.c')
            Kind = 'VendoredTree'
            UpdateMode = 'Manual'
            Optional = $false
            TagPattern = '^v?(\d+\.\d+\.\d+)$'
            VersionReader = {
                param($entry)
                Get-CMakeProjectVersion -CMakeListsPath (Join-Path $entry.LocalPath 'CMakeLists.txt') -ProjectName 'plutovg'
            }
            RemoteInfoReader = {
                param($entry)
                Get-LatestStableGitRelease -RepositoryUrl $entry.RepositoryUrl -TagPattern $entry.TagPattern -ZipUrlTemplate 'https://github.com/sammycage/plutovg/archive/refs/tags/{0}.zip'
            }
        }
    )
}

function Get-DependencyEntry {
    param([Parameter(Mandatory)][string]$Name)

    $normalized = $Name.ToLowerInvariant()
    if ($normalized -eq 'libaom') { $normalized = 'aom' }
    if ($normalized -eq 'open-jpeg') { $normalized = 'openjpeg' }

    $entry = Get-DependencyCatalog | Where-Object { $_.Name -eq $normalized } | Select-Object -First 1
    if (-not $entry) { throw "Unknown dependency: $Name" }
    return $entry
}

function Get-LocalDependencyVersion {
    param([Parameter(Mandatory)]$Dependency)

    if ($Dependency.Kind -eq 'GitSubmodule') {
        $commit = Get-GitLinkCommit -RelativePath $Dependency.RelativePath
        if (-not $commit) { return $null }
        $tag = Get-StableTagForCommit -RepositoryUrl $Dependency.RepositoryUrl -TagPattern $Dependency.TagPattern -Commit $commit
        if ($tag) { return $tag.Version }
        return $commit.Substring(0, [Math]::Min(12, $commit.Length))
    }

    if (-not $Dependency.VersionReader) {
        throw "No local version reader configured for $($Dependency.Name)."
    }

    return & $Dependency.VersionReader $Dependency
}

function Assert-DependencySourceTree {
    param(
        [Parameter(Mandatory)]$Dependency,
        [Parameter(Mandatory)][string]$Path
    )

    if (-not (Test-Path -LiteralPath $Path)) { throw "Source directory not found: $Path" }
    foreach ($relativePath in @($Dependency.ValidationPaths)) {
        if (-not $relativePath) { continue }
        $fullPath = Join-Path $Path $relativePath
        if (-not (Test-Path -LiteralPath $fullPath)) {
            throw "$Path does not look like $($Dependency.DisplayName): missing $relativePath"
        }
    }
}

function Get-RemoteDependencyRelease {
    param([Parameter(Mandatory)]$Dependency)
    if (-not $Dependency.RemoteInfoReader) { throw "No remote reader configured for $($Dependency.Name)." }
    return & $Dependency.RemoteInfoReader $Dependency
}

function Get-DependencyUpdateInfo {
    param([Parameter(Mandatory)]$Dependency)

    $remote = Get-RemoteDependencyRelease -Dependency $Dependency

    if ($Dependency.Kind -eq 'GitSubmodule') {
        $localCommit = Get-GitLinkCommit -RelativePath $Dependency.RelativePath
        if (-not $localCommit) {
            return [pscustomobject]@{
                Name=$Dependency.Name; DisplayName=$Dependency.DisplayName; Repository=$Dependency.Repository; LocalPath=$Dependency.LocalPath
                LocalVersion=$null; LocalCommit=$null; RemoteVersion=$remote.Version; RemoteTag=$remote.Tag; RemoteCommit=$remote.Commit
                RemoteZipUrl=$remote.ZipUrl; RemoteSource=$remote.Source; Status='NotInstalled'; Kind=$Dependency.Kind; UpdateMode=$Dependency.UpdateMode
            }
        }

        $localRelease = Get-StableTagForCommit -RepositoryUrl $Dependency.RepositoryUrl -TagPattern $Dependency.TagPattern -Commit $localCommit
        $localVersion = if ($localRelease) { $localRelease.Version } else { $localCommit.Substring(0, [Math]::Min(12,$localCommit.Length)) }

        if ($localCommit -eq $remote.Commit) {
            $status = 'UpToDate'
        }
        elseif ($localRelease) {
            $localComparison = [version]$localRelease.Version
            $remoteComparison = [version]$remote.Version
            if ($localComparison -lt $remoteComparison) { $status = 'UpdateAvailable' }
            elseif ($localComparison -gt $remoteComparison) { $status = 'LocalNewer' }
            else { $status = 'NeedsReview' }
        }
        else {
            $status = 'NeedsReview'
        }

        return [pscustomobject]@{
            Name=$Dependency.Name; DisplayName=$Dependency.DisplayName; Repository=$Dependency.Repository; LocalPath=$Dependency.LocalPath
            LocalVersion=$localVersion; LocalCommit=$localCommit; RemoteVersion=$remote.Version; RemoteTag=$remote.Tag; RemoteCommit=$remote.Commit
            RemoteZipUrl=$remote.ZipUrl; RemoteSource=$remote.Source; Status=$status; Kind=$Dependency.Kind; UpdateMode=$Dependency.UpdateMode
        }
    }

    if (-not (Test-Path -LiteralPath $Dependency.LocalPath)) {
        if ($Dependency.Optional) {
            return [pscustomobject]@{
                Name=$Dependency.Name; DisplayName=$Dependency.DisplayName; Repository=$Dependency.Repository; LocalPath=$Dependency.LocalPath
                LocalVersion=$null; LocalCommit=$null; RemoteVersion=$remote.Version; RemoteTag=$remote.Tag; RemoteCommit=$remote.Commit
                RemoteZipUrl=$remote.ZipUrl; RemoteSource=$remote.Source; Status='NotInstalled'; Kind=$Dependency.Kind; UpdateMode=$Dependency.UpdateMode
            }
        }
        throw "Local dependency directory not found: $($Dependency.LocalPath)"
    }

    $localVersion = Get-LocalDependencyVersion -Dependency $Dependency
    $localComparison = [version]$localVersion
    $remoteComparison = [version]$remote.Version

    if ($localComparison -lt $remoteComparison) { $status = 'UpdateAvailable' }
    elseif ($localComparison -eq $remoteComparison) { $status = 'UpToDate' }
    else { $status = 'LocalNewer' }

    return [pscustomobject]@{
        Name=$Dependency.Name; DisplayName=$Dependency.DisplayName; Repository=$Dependency.Repository; LocalPath=$Dependency.LocalPath
        LocalVersion=$localVersion; LocalCommit=$null; RemoteVersion=$remote.Version; RemoteTag=$remote.Tag; RemoteCommit=$remote.Commit
        RemoteZipUrl=$remote.ZipUrl; RemoteSource=$remote.Source; Status=$status; Kind=$Dependency.Kind; UpdateMode=$Dependency.UpdateMode
    }
}

function Expand-ZipArchiveToSingleRoot {
    param(
        [Parameter(Mandatory)][string]$ZipPath,
        [Parameter(Mandatory)][string]$DestinationPath
    )

    if (Test-Path -LiteralPath $DestinationPath) { Remove-Item -LiteralPath $DestinationPath -Recurse -Force }
    $stagingPath = Join-Path ([IO.Path]::GetDirectoryName($DestinationPath)) ([IO.Path]::GetRandomFileName())
    New-Item -ItemType Directory -Path $stagingPath -Force | Out-Null

    try {
        Expand-Archive -LiteralPath $ZipPath -DestinationPath $stagingPath -Force
        $roots = @(Get-ChildItem -LiteralPath $stagingPath)
        if ($roots.Count -ne 1 -or -not $roots[0].PSIsContainer) {
            throw "Expected one root directory in archive: $ZipPath"
        }
        Move-Item -LiteralPath $roots[0].FullName -Destination $DestinationPath
    }
    finally {
        Remove-Item -LiteralPath $stagingPath -Recurse -Force -ErrorAction SilentlyContinue
    }
}
