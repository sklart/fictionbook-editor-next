<# Resolves the single supported NSIS compiler contract for local and CI builds. #>
[CmdletBinding()]
param(
    [string]$MakensisPath
)

$ErrorActionPreference = 'Stop'
$minimumVersion = [version]'3.12'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path

$candidates = @(
    $MakensisPath,
    $env:FBE_MAKENSIS,
    (Join-Path ${env:ProgramFiles(x86)} 'NSIS\makensis.exe'),
    (Join-Path $env:ProgramFiles 'NSIS\makensis.exe')
) | Where-Object { -not [string]::IsNullOrWhiteSpace($_) }

$makensis = $candidates | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } | Select-Object -First 1
if (-not $makensis) {
    throw "Не найден makensis.exe. Установите NSIS $minimumVersion или задайте FBE_MAKENSIS."
}
$makensis = (Resolve-Path -LiteralPath $makensis).Path
$versionText = (& $makensis /VERSION | Out-String).Trim()
$versionMatch = [regex]::Match($versionText, '(?<version>\d+\.\d+(?:\.\d+)?)')
if (-not $versionMatch.Success -or [version]$versionMatch.Groups['version'].Value -lt $minimumVersion) {
    throw "Требуется NSIS $minimumVersion или новее; '$makensis' сообщает '$versionText'."
}

$uacPlugin = Join-Path $repoRoot 'packaging\nsis\NSIS\Plugins\x86-unicode\UAC.dll'
if (-not (Test-Path -LiteralPath $uacPlugin -PathType Leaf)) {
    throw "Не найден обязательный Unicode UAC plugin: $uacPlugin"
}

Write-Output $makensis
