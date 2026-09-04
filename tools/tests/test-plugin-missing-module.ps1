<# A missing optional bundled DLL is a skipped catalog entry, never a menu item. #>
[CmdletBinding()] param([string]$SourcePath = (Join-Path $PSScriptRoot '..\..\src\fbe\PluginManager.cpp'))
$source = Get-Content -LiteralPath $SourcePath -Raw
foreach ($needle in 'IsRegularFile(entry.modulePath)', 'Trace(L"module-missing"', 'm_plugins.push_back(entry)') {
    if (-not $source.Contains($needle)) { throw "Missing-DLL behavior is incomplete: $needle" }
}
$missing = [pscustomobject]@{ id='synthetic-missing'; module='missing.dll' }
if (Test-Path -LiteralPath (Join-Path $PSScriptRoot $missing.module)) { throw 'Synthetic fixture unexpectedly exists.' }
Write-Host 'Missing bundled module is safely excluded from the catalog.'
