<# Shared SemVer 2.0 validation for release/update scripts. #>

function Test-FbeSemVer {
    param([Parameter(Mandatory)][string]$Version)
    $match = [regex]::Match($Version, '^(?<core>[^-+]+)(?:-(?<pre>[^+]+))?(?:\+(?<meta>.+))?$')
    if (-not $match.Success) { return $false }
    $core = @($match.Groups['core'].Value -split '\.')
    $preText = $match.Groups['pre'].Value
    $metaText = $match.Groups['meta'].Value
    if ($core.Count -ne 3) { return $false }
    foreach ($part in $core) { if ($part -notmatch '^(0|[1-9][0-9]*)$') { return $false } }
    if ($preText) {
        foreach ($part in ($preText -split '\.')) {
            if ($part -notmatch '^[0-9A-Za-z-]+$') { return $false }
            if ($part -match '^[0-9]+$' -and $part.Length -gt 1 -and $part[0] -eq '0') { return $false }
        }
    }
    if ($metaText) { foreach ($part in ($metaText -split '\.')) { if ($part -notmatch '^[0-9A-Za-z-]+$') { return $false } } }
    return $true
}

function Test-FbeReleaseTag {
    param([Parameter(Mandatory)][string]$ReleaseTag)
    return $ReleaseTag.Length -gt 1 -and $ReleaseTag[0] -eq 'v' -and (Test-FbeSemVer $ReleaseTag.Substring(1))
}

function Get-FbeBaseVersion {
    param([Parameter(Mandatory)][string]$Version)
    if (-not (Test-FbeSemVer $Version)) { return $null }
    return ($Version -split '[-+]', 2)[0]
}

function Test-FbePrereleaseVersion {
    param([Parameter(Mandatory)][string]$Version)
    return (Test-FbeSemVer $Version) -and $Version -match '-[^+]+(?:\+|$)'
}
