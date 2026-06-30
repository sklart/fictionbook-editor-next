[CmdletBinding()]
param(
    [string]$ArtifactsDirectory = ""
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
if ([string]::IsNullOrWhiteSpace($ArtifactsDirectory)) {
    $ArtifactsDirectory = Join-Path $repoRoot "out\artifacts"
}

$ArtifactsDirectory = (Resolve-Path -LiteralPath $ArtifactsDirectory).Path
$checksumsPath = Join-Path $ArtifactsDirectory "SHA256SUMS.txt"

$artifactFiles = Get-ChildItem -LiteralPath $ArtifactsDirectory -File |
    Where-Object { $_.Name -ne "SHA256SUMS.txt" } |
    Sort-Object Name

$checksumLines = foreach ($file in $artifactFiles) {
    $hash = Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256
    "{0}  {1}" -f $hash.Hash, $file.Name
}

[IO.File]::WriteAllLines($checksumsPath, $checksumLines, [Text.Encoding]::ASCII)
Write-Host "Контрольные суммы артефактов обновлены: $checksumsPath"
