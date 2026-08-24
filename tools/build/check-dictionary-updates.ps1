<#
.SYNOPSIS
Reports available dictionary releases without changing the checkout.
#>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$manifest = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'runtime\dict\sources.json') | ConvertFrom-Json
$sources = @{
    en_US = 'https://github.com/en-wl/wordlist.git'
    ru_RU = 'https://github.com/Goudron/ru-spelling-dictionary.git'
    uk_UA = 'https://github.com/brown-uk/dict_uk.git'
}

function Get-GermanDictionaryStatus {
    $entry = $manifest.de_DE
    $tempRoot = Join-Path ([IO.Path]::GetTempPath()) "fbe-de-dictionary-$PID"
    try {
        New-Item -ItemType Directory -Path $tempRoot -Force | Out-Null
        $affPath = Join-Path $tempRoot 'de_DE_frami.aff'
        $dicPath = Join-Path $tempRoot 'de_DE_frami.dic'
        Invoke-WebRequest -UseBasicParsing -Uri 'https://raw.githubusercontent.com/LibreOffice/dictionaries/master/de/de_DE_frami.aff' -OutFile $affPath
        Invoke-WebRequest -UseBasicParsing -Uri 'https://raw.githubusercontent.com/LibreOffice/dictionaries/master/de/de_DE_frami.dic' -OutFile $dicPath
        $affHash = (Get-FileHash -LiteralPath $affPath -Algorithm SHA256).Hash
        $dicHash = (Get-FileHash -LiteralPath $dicPath -Algorithm SHA256).Hash
        $text = [Text.Encoding]::GetEncoding(28591).GetString([IO.File]::ReadAllBytes($affPath))
        $version = ([regex]::Match($text, '(?m)^# Version:\s*(?<version>\S+)')).Groups['version'].Value
        [pscustomobject]@{
            Dictionary = 'de_DE'; Installed = [string]$entry.version; Latest = $version
            Status = $(if ($affHash -eq $entry.affSha256 -and $dicHash -eq $entry.dicSha256) { 'Current' } else { 'Check manually' })
            Upstream = $entry.repository
        }
    }
    catch {
        [pscustomobject]@{ Dictionary = 'de_DE'; Installed = [string]$entry.version; Latest = ''; Status = "Error: $($_.Exception.Message)"; Upstream = $entry.repository }
    }
    finally {
        Remove-Item -LiteralPath $tempRoot -Recurse -Force -ErrorAction SilentlyContinue
    }
}

function Get-ReleaseTag([string]$Dictionary, [string[]]$Lines) {
    $tags = $Lines | ForEach-Object { ($_ -split 'refs/tags/')[-1] -replace '\^\{\}$' } | Where-Object { $_ }
    if ($Dictionary -eq 'en_US') {
        return $tags | Where-Object { $_ -match '^rel-\d{4}\.\d{2}\.\d{2}$' } |
            Sort-Object { [datetime]::ParseExact($_.Substring(4), 'yyyy.MM.dd', [Globalization.CultureInfo]::InvariantCulture) } | Select-Object -Last 1
    }
    return $tags | Where-Object { $_ -match '^v?\d+\.\d+\.\d+$' } |
        Sort-Object { [version](($_ -replace '^v', '')) } | Select-Object -Last 1
}

$results = foreach ($name in @('en_US', 'ru_RU', 'uk_UA')) {
    $installed = [string]$manifest.$name.version
    try {
        $tags = & git ls-remote --tags $sources[$name]
        if ($LASTEXITCODE -ne 0) { throw 'git ls-remote failed' }
        $latest = Get-ReleaseTag -Dictionary $name -Lines $tags
        if (-not $latest) { throw 'no stable release tag found' }
        [pscustomobject]@{ Dictionary = $name; Installed = $installed; Latest = $latest; Status = $(if ($latest -eq $manifest.$name.upstreamTag) { 'Current' } else { 'Check manually' }); Upstream = $manifest.$name.repository }
    } catch {
        [pscustomobject]@{ Dictionary = $name; Installed = $installed; Latest = ''; Status = "Error: $($_.Exception.Message)"; Upstream = $manifest.$name.repository }
    }
}
$results += Get-GermanDictionaryStatus
$results | Format-Table -AutoSize
