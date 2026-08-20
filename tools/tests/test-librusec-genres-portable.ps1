<# Ensures a newly staged portable payload contains current Librusec catalogs. #>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$stage = Get-Content -Raw -LiteralPath (Join-Path $root 'tools\build\stage-core.ps1')
$loader = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\ExternalHelper.cpp')
$manifest = Get-Content -Raw -LiteralPath (Join-Path $root 'packaging\package-manifest.json') | ConvertFrom-Json

foreach ($name in @('genres.txt', 'genres.rus.txt', 'genres.ukr.txt', 'genres.librusec.txt', 'genres.rus.librusec.txt')) {
    if ($manifest.core.required -notcontains $name) { throw "Package manifest does not require $name." }
}
foreach ($name in @('genres.librusec.txt', 'genres.rus.librusec.txt')) {
    if ($stage.IndexOf($name, [StringComparison]::Ordinal) -lt 0) { throw "Core staging does not create $name." }
    if ($loader.IndexOf($name, [StringComparison]::Ordinal) -lt 0) { throw "Runtime does not discover $name." }
}
foreach ($name in @('genres.txt_L', 'genres.rus.txt_L')) {
    if ($loader.IndexOf($name, [StringComparison]::Ordinal) -lt 0) { throw "Runtime lacks legacy fallback $name." }
}

Write-Host 'Librusec genres portable contract passed.'
