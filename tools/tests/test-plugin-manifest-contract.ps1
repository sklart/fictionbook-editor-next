<# Validates the v1 runtime contract without relying on an installed FBE. #>
[CmdletBinding()] param([string]$ManifestPath = (Join-Path $PSScriptRoot '..\..\runtime\Plugins\plugins.json'))
$ErrorActionPreference = 'Stop'
$manifest = Get-Content -LiteralPath $ManifestPath -Raw | ConvertFrom-Json
if ($manifest.schemaVersion -ne 1) { throw 'plugins.json must declare schemaVersion 1.' }
$required = 'id','type','module','clsid','menu','menuKey','activation'
$seen = @{ id=@{}; clsid=@{}; module=@{} }
foreach ($plugin in @($manifest.plugins)) {
    foreach ($name in $required) { if ([string]::IsNullOrWhiteSpace([string]$plugin.$name)) { throw "Missing $name." } }
    if ($plugin.type -notin 'Import','Export') { throw "Invalid type: $($plugin.type)" }
    if ($plugin.activation -ne 'local-com') { throw "Invalid activation: $($plugin.activation)" }
    if ($plugin.module -match '\.\.|[\\/:]') { throw "Unsafe module: $($plugin.module)" }
    foreach ($key in 'id','clsid','module') { if ($seen[$key].ContainsKey([string]$plugin.$key)) { throw "Duplicate ${key}: $($plugin.$key)" }; $seen[$key][[string]$plugin.$key]=$true }
}
Write-Host 'Plugin manifest v1 contract passed.'
