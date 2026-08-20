<# Ensures the updater cannot convert a portable copy into an installed one. #>
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$source = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\AboutBox.cpp')
foreach ($needle in @('DeploymentContext::CurrentMode() == DeploymentContext::Mode::Portable', 'GetPortableUpdateUrl', '-portable.zip', 'FictionBookEditorNext-%s-win32-setup.exe')) {
    if ($source.IndexOf($needle, [StringComparison]::Ordinal) -lt 0) { throw "Mode-aware updater is missing '$needle'." }
}
Write-Host 'Mode-aware updater contract passed.'
