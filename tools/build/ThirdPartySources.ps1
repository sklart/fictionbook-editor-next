<#
.SYNOPSIS
Общие helper-функции для проверки, скачивания и раскладки исходников сторонних зависимостей.
#>

[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"

function Get-ThirdPartyText {
    param(
        [Parameter(Mandatory)]
        [string]$Base64
    )

    return [Text.Encoding]::UTF8.GetString([Convert]::FromBase64String($Base64))
}

function Format-ThirdPartyText {
    param(
        [Parameter(Mandatory)]
        [string]$Base64,

        [Parameter(ValueFromRemainingArguments = $true)]
        [object[]]$Arguments
    )

    $text = Get-ThirdPartyText -Base64 $Base64

    if (-not $Arguments -or $Arguments.Count -eq 0) {
        return $text
    }

    switch ($Arguments.Count) {
        1 { return $text -f $Arguments[0] }
        2 { return $text -f $Arguments[0], $Arguments[1] }
        3 { return $text -f $Arguments[0], $Arguments[1], $Arguments[2] }
        default { return $text -f $Arguments }
    }
}

function Get-DependencyStatusDisplay {
    param(
        [Parameter(Mandatory)]
        [string]$Status
    )

    switch ($Status) {
        "UpdateAvailable" { return Get-ThirdPartyText -Base64 "0J7QsdC90L7QstC70LXQvdC40LUg0LTQvtGB0YLRg9C/0L3Qvg==" }
        "UpToDate" { return Get-ThirdPartyText -Base64 "0JDQutGC0YPQsNC70YzQvdC+" }
        "LocalNewer" { return Get-ThirdPartyText -Base64 "0JvQvtC60LDQu9GM0L3QsNGPINCy0LXRgNGB0LjRjyDQvdC+0LLQtdC1" }
        "Error" { return Get-ThirdPartyText -Base64 "0J7RiNC40LHQutCw" }
        default { return $Status }
    }
}

function Get-ThirdPartyRepoRoot {
    return (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
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
    }
}

function Get-RemoteTextContent {
    param(
        [Parameter(Mandatory)]
        [string]$Uri
    )

    Enable-ModernTls

    $client = New-Object System.Net.WebClient
    $client.Headers["User-Agent"] = "Mozilla/5.0 (compatible; FBeditor-third-party-update-check)"

    try {
        return $client.DownloadString($Uri)
    }
    finally {
        $client.Dispose()
    }
}

function Convert-ScintillaStyleVersion {
    param(
        [Parameter(Mandatory)]
        [string]$RawVersion
    )

    $trimmed = $RawVersion.Trim()
    if ($trimmed -notmatch '^\d{3,}$') {
        throw "Neozhidannyj format versii Scintilla/Lexilla: $RawVersion"
    }

    if ($trimmed.Length -eq 3) {
        return "{0}.{1}.{2}" -f $trimmed.Substring(0, 1), $trimmed.Substring(1, 1), $trimmed.Substring(2, 1)
    }

    return "{0}.{1}.{2}" -f $trimmed.Substring(0, 1), $trimmed.Substring(1, 1), $trimmed.Substring(2)
}

function Get-DependencyCatalog {
    $repoRoot = Get-ThirdPartyRepoRoot

    return @(
        [pscustomobject]@{
            Name = "scintilla"
            DisplayName = "Scintilla"
            Repository = "https://www.scintilla.org/"
            LocalPath = Join-Path $repoRoot "third_party\scintilla"
            ValidationPaths = @(
                "version.txt",
                "win32\scintilla.mak",
                "include\Scintilla.h"
            )
            VersionReader = {
                param($entry)
                $versionFile = Join-Path $entry.LocalPath "version.txt"
                if (-not (Test-Path -LiteralPath $versionFile)) {
                    throw "Ne naiden version.txt: $versionFile"
                }

                $raw = Get-Content -LiteralPath $versionFile -Raw
                return Convert-ScintillaStyleVersion -RawVersion $raw
            }
            RemoteInfoReader = {
                param($entry)
                $content = Get-RemoteTextContent -Uri "https://www.scintilla.org/ScintillaDownload.html"

                $releaseMatch = [regex]::Match($content, 'Release\s+(\d+\.\d+\.\d+)')
                $zipMatch = [regex]::Match($content, 'https://www\.scintilla\.org/scintilla\d+\.zip')

                if (-not $releaseMatch.Success -or -not $zipMatch.Success) {
                    throw "Ne udalos razobrat stranicu zagruzki Scintilla."
                }

                return [pscustomobject]@{
                    Tag = $releaseMatch.Groups[1].Value
                    Version = $releaseMatch.Groups[1].Value
                    ZipUrl = $zipMatch.Value
                    Source = "scintilla.org"
                }
            }
        }
        [pscustomobject]@{
            Name = "lexilla"
            DisplayName = "Lexilla"
            Repository = "ScintillaOrg/lexilla"
            LocalPath = Join-Path $repoRoot "third_party\lexilla"
            ValidationPaths = @(
                "version.txt",
                "src\lexilla.mak",
                "include\Lexilla.h"
            )
            VersionReader = {
                param($entry)
                $versionFile = Join-Path $entry.LocalPath "version.txt"
                if (-not (Test-Path -LiteralPath $versionFile)) {
                    throw "Ne naiden version.txt: $versionFile"
                }

                $raw = Get-Content -LiteralPath $versionFile -Raw
                return Convert-ScintillaStyleVersion -RawVersion $raw
            }
            RemoteInfoReader = {
                param($entry)
                $content = Get-RemoteTextContent -Uri "https://www.scintilla.org/LexillaDownload.html"

                $releaseMatch = [regex]::Match($content, 'Release\s+(\d+\.\d+\.\d+)')
                $zipMatch = [regex]::Match($content, 'https://www\.scintilla\.org/lexilla\d+\.zip')

                if (-not $releaseMatch.Success -or -not $zipMatch.Success) {
                    throw "Ne udalos razobrat stranicu zagruzki Lexilla."
                }

                return [pscustomobject]@{
                    Tag = $releaseMatch.Groups[1].Value
                    Version = $releaseMatch.Groups[1].Value
                    ZipUrl = $zipMatch.Value
                    Source = "scintilla.org"
                }
            }
        }
        [pscustomobject]@{
            Name = "pcre2"
            DisplayName = "PCRE2"
            Repository = "PCRE2Project/pcre2"
            LocalPath = Join-Path $repoRoot "third_party\pcre2"
            ValidationPaths = @(
                "CMakeLists.txt",
                "src\pcre2.h.generic",
                "src\config.h.generic"
            )
            VersionReader = {
                param($entry)
                $headerFile = Join-Path $entry.LocalPath "src\pcre2.h.generic"
                if (-not (Test-Path -LiteralPath $headerFile)) {
                    throw "Ne naiden pcre2.h.generic: $headerFile"
                }

                $content = Get-Content -LiteralPath $headerFile
                $majorLine = $content | Where-Object { $_ -match '^\s*#define\s+PCRE2_MAJOR\s+\d+' } | Select-Object -First 1
                $minorLine = $content | Where-Object { $_ -match '^\s*#define\s+PCRE2_MINOR\s+\d+' } | Select-Object -First 1

                if (-not $majorLine -or -not $minorLine) {
                    throw "Ne udalos razobrat versiyu PCRE2 iz $headerFile"
                }

                $major = [int]($majorLine -replace '^\s*#define\s+PCRE2_MAJOR\s+', '')
                $minor = [int]($minorLine -replace '^\s*#define\s+PCRE2_MINOR\s+', '')
                return "{0}.{1}" -f $major, $minor
            }
            RemoteInfoReader = {
                param($entry)
                $lines = & git ls-remote --tags "https://github.com/PCRE2Project/pcre2.git"
                if ($LASTEXITCODE -ne 0) {
                    throw "git ls-remote ne smog poluchit spisok tagov PCRE2."
                }

                $versions = foreach ($line in $lines) {
                    if ($line -match 'refs/tags/pcre2-(\d+\.\d+)$') {
                        [pscustomobject]@{
                            Tag = "pcre2-$($Matches[1])"
                            Version = $Matches[1]
                            Comparison = [version]$Matches[1]
                        }
                    }
                }

                $best = $versions | Sort-Object Comparison -Descending | Select-Object -First 1
                if (-not $best) {
                    throw "Ne udalos najti release-tegi PCRE2 v udalyonnom repozitorii."
                }

                return [pscustomobject]@{
                    Tag = $best.Tag
                    Version = $best.Version
                    ZipUrl = "https://github.com/PCRE2Project/pcre2/archive/refs/tags/$($best.Tag).zip"
                    Source = "git ls-remote"
                }
            }
        }
        [pscustomobject]@{
            Name = "hunspell"
            DisplayName = "Hunspell"
            Repository = "hunspell/hunspell"
            LocalPath = Join-Path $repoRoot "third_party\hunspell"
            ValidationPaths = @(
                "configure.ac",
                "README",
                "src\hunspell\hunspell.cxx"
            )
            VersionReader = {
                param($entry)
                $configureAc = Join-Path $entry.LocalPath "configure.ac"
                if (-not (Test-Path -LiteralPath $configureAc)) {
                    throw "Не найден configure.ac: $configureAc"
                }

                $content = Get-Content -LiteralPath $configureAc
                $versionLine = $content | Where-Object { $_ -match 'AC_INIT\(\[hunspell\],\[(\d+\.\d+\.\d+)\]' } | Select-Object -First 1
                if (-not $versionLine) {
                    throw "Не удалось разобрать версию Hunspell из $configureAc"
                }

                $match = [regex]::Match($versionLine, 'AC_INIT\(\[hunspell\],\[(\d+\.\d+\.\d+)\]')
                return $match.Groups[1].Value
            }
            RemoteInfoReader = {
                param($entry)
                $lines = & git ls-remote --tags "https://github.com/hunspell/hunspell.git"
                if ($LASTEXITCODE -ne 0) {
                    throw "git ls-remote не смог получить список тегов Hunspell."
                }

                $versions = foreach ($line in $lines) {
                    if ($line -match 'refs/tags/(?:v)?(\d+\.\d+\.\d+)$') {
                        [pscustomobject]@{
                            Tag = $Matches[0].Split('/')[-1]
                            Version = $Matches[1]
                            Comparison = [version]$Matches[1]
                        }
                    }
                }

                $best = $versions | Sort-Object Comparison -Descending | Select-Object -First 1
                if (-not $best) {
                    throw "Не удалось найти release-теги Hunspell в удалённом репозитории."
                }

                return [pscustomobject]@{
                    Tag = $best.Tag
                    Version = $best.Version
                    ZipUrl = "https://github.com/hunspell/hunspell/archive/refs/tags/$($best.Tag).zip"
                    Source = "git ls-remote"
                }
            }
        }
        [pscustomobject]@{
            Name = "wtl"
            DisplayName = "WTL"
            Repository = "https://sourceforge.net/projects/wtl/"
            LocalPath = Join-Path $repoRoot "third_party\wtl"
            SourceSubdirectory = "Include"
            ValidationPaths = @(
                "atlapp.h",
                "atlctrls.h",
                "atlframe.h",
                "atlres.h"
            )
            VersionReader = {
                param($entry)
                $headerFile = Join-Path $entry.LocalPath "atlapp.h"
                if (-not (Test-Path -LiteralPath $headerFile)) {
                    throw "Не найден atlapp.h: $headerFile"
                }

                $content = Get-Content -LiteralPath $headerFile
                $versionLine = $content | Where-Object { $_ -match '^\s*#define\s+_WTL_VER\s+0x([0-9A-Fa-f]+)' } | Select-Object -First 1
                if (-not $versionLine) {
                    throw "Не удалось разобрать версию WTL из $headerFile"
                }

                $match = [regex]::Match($versionLine, '0x([0-9A-Fa-f]{4})')
                if (-not $match.Success) {
                    throw "Неожиданный формат _WTL_VER в $headerFile"
                }

                $digits = $match.Groups[1].Value
                $major = [int]$digits.Substring(0, 2)
                $minor = [int]$digits.Substring(2, 1)
                $patch = [int]$digits.Substring(3, 1)
                return "{0}.{1}.{2}" -f $major, $minor, $patch
            }
            RemoteInfoReader = {
                param($entry)
                $content = Get-RemoteTextContent -Uri "https://sourceforge.net/projects/wtl/rss?path=/WTL%2010"

                $matches = [regex]::Matches($content, 'WTL(\d{2})_(\d{2})_Release\.zip')
                if ($matches.Count -eq 0) {
                    throw "Не удалось найти архив WTL на странице SourceForge."
                }

                $versions = foreach ($match in $matches) {
                    $major = [int]$match.Groups[1].Value
                    $patch = [int]$match.Groups[2].Value
                    $version = "{0}.0.{1}" -f $major, $patch
                    [pscustomobject]@{
                        Tag = $match.Value
                        Version = $version
                        Comparison = [version]$version
                    }
                }

                $best = $versions | Sort-Object Comparison -Descending | Select-Object -First 1
                return [pscustomobject]@{
                    Tag = $best.Tag
                    Version = $best.Version
                    ZipUrl = "https://sourceforge.net/projects/wtl/files/WTL%2010/$($best.Tag)/download"
                    Source = "SourceForge"
                }
            }
        }
    )
}

function Get-DependencyEntry {
    param(
        [Parameter(Mandatory)]
        [string]$Name
    )

    $entry = Get-DependencyCatalog | Where-Object { $_.Name -eq $Name } | Select-Object -First 1
    if (-not $entry) {
        throw "Neizvestnaya zavisimost: $Name"
    }

    return $entry
}

function Get-LocalDependencyVersion {
    param(
        [Parameter(Mandatory)]
        $Dependency
    )

    return & $Dependency.VersionReader $Dependency
}

function Assert-DependencySourceTree {
    param(
        [Parameter(Mandatory)]
        $Dependency,

        [Parameter(Mandatory)]
        [string]$Path
    )

    if (-not (Test-Path -LiteralPath $Path)) {
        throw "Ne naiden katalog ishodnikov: $Path"
    }

    foreach ($relativePath in $Dependency.ValidationPaths) {
        $fullPath = Join-Path $Path $relativePath
        if (-not (Test-Path -LiteralPath $fullPath)) {
            throw "Katalog $Path ne pohozh na $($Dependency.DisplayName): otsutstvuet $relativePath"
        }
    }
}

function Get-RemoteDependencyRelease {
    param(
        [Parameter(Mandatory)]
        $Dependency
    )

    if (-not $Dependency.RemoteInfoReader) {
        throw "Dlya zavisimosti $($Dependency.Name) ne nastroen reader udalyonnoj versii."
    }

    return & $Dependency.RemoteInfoReader $Dependency
}

function Get-DependencyUpdateInfo {
    param(
        [Parameter(Mandatory)]
        $Dependency
    )

    $localVersion = Get-LocalDependencyVersion -Dependency $Dependency
    $remote = Get-RemoteDependencyRelease -Dependency $Dependency

    $comparison = [version]$localVersion
    $remoteComparison = [version]$remote.Version

    $status = if ($comparison -lt $remoteComparison) {
        "UpdateAvailable"
    }
    elseif ($comparison -eq $remoteComparison) {
        "UpToDate"
    }
    else {
        "LocalNewer"
    }

    return [pscustomobject]@{
        Name = $Dependency.Name
        DisplayName = $Dependency.DisplayName
        Repository = $Dependency.Repository
        LocalPath = $Dependency.LocalPath
        LocalVersion = $localVersion
        RemoteVersion = $remote.Version
        RemoteTag = $remote.Tag
        RemoteZipUrl = $remote.ZipUrl
        RemoteSource = $remote.Source
        Status = $status
    }
}

function Expand-ZipArchiveToSingleRoot {
    param(
        [Parameter(Mandatory)]
        [string]$ZipPath,

        [Parameter(Mandatory)]
        [string]$DestinationPath
    )

    if (Test-Path -LiteralPath $DestinationPath) {
        Remove-Item -LiteralPath $DestinationPath -Recurse -Force
    }

    New-Item -ItemType Directory -Path $DestinationPath -Force | Out-Null

    $stagingPath = Join-Path ([IO.Path]::GetDirectoryName($DestinationPath)) ([IO.Path]::GetRandomFileName())
    try {
        Expand-Archive -LiteralPath $ZipPath -DestinationPath $stagingPath -Force
        $roots = Get-ChildItem -LiteralPath $stagingPath
        if ($roots.Count -ne 1 -or -not $roots[0].PSIsContainer) {
            throw "Ozhidalsya odin kornevoj katalog posle raspakovki arhiva: $ZipPath"
        }

        Move-Item -LiteralPath $roots[0].FullName -Destination $DestinationPath
    }
    finally {
        Remove-Item -LiteralPath $stagingPath -Recurse -Force -ErrorAction SilentlyContinue
    }
}
