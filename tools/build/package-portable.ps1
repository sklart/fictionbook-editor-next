<#
.SYNOPSIS
Creates the portable payload from an already staged Core directory.

No compilation, shell registration, property-handler work, or MUI generation is
allowed here. This is deliberately a final materialisation step only.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$CoreDirectory,
    [Parameter(Mandatory)][string]$OutputDirectory
)
$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$core = (Resolve-Path -LiteralPath $CoreDirectory).Path
$output = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputDirectory)
& (Join-Path $PSScriptRoot 'verify-package-stage.ps1') -Kind Core -StageDirectory $core
if (Test-Path -LiteralPath $output) { Remove-Item -LiteralPath $output -Recurse -Force }
New-Item -ItemType Directory -Path $output | Out-Null
Copy-Item -Path (Join-Path $core '*') -Destination $output -Recurse -Force
@"
[Portable]
DataPath=Data
"@ | Set-Content -LiteralPath (Join-Path $output 'portable.ini') -Encoding utf8NoBOM
foreach ($name in @('Settings','Logs','Diagnostics','Recovery','Cache','Temp')) { New-Item -ItemType Directory -Path (Join-Path $output "Data\\$name") -Force | Out-Null }
& (Join-Path $PSScriptRoot 'verify-package-stage.ps1') -Kind Portable -StageDirectory $output
Write-Host "Portable package prepared: $output"
