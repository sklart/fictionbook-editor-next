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
foreach ($name in @('Settings','Scripts','Dictionaries','Themes','Logs','Diagnostics','Recovery','Cache','Temp')) {
    $directory = Join-Path $output "Data\\$name"; New-Item -ItemType Directory -Path $directory -Force | Out-Null
    # Empty directories are otherwise omitted by Compress-Archive.
    Set-Content -LiteralPath (Join-Path $directory '.keep') -Value '' -Encoding ascii
}
& (Join-Path $PSScriptRoot 'verify-package-stage.ps1') -Kind Portable -StageDirectory $output
& (Join-Path $repoRoot 'tools\tests\test-core-identity.ps1') -CoreDirectory $core -CandidateDirectory $output -CandidateName 'portable payload'
Write-Host "Portable package prepared: $output"
