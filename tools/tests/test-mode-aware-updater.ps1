<# Ensures the updater cannot convert a portable copy into an installed one. #>
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$source = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\AboutBox.cpp')
$selector = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\UpdateArtifact.h')
foreach ($needle in @('SelectUpdateArtifact', 'DeploymentContext::CurrentMode()', 'DeploymentContext::CurrentCompatibilityTarget()', 'GetProfileArtifact')) {
    if ($source.IndexOf($needle, [StringComparison]::Ordinal) -lt 0) { throw "Mode-aware updater is missing '$needle'." }
}
foreach ($needle in @('UpdateArtifact', 'PortableUrl', 'PortableSHA256', 'SetupUrl', 'SetupSHA256', '-win7-win32-setup.exe', '-win7-win32-portable.zip')) {
    if ($selector.IndexOf($needle, [StringComparison]::Ordinal) -lt 0) { throw "Mode-aware update selector is missing '$needle'." }
}
Write-Host 'Mode-aware updater contract passed.'
