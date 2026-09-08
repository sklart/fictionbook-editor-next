<#
.SYNOPSIS
Validates the generated CMake tree of one dependency against the selected VS
instance and removes only that generated tree (and its install prefix) when it
cannot safely be reused.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidateSet('Prepare', 'Write')][string]$Action,
    [Parameter(Mandatory)][string]$DependencyName,
    [Parameter(Mandatory)][string]$BuildDirectory,
    [Parameter(Mandatory)][string]$InstallDirectory,
    [Parameter(Mandatory)][object]$Vs,
    [Parameter(Mandatory)][string]$PlatformToolset,
    [string]$Platform = 'Win32'
)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$buildRoot = [IO.Path]::GetFullPath((Join-Path $repoRoot 'build'))

function Get-NormalizedPath([string]$Path) {
    return [IO.Path]::GetFullPath($Path).TrimEnd([char[]]@('\', '/')).ToLowerInvariant()
}

function Assert-GeneratedBuildPath([string]$Path, [string]$Purpose) {
    $normalizedPath = Get-NormalizedPath $Path
    $normalizedRoot = Get-NormalizedPath $buildRoot
    if (-not $normalizedPath.StartsWith($normalizedRoot + '\')) {
        throw "$Purpose '$Path' is outside the generated build root '$buildRoot'."
    }
}

function Get-Fingerprint {
    [ordered]@{
        schemaVersion = 1
        dependency = $DependencyName
        generator = $Vs.Generator
        installationPath = (Get-NormalizedPath $Vs.InstallationPath)
        installationVersion = [string]$Vs.InstallationVersion
        platform = $Platform
        platformToolset = $PlatformToolset
        vcToolsVersion = [string]$Vs.VCToolsVersion
        generatorToolset = [string]$Vs.GeneratorToolset
        cmakeVersion = [string]$Vs.CMakeVersion
    }
}

function Test-Fingerprint([object]$Actual, [object]$Expected) {
    if (-not $Actual) { return $false }
    foreach ($property in $Expected.Keys) {
        if ([string]$Actual.$property -ne [string]$Expected[$property]) { return $false }
    }
    return $true
}

Assert-GeneratedBuildPath $BuildDirectory 'CMake build directory'
Assert-GeneratedBuildPath $InstallDirectory 'CMake install directory'
$metadataPath = Join-Path $BuildDirectory '.fbe-cmake-fingerprint.json'
$fingerprint = Get-Fingerprint

if ($Action -eq 'Write') {
    if (-not (Test-Path -LiteralPath $BuildDirectory)) { throw "Cannot write fingerprint: build directory '$BuildDirectory' does not exist." }
    $fingerprint | ConvertTo-Json | Set-Content -LiteralPath $metadataPath -Encoding utf8NoBOM
    return [PSCustomObject]@{ Recreated = $false; MetadataPath = $metadataPath }
}

$recreated = $false
if (Test-Path -LiteralPath $BuildDirectory) {
    $actual = $null
    if (Test-Path -LiteralPath $metadataPath) {
        try { $actual = Get-Content -LiteralPath $metadataPath -Raw | ConvertFrom-Json } catch { $actual = $null }
    }
    $oldInstanceStillExists = $actual -and $actual.installationPath -and (Test-Path -LiteralPath ([string]$actual.installationPath))
    if (-not (Test-Fingerprint $actual $fingerprint) -or -not $oldInstanceStillExists) {
        Write-Host "Recreating stale $DependencyName CMake tree: toolchain fingerprint differs or its Visual Studio instance is unavailable."
        Remove-Item -LiteralPath $BuildDirectory -Recurse -Force
        if (Test-Path -LiteralPath $InstallDirectory) { Remove-Item -LiteralPath $InstallDirectory -Recurse -Force }
        $recreated = $true
    }
}
[PSCustomObject]@{ Recreated = $recreated; MetadataPath = $metadataPath }
