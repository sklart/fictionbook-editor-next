<# Validates the v1 runtime contract without relying on an installed FBE. #>
[CmdletBinding()] param([string]$ManifestPath = (Join-Path $PSScriptRoot '..\..\runtime\Plugins\plugins.json'))
$ErrorActionPreference = 'Stop'
$manifest = Get-Content -LiteralPath $ManifestPath -Raw | ConvertFrom-Json
if ($manifest.schemaVersion -ne 1) { throw 'plugins.json must declare schemaVersion 1.' }
function Test-StrictSchemaVersion([string]$json) { return $json -match '"schemaVersion"\s*:\s*1(?=\s*[,}])' }
foreach ($invalid in @('{"schemaVersion":1.0}', '{"schemaVersion":1.5}', '{"schemaVersion":1e0}', '{"schemaVersion":"1"}', '{"schemaVersion":-1}', '{"schemaVersion":2}')) { if (Test-StrictSchemaVersion $invalid) { throw "Non-integer v1 schema accepted: $invalid" } }
if (-not (Test-StrictSchemaVersion '{"schemaVersion":1}')) { throw 'Integer schema v1 was rejected.' }
$required = 'id','type','module','clsid','menu','menuKey','activation'
$seen = @{ id=@{}; clsid=@{}; module=@{} }
foreach ($plugin in @($manifest.plugins)) {
    foreach ($name in $required) { if ([string]::IsNullOrWhiteSpace([string]$plugin.$name)) { throw "Missing $name." } }
    if ($plugin.type -notin 'Import','Export') { throw "Invalid type: $($plugin.type)" }
    if ($plugin.activation -ne 'local-com') { throw "Invalid activation: $($plugin.activation)" }
    if ($plugin.module -match '\.\.|[\\/:]' -or -not $plugin.module.EndsWith('.dll', [StringComparison]::OrdinalIgnoreCase)) { throw "Unsafe module: $($plugin.module)" }
    $guid = [guid]$plugin.clsid
    foreach ($key in 'id','module') { $value = [string]$plugin.$key; if ($key -eq 'module') { $value = $value.ToUpperInvariant() }; if ($seen[$key].ContainsKey($value)) { throw "Duplicate ${key}: $($plugin.$key)" }; $seen[$key][$value]=$true }
    if ($seen.clsid.ContainsKey($guid)) { throw "Duplicate CLSID: $guid" }; $seen.clsid[$guid]=$true
}
Write-Host 'Plugin manifest v1 contract passed.'
