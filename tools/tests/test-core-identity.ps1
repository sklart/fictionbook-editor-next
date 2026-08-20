<#
.SYNOPSIS
Сверяет байтовую идентичность Core payload с его materialized copy.

.DESCRIPTION
Portable и input установщика могут добавлять свои файлы, но ни один artifact
из package-manifest.json/core.required не должен быть заменён или изменён.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$CoreDirectory,
    [Parameter(Mandatory)][string]$CandidateDirectory,
    [string]$CandidateName = 'candidate'
)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$core = (Resolve-Path -LiteralPath $CoreDirectory).Path
$candidate = (Resolve-Path -LiteralPath $CandidateDirectory).Path
$manifest = Get-Content -LiteralPath (Join-Path $root 'packaging\package-manifest.json') -Raw | ConvertFrom-Json
foreach ($relativePath in @($manifest.core.required)) {
    $source = Join-Path $core $relativePath
    $actual = Join-Path $candidate $relativePath
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) { throw "Core identity: отсутствует исходный файл $source" }
    if (-not (Test-Path -LiteralPath $actual -PathType Leaf)) { throw "Core identity: в $CandidateName отсутствует $relativePath" }
    $sourceHash = (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash
    $actualHash = (Get-FileHash -LiteralPath $actual -Algorithm SHA256).Hash
    if ($sourceHash -ne $actualHash) { throw "Core identity: $relativePath отличается в $CandidateName" }
}
Write-Host "Core identity passed: $CandidateName"
