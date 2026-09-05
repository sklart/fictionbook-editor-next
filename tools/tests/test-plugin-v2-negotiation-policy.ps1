<# Ensures bundled manifest plugins cannot silently downgrade to host API v1. #>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$manager = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\PluginManager.cpp')
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
if ($mainFrame -match 'apiGeneration == PluginApiV2Detected\) \{ TracePluginDiagnostic\(L"Import"[^\n]*E_NOTIMPL') { throw 'Import v2 negotiation still stops before invoking ImportV2.' }
if ($mainFrame -notmatch 'importV2->Import\(' -or $mainFrame -notmatch 'exportV2->Export\(') { throw 'Normal host dispatch does not invoke both v2 interfaces.' }
if ($importPrecompiledHeader -notmatch '#include\s+"\.\.\\\\fbe\\\\FBE\.h"') { throw 'ImportEPUB no longer uses the generated FBE.h contract.' }
$importProject = [regex]::Match($solution, '(?ms)^Project\([^\r\n]+\) = "ImportEPUB".*?^EndProject\s*$').Value
if ($importProject -notmatch '\{E1B04471-3393-4970-93ED-FB6A57BCDA8B\}\s*=\s*\{E1B04471-3393-4970-93ED-FB6A57BCDA8B\}') { throw 'ImportEPUB must depend on FBE so MIDL generates FBE.h before compilation.' }
foreach ($plugin in @($manifest.plugins)) {
    if ($plugin.type -notin 'Import','Export') { throw "Unexpected bundled plugin type: $($plugin.type)" }
}
Write-Host 'Plugin API v2 negotiation policy passed.'
