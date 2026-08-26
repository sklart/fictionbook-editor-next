[CmdletBinding()]
param(
    [ValidateSet('Win32')][string]$Platform = 'Win32',
    [string]$ArtifactsDirectory,
    [switch]$SkipInstaller
)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
if (-not $ArtifactsDirectory) { $ArtifactsDirectory = Join-Path $repoRoot 'out\artifacts' }
$ArtifactsDirectory = (Resolve-Path -LiteralPath $ArtifactsDirectory).Path
$versionText = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\version.h')
$match = [regex]::Match($versionText, 'FBE_VERSION_STRING\s+"(?<version>\d+\.\d+\.\d+)"')
if (-not $match.Success) { throw 'Не найден FBE_VERSION_STRING.' }
$version = $match.Groups['version'].Value
$architecture = $Platform.ToLowerInvariant()
$expected = @(
    "FictionBookEditorNext-$version-$architecture-setup.exe",
    "FictionBookEditorNext-$version-$architecture-portable.zip",
    "FictionBookEditorNext-$version-$architecture-symbols.zip",
    'SHA256SUMS.txt'
)
$actual = @(Get-ChildItem -LiteralPath $ArtifactsDirectory -File | Select-Object -ExpandProperty Name | Sort-Object)
if (($actual -join '|') -ne (($expected | Sort-Object) -join '|')) {
    throw "Ожидается ровно один unified release: $($expected -join ', '). Получено: $($actual -join ', ')."
}
if ($actual | Where-Object { $_ -match '-win7-' }) { throw 'Unified release не должен содержать -win7- artifacts.' }
$checksums = @{}
foreach ($line in Get-Content -LiteralPath (Join-Path $ArtifactsDirectory 'SHA256SUMS.txt')) {
    if ($line -match '^(?<hash>[0-9A-Fa-f]{64})\s\s(?<name>.+)$') { $checksums[$Matches.name] = $Matches.hash }
}
foreach ($name in $expected | Where-Object { $_ -ne 'SHA256SUMS.txt' }) {
    if ($checksums[$name] -ne (Get-FileHash -LiteralPath (Join-Path $ArtifactsDirectory $name) -Algorithm SHA256).Hash) {
        throw "SHA256SUMS.txt не соответствует $name."
    }
}
Write-Host "Unified release artifacts verified: $ArtifactsDirectory"
