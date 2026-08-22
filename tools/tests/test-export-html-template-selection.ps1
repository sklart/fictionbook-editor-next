<# Source-contract companion for the native TemplateResolver regression test. #>
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$resolver = Get-Content -Raw -LiteralPath (Join-Path $root 'src\export-html\TemplateResolver.h')
foreach ($contract in @('UseCustomTemplate', 'ExportHTML.dll', 'FBE.exe', 'DeleteValue(L"Template")')) {
    if ($resolver.IndexOf($contract, [StringComparison]::Ordinal) -lt 0) { throw "Template resolver lacks migration contract: $contract" }
}
foreach ($contract in @('ResolveExportHtmlTemplateState', 'ExportHtmlPathsEqual', 'GetFullPathName')) {
    if ($resolver.IndexOf($contract, [StringComparison]::Ordinal) -lt 0) { throw "Template resolver lacks production decision contract: $contract" }
}
$plugin = Get-Content -Raw -LiteralPath (Join-Path $root 'src\export-html\ExportHTMLPlugin.cpp')
$guard = $plugin.IndexOf('fEmbeddedImages && dlg.m_usingCustomTemplate && !SupportsEmbeddedImages', [StringComparison]::Ordinal)
$createFile = $plugin.IndexOf('CreateFile(dlg.m_szFileName', [StringComparison]::Ordinal)
if ($guard -lt 0 -or $guard -gt $createFile) { throw 'Incompatible custom XSL must be rejected before the output file is created.' }
Write-Host 'ExportHTML template selection regression passed.'
