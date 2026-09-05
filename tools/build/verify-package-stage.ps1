[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidateSet('Core','Integration','Portable')][string]$Kind,
    [Parameter(Mandatory)][string]$StageDirectory
)
$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$stage = (Resolve-Path -LiteralPath $StageDirectory).Path
$manifest = Get-Content -LiteralPath (Join-Path $repoRoot 'packaging\package-manifest.json') -Raw | ConvertFrom-Json
$section = $manifest.($Kind.ToLowerInvariant())
$required = if ($null -ne $section.PSObject.Properties['required']) { @($section.required) } else { @() }
$forbidden = if ($null -ne $section.PSObject.Properties['forbidden']) { @($section.forbidden) } else { @() }
foreach ($name in @($required | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })) { if (-not (Test-Path -LiteralPath (Join-Path $stage $name))) { throw "$Kind stage misses required item: $name" } }
foreach ($name in @($forbidden | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })) { if (Test-Path -LiteralPath (Join-Path $stage $name)) { throw "$Kind stage contains forbidden item: $name" } }
if ($Kind -eq 'Portable') {
    foreach ($name in @($manifest.core.required)) {
        if (-not (Test-Path -LiteralPath (Join-Path $stage $name) -PathType Leaf)) { throw "Portable stage misses Core file: $name" }
    }
}
Write-Host "$Kind stage verification passed: $stage"
