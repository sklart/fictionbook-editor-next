<# Ensures every catalog text is either bound by the generic dialog layer or
   consumed by an existing explicit runtime-localization implementation. #>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$catalog = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'localization\app-ui\fbe-small-dialogs.json') | ConvertFrom-Json
$source = [string]::Join("`n", @(Get-ChildItem -LiteralPath (Join-Path $repoRoot 'src\fbe') -Recurse -File | Where-Object { $_.Extension -in '.cpp', '.h' } | ForEach-Object { Get-Content -Raw -LiteralPath $_.FullName }))
$bindingSource = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\RuntimeLocalization.cpp')

foreach ($entry in $catalog.strings.PSObject.Properties) {
    $key = $entry.Name
    $value = $entry.Value
    if ($value.targetId -eq 'IDC_STATIC') {
        throw "Runtime-localized control must have a stable ID, not IDC_STATIC: $key"
    }
    if (-not $source.Contains($key)) {
        throw "JSON dialog key has no runtime consumer or documented dynamic binding: $key"
    }
}

if (-not $bindingSource.Contains('FbeApplyRuntimeDialogLocalization')) {
    throw 'The generic runtime dialog binding is missing.'
}
Write-Host "Runtime dialog coverage verified: $(@($catalog.strings.PSObject.Properties).Count) catalog keys."
