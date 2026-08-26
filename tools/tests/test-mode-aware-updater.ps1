<# Ensures the updater cannot convert a portable copy into an installed one. #>
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$source = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\AboutBox.cpp')
$selector = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\UpdateArtifact.h')
foreach ($needle in @('SelectUpdateArtifact', 'DeploymentContext::CurrentMode()', 'GetArtifact')) {
    if ($source.IndexOf($needle, [StringComparison]::Ordinal) -lt 0) { throw "Mode-aware updater is missing '$needle'." }
}
foreach ($needle in @('UpdateArtifact', 'PortableUrl', 'PortableSHA256', 'SetupUrl', 'SetupSHA256', '-win32-setup.exe', '-win32-portable.zip')) {
    if ($selector.IndexOf($needle, [StringComparison]::Ordinal) -lt 0) { throw "Mode-aware update selector is missing '$needle'." }
}
foreach ($forbidden in @('CompatibilityTarget', '-win7-win32-setup.exe', '-win7-win32-portable.zip')) {
    if ($selector.IndexOf($forbidden, [StringComparison]::Ordinal) -ge 0) { throw "Unified update selector must not contain '$forbidden'." }
}
Write-Host 'Mode-aware updater contract passed.'
