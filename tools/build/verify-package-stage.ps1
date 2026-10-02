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

function Test-ExactStagePath {
    param(
        [Parameter(Mandatory)][string]$Root,
        [Parameter(Mandatory)][string]$RelativePath
    )

    $current = $Root
    foreach ($segment in ($RelativePath -split '[\\/]')) {
        if ([string]::IsNullOrWhiteSpace($segment)) { continue }
        $item = Get-ChildItem -LiteralPath $current -Force | Where-Object { $_.Name -ceq $segment } | Select-Object -First 1
        if ($null -eq $item) { return $false }
        $current = $item.FullName
    }

    return $true
}
foreach ($name in @($required | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })) { if (-not (Test-Path -LiteralPath (Join-Path $stage $name))) { throw "$Kind stage misses required item: $name" } }
foreach ($name in @($forbidden | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })) { if (Test-ExactStagePath -Root $stage -RelativePath $name) { throw "$Kind stage contains forbidden item: $name" } }
if ($Kind -eq 'Portable') {
    foreach ($name in @($manifest.core.required)) {
        if (-not (Test-Path -LiteralPath (Join-Path $stage $name) -PathType Leaf)) { throw "Portable stage misses Core file: $name" }
    }
}
Write-Host "$Kind stage verification passed: $stage"
