[CmdletBinding()]
param(
    [ValidateSet('Release', 'Debug')]
    [string]$Configuration = 'Release',

    [ValidateSet('Win32', 'x64')]
    [string]$Platform = 'x64',

    [Parameter(Mandatory)]
    [ValidateSet('ru', 'en')]
    [string]$ExpectedLanguage,

    [string]$ShellDllPath = '',

    [switch]$SkipBuild,
    [switch]$NoRestartExplorer
)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = [Security.Principal.WindowsPrincipal]::new($identity)
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw 'Administrator rights are required for the installed shell-localization integration test.'
}

$registrationArguments = @{
    Configuration = $Configuration
    Platform = $Platform
    SkipBuild = $SkipBuild
    NoRestartExplorer = $NoRestartExplorer
}
if ($ShellDllPath) { $registrationArguments.ShellDllPath = $ShellDllPath }

# Use the same registration paths as installer repair/manual shell registration.
& (Join-Path $repoRoot 'tools\build\register-shell-integration.ps1') @registrationArguments
& (Join-Path $repoRoot 'tools\build\register-sequence-property-schema.ps1') -NoRestartExplorer

# GetDisplayName resolves the installed schema's @FBShell.dll,-201..-206 labels.
& (Join-Path $repoRoot 'tools\tests\test-fbe-property-schema-localization.ps1') `
    -RegisterSchema -ExpectedLanguage $ExpectedLanguage

Write-Host "Installed FBE shell localization integration test passed for $ExpectedLanguage."
