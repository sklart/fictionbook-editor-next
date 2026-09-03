<#
.SYNOPSIS
Dictionary update-check helpers shared by check-third-party-updates.ps1 and check-dictionary-updates.ps1.
#>

[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'

if (-not (Get-Command Get-ThirdPartyRepoRoot -ErrorAction SilentlyContinue)) {
    . (Join-Path $PSScriptRoot 'ThirdPartySources.ps1')
}

function Get-DictionaryManifest {
    $repoRoot = Get-ThirdPartyRepoRoot
    $path = Join-Path $repoRoot 'runtime\dict\sources.json'
    if (-not (Test-Path -LiteralPath $path)) { throw "Dictionary manifest not found: $path" }
    return Get-Content -Raw -LiteralPath $path | ConvertFrom-Json
}

function Get-DictionaryCatalog {
    $repoRoot = Get-ThirdPartyRepoRoot
    $manifest = Get-DictionaryManifest

    return @(
        [pscustomobject]@{ Name='dict-en_US'; Dictionary='en_US'; DisplayName='Dictionary en_US'; Repository=[string]$manifest.en_US.repository; RepositoryUrl='https://github.com/en-wl/wordlist.git'; LocalPath=(Join-Path $repoRoot 'runtime\dict'); Kind='Dictionary'; UpdateMode='Manual'; TagPattern='^rel-(\d{4}\.\d{2}\.\d{2})$' }
        [pscustomobject]@{ Name='dict-ru_RU'; Dictionary='ru_RU'; DisplayName='Dictionary ru_RU'; Repository=[string]$manifest.ru_RU.repository; RepositoryUrl='https://github.com/Goudron/ru-spelling-dictionary.git'; LocalPath=(Join-Path $repoRoot 'runtime\dict'); Kind='Dictionary'; UpdateMode='Manual'; TagPattern='^v?(\d+\.\d+\.\d+)$' }
        [pscustomobject]@{ Name='dict-uk_UA'; Dictionary='uk_UA'; DisplayName='Dictionary uk_UA'; Repository=[string]$manifest.uk_UA.repository; RepositoryUrl='https://github.com/brown-uk/dict_uk.git'; LocalPath=(Join-Path $repoRoot 'runtime\dict'); Kind='Dictionary'; UpdateMode='Manual'; TagPattern='^v?(\d+\.\d+\.\d+)$' }
        [pscustomobject]@{ Name='dict-de_DE'; Dictionary='de_DE'; DisplayName='Dictionary de_DE'; Repository=[string]$manifest.de_DE.repository; RepositoryUrl='https://github.com/LibreOffice/dictionaries.git'; LocalPath=(Join-Path $repoRoot 'runtime\dict'); Kind='Dictionary'; UpdateMode='Manual'; TagPattern=$null }
    )
}

function Get-DictionaryEntry {
    param([Parameter(Mandatory)][string]$Name)

    $normalized = $Name.ToLowerInvariant()
    $aliases = @{
        'en_us'='dict-en_US'; 'dict-en_us'='dict-en_US'
        'ru_ru'='dict-ru_RU'; 'dict-ru_ru'='dict-ru_RU'
        'uk_ua'='dict-uk_UA'; 'dict-uk_ua'='dict-uk_UA'
        'de_de'='dict-de_DE'; 'dict-de_de'='dict-de_DE'
    }

    if ($aliases.ContainsKey($normalized)) { $wanted = $aliases[$normalized] }
    else { $wanted = $Name }

    $entry = Get-DictionaryCatalog | Where-Object { $_.Name -ieq $wanted } | Select-Object -First 1
    if (-not $entry) { throw "Unknown dictionary: $Name" }
    return $entry
}

function Get-DictionaryUpdateInfo {
    param([Parameter(Mandatory)]$DictionaryEntry)

    $manifest = Get-DictionaryManifest
    $name = $DictionaryEntry.Dictionary
    $installed = [string]$manifest.$name.version

    if ($name -eq 'de_DE') {
        $entry = $manifest.de_DE
        $tempRoot = Join-Path ([IO.Path]::GetTempPath()) "fbe-de-dictionary-$PID-$([Guid]::NewGuid().ToString('N'))"
        try {
            New-Item -ItemType Directory -Path $tempRoot -Force | Out-Null
            $affPath = Join-Path $tempRoot 'de_DE_frami.aff'
            $dicPath = Join-Path $tempRoot 'de_DE_frami.dic'
            Enable-ModernTls
            Invoke-WebRequest -UseBasicParsing -Uri 'https://raw.githubusercontent.com/LibreOffice/dictionaries/master/de/de_DE_frami.aff' -OutFile $affPath
            Invoke-WebRequest -UseBasicParsing -Uri 'https://raw.githubusercontent.com/LibreOffice/dictionaries/master/de/de_DE_frami.dic' -OutFile $dicPath

            $affHash = (Get-FileHash -LiteralPath $affPath -Algorithm SHA256).Hash
            $dicHash = (Get-FileHash -LiteralPath $dicPath -Algorithm SHA256).Hash
            $text = [Text.Encoding]::GetEncoding(28591).GetString([IO.File]::ReadAllBytes($affPath))
            $versionMatch = [regex]::Match($text, '(?m)^# Version:\s*(?<version>\S+)')
            $remoteVersion = if ($versionMatch.Success) { $versionMatch.Groups['version'].Value } else { 'unknown' }
            $status = if ($affHash -eq [string]$entry.affSha256 -and $dicHash -eq [string]$entry.dicSha256) { 'UpToDate' } else { 'UpdateAvailable' }

            return [pscustomobject]@{
                Name=$DictionaryEntry.Name; DisplayName=$DictionaryEntry.DisplayName; Repository=$DictionaryEntry.Repository; LocalPath=$DictionaryEntry.LocalPath
                LocalVersion=$installed; LocalCommit=[string]$entry.upstreamCommit; RemoteVersion=$remoteVersion; RemoteTag='master'; RemoteCommit=$null
                RemoteZipUrl=$null; RemoteSource='LibreOffice master + SHA256'; Status=$status; Kind='Dictionary'; UpdateMode='Manual'
            }
        }
        finally {
            Remove-Item -LiteralPath $tempRoot -Recurse -Force -ErrorAction SilentlyContinue
        }
    }

    $latest = Get-LatestStableGitRelease -RepositoryUrl $DictionaryEntry.RepositoryUrl -TagPattern $DictionaryEntry.TagPattern
    $currentTag = [string]$manifest.$name.upstreamTag
    $status = if ($currentTag -eq $latest.Tag) { 'UpToDate' } else { 'UpdateAvailable' }

    return [pscustomobject]@{
        Name=$DictionaryEntry.Name; DisplayName=$DictionaryEntry.DisplayName; Repository=$DictionaryEntry.Repository; LocalPath=$DictionaryEntry.LocalPath
        LocalVersion=$installed; LocalCommit=[string]$manifest.$name.upstreamCommit; RemoteVersion=$latest.Version; RemoteTag=$latest.Tag; RemoteCommit=$latest.Commit
        RemoteZipUrl=$null; RemoteSource=$latest.Source; Status=$status; Kind='Dictionary'; UpdateMode='Manual'
    }
}
