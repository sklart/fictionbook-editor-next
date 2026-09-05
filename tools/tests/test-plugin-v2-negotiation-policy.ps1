<# Ensures bundled manifest plugins cannot silently downgrade to host API v1. #>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$manager = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\plugins\PluginManager.cpp')
$mainFrame = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\mainfrm.cpp')
$importPrecompiledHeader = Get-Content -Raw -LiteralPath (Join-Path $root 'src\import-epub\stdafx.h')
$solution = Get-Content -Raw -LiteralPath (Join-Path $root 'FBE.sln')
$manifest = Get-Content -Raw -LiteralPath (Join-Path $root 'runtime\Plugins\plugins.json') | ConvertFrom-Json
$method = [regex]::Match($manager, 'HRESULT PluginManager::NegotiateApi[\s\S]*?\n\}').Value
if ([string]::IsNullOrWhiteSpace($method)) { throw 'PluginManager::NegotiateApi was not found.' }
foreach ($needle in @('IID_IFBEPluginInfo2', 'GetPluginId', 'GetApiVersion', 'apiVersion != 2', 'IID_IFBEImportPlugin2', 'IID_IFBEExportPlugin2')) {
    if (-not $method.Contains($needle)) { throw "v2 negotiation is missing $needle." }
}
if ($method -match 'plugin-api-v1-fallback|E_NOINTERFACE\)\s*\{[^}]*return S_OK') { throw 'PluginManager still silently downgrades an interface mismatch to API v1.' }
if ($method -notmatch 'if \(FAILED\(hr\)\).*return hr') { throw 'Unexpected QueryInterface HRESULT is not propagated.' }
if ($method -notmatch 'plugin-info-mismatch.*E_ACCESSDENIED') { throw 'Plugin ID/API-version mismatch is not rejected.' }
if ($mainFrame -notmatch 'importV2->Import\(' -or $mainFrame -notmatch 'exportV2->Export\(') { throw 'Normal host dispatch does not invoke both v2 interfaces.' }
foreach ($legacyHostToken in @('PluginApiGeneration', 'PluginApiV1Fallback', 'PluginApiV2Detected', 'plugin-api-v1-fallback', 'IFBEImportPlugin\b', 'IFBEExportPlugin\b')) {
    if (($manager + "`n" + $mainFrame) -match $legacyHostToken) { throw "Dead host-side legacy plugin path remains: $legacyHostToken" }
}
if ($importPrecompiledHeader -notmatch '#include\s+"FBE\.h"') { throw 'ImportEPUB no longer uses the generated FBE.h contract.' }
$importProject = [regex]::Match($solution, '(?ms)^Project\([^\r\n]+\) = "ImportEPUB".*?^EndProject\s*$').Value
if ($importProject -notmatch '\{A6F27D46-6116-4A85-A1E5-8C68E79E5B4D\}\s*=\s*\{A6F27D46-6116-4A85-A1E5-8C68E79E5B4D\}') { throw 'ImportEPUB must depend on FBEContracts so MIDL generates FBE.h before compilation.' }
foreach ($plugin in @($manifest.plugins)) {
    if ($plugin.type -notin 'Import','Export') { throw "Unexpected bundled plugin type: $($plugin.type)" }
}
$legacyPluginInterfaces = @{
    'src\import-epub\ImportEPUBPlugin.cpp' = 'IFBEImportPlugin'
    'src\export-html\ExportHTMLPlugin.h' = 'IFBEExportPlugin'
    'src\export-epub\ExportEPUBPlugin.h' = 'IFBEExportPlugin'
    'src\export-docx\ExportDOCXPlugin.h' = 'IFBEExportPlugin'
}
foreach ($entry in $legacyPluginInterfaces.GetEnumerator()) {
    $source = Get-Content -Raw -LiteralPath (Join-Path $root $entry.Key)
    if ($source -notmatch ("COM_INTERFACE_ENTRY\(" + $entry.Value + "\)")) { throw "Plugin-side v1 ABI is no longer exposed by $($entry.Key)." }
}
Write-Host 'Plugin API v2 negotiation policy passed.'
