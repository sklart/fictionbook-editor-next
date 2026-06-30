[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$nsisPath = Join-Path $repoRoot "packaging\nsis\Installer\MakeInstaller.nsi"
$installerSmokePath = Join-Path $repoRoot "tools\tests\test-fbe-specific-installer.ps1"
$propertyCheckPath = Join-Path $repoRoot "tools\tests\check-fbe-specific-properties.ps1"

function Get-RequiredFileText {
    param(
        [Parameter(Mandatory)]
        [string]$Path
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Не найден обязательный файл: $Path"
    }

    return Get-Content -LiteralPath $Path -Raw
}

function Get-NsisShellStringMap {
    param(
        [Parameter(Mandatory)]
        [string]$Text
    )

    $result = [ordered]@{}
    $pattern = '!define\s+FB2_(INFOTIP|TILEINFO|DETAILS|PREVIEWDETAILS)_PROPERTIES\s+"([^"]*)"'

    foreach ($match in [regex]::Matches($Text, $pattern)) {
        $name = switch ($match.Groups[1].Value) {
            "INFOTIP" { "InfoTip" }
            "TILEINFO" { "TileInfo" }
            "DETAILS" { "Details" }
            "PREVIEWDETAILS" { "PreviewDetails" }
            default { throw "Неожиданный ключ NSIS: $($match.Groups[1].Value)" }
        }
        $result[$name] = $match.Groups[2].Value
    }

    return $result
}

function Get-PowershellShellStringMap {
    param(
        [Parameter(Mandatory)]
        [string]$Text,
        [Parameter(Mandatory)]
        [string]$SourceLabel
    )

    $blockMatch = [regex]::Match(
        $Text,
        '\$expectedFb2ShellStrings\s*=\s*\[ordered\]@\{(?<body>.*?)\r?\n\}',
        [System.Text.RegularExpressions.RegexOptions]::Singleline
    )
    if (-not $blockMatch.Success) {
        throw "В $SourceLabel не найден блок `$expectedFb2ShellStrings."
    }

    $result = [ordered]@{}
    $entryPattern = '^\s*(InfoTip|TileInfo|Details|PreviewDetails)\s*=\s*"([^"]*)"' 
    foreach ($line in ($blockMatch.Groups["body"].Value -split "`r?`n")) {
        $entryMatch = [regex]::Match($line, $entryPattern)
        if ($entryMatch.Success) {
            $result[$entryMatch.Groups[1].Value] = $entryMatch.Groups[2].Value
        }
    }

    return $result
}

function Assert-SameShellStrings {
    param(
        [Parameter(Mandatory)]
        [hashtable]$ReferenceMap,
        [Parameter(Mandatory)]
        [hashtable]$CandidateMap,
        [Parameter(Mandatory)]
        [string]$CandidateLabel
    )

    foreach ($key in $ReferenceMap.Keys) {
        if (-not $CandidateMap.Contains($key)) {
            throw "${CandidateLabel}: отсутствует значение $key."
        }

        if ($CandidateMap[$key] -ne $ReferenceMap[$key]) {
            throw "${CandidateLabel}: значение $key не совпадает.`nОжидалось: $($ReferenceMap[$key])`nФактически: $($CandidateMap[$key])"
        }
    }
}

$nsisText = Get-RequiredFileText -Path $nsisPath
$installerSmokeText = Get-RequiredFileText -Path $installerSmokePath
$propertyCheckText = Get-RequiredFileText -Path $propertyCheckPath

$nsisShellStrings = Get-NsisShellStringMap -Text $nsisText
$installerSmokeShellStrings = Get-PowershellShellStringMap -Text $installerSmokeText -SourceLabel $installerSmokePath
$propertyCheckShellStrings = Get-PowershellShellStringMap -Text $propertyCheckText -SourceLabel $propertyCheckPath

Assert-SameShellStrings -ReferenceMap $nsisShellStrings -CandidateMap $installerSmokeShellStrings -CandidateLabel "test-fbe-specific-installer.ps1"
Assert-SameShellStrings -ReferenceMap $nsisShellStrings -CandidateMap $propertyCheckShellStrings -CandidateLabel "check-fbe-specific-properties.ps1"

Write-Host "Проверка согласованности shell-строк .fb2 прошла успешно."
foreach ($entry in $nsisShellStrings.GetEnumerator()) {
    Write-Host ("  {0}: {1}" -f $entry.Key, $entry.Value)
}
