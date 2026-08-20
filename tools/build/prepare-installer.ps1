<# Prepares a flat NSIS input from distinct, prebuilt Core and Integration stages. #>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$CoreDirectory,
    [Parameter(Mandatory)][string]$IntegrationDirectory,
    [Parameter(Mandatory)][string]$OutputDirectory
)
$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$core = (Resolve-Path -LiteralPath $CoreDirectory).Path
$integration = (Resolve-Path -LiteralPath $IntegrationDirectory).Path
$output = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputDirectory)
& (Join-Path $PSScriptRoot 'verify-package-stage.ps1') -Kind Core -StageDirectory $core
& (Join-Path $PSScriptRoot 'verify-package-stage.ps1') -Kind Integration -StageDirectory $integration
if (Test-Path -LiteralPath $output) { Remove-Item -LiteralPath $output -Recurse -Force }
New-Item -ItemType Directory -Path $output | Out-Null
Copy-Item -Path (Join-Path $core '*') -Destination $output -Recurse -Force
Copy-Item -Path (Join-Path $integration '*') -Destination $output -Recurse -Force
$uacThirdPartyDir = Join-Path $repoRoot 'third_party\uac'; $uacNsisDir = Join-Path $repoRoot 'packaging\nsis\NSIS'
if (Test-Path -LiteralPath $uacThirdPartyDir) {
    $pluginDir = Join-Path $uacNsisDir 'Plugins\x86-unicode'; New-Item -ItemType Directory -Path $pluginDir -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $uacThirdPartyDir 'UAC.nsh') -Destination (Join-Path $uacNsisDir 'UAC.nsh') -Force
    Copy-Item -LiteralPath (Join-Path $uacThirdPartyDir 'Plugins\x86-unicode\UAC.dll') -Destination (Join-Path $uacNsisDir 'UAC.dll') -Force
    Copy-Item -LiteralPath (Join-Path $uacThirdPartyDir 'Plugins\x86-unicode\UAC.dll') -Destination $pluginDir -Force
}
Write-Host "Installer input prepared: $output"
