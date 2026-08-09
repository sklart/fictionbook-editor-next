param(
    [string]$RepoRoot = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot))
)

$ErrorActionPreference = 'Stop'

$viewSource = Get-Content -LiteralPath (Join-Path $RepoRoot 'src\fbe\FBEview.cpp') -Raw

if ($viewSource -notmatch 'execCommand\(L"AutoUrlDetect",\s*VARIANT_FALSE,\s*_variant_t\(VARIANT_FALSE\)\)') {
    throw 'MSHTML automatic URL detection is not disabled during visual editor initialization.'
}

if ($viewSource -notmatch 'Links in FB2 must' -or $viewSource -notmatch 'only be created by an explicit editor command') {
    throw 'The requirement for explicit hyperlink creation in FB2 is not documented.'
}

Write-Host 'MSHTML automatic URL detection is disabled in the visual editor.'
