<# Builds NSIS from verified, prebuilt Core and Integration stages. #>
[CmdletBinding()]
param([string]$CoreDirectory, [string]$IntegrationDirectory, [string]$OutputDirectory, [string]$OutputFile)
$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
if (-not $CoreDirectory) { $CoreDirectory = Join-Path $repoRoot 'out\stage\Core' }
if (-not $IntegrationDirectory) { $IntegrationDirectory = Join-Path $repoRoot 'out\stage\Integration' }
if (-not $OutputDirectory) { $OutputDirectory = Join-Path $repoRoot 'out\package\InstallerInput' }
& (Join-Path $repoRoot 'tools\version\sync-version.ps1')
& (Join-Path $repoRoot 'tools\localization\export-nsis-language-pack-plan.ps1')
& (Join-Path $repoRoot 'tools\localization\export-nsis-installer-fallbacks.ps1')
& (Join-Path $PSScriptRoot 'prepare-installer.ps1') -CoreDirectory $CoreDirectory -IntegrationDirectory $IntegrationDirectory -OutputDirectory $OutputDirectory
$inputPath = (Resolve-Path -LiteralPath $OutputDirectory).Path
$hashes = @{}
foreach ($pair in @(@('FBE_SHELL_X86_SHA256','FBShell.dll'), @('FBE_SHELL_X64_SHA256','FBShell64.dll'), @('FBE_SCHEMA_SHA256','FBE.Sequence.propdesc'))) {
    $path = Join-Path $inputPath $pair[1]
    if (-not (Test-Path -LiteralPath $path -PathType Leaf) -or (Get-Item -LiteralPath $path).Length -eq 0) { throw "Missing or empty installer payload: $path" }
    $hashes[$pair[0]] = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
    if ($hashes[$pair[0]] -notmatch '^[0-9a-fA-F]{64}$') { throw "Invalid package SHA-256: $path" }
}
$installerDir = Join-Path $repoRoot 'packaging\nsis\Installer'
if (-not $OutputFile) {
    $versionText = Get-Content -LiteralPath (Join-Path $installerDir 'version.nsh') -Raw
    $version = [regex]::Match($versionText, '(?m)^!define PRODUCT_VER_NUM "([^"]+)"').Groups[1].Value
    if (-not $version) { throw 'PRODUCT_VER_NUM is missing from version.nsh.' }
    $OutputFile = Join-Path $repoRoot "out\artifacts\FictionBookEditorNext-$version-win32-setup.exe"
}
$OutputFile = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputFile)
New-Item -ItemType Directory -Path (Split-Path -Parent $OutputFile) -Force | Out-Null
$compiler = & (Join-Path $PSScriptRoot 'resolve-nsis.ps1')
$arguments = @('/X!addincludedir ..\NSIS', '/X!addplugindir /x86-unicode ..\NSIS', "/DINPUTDIR=$inputPath", "/DOUTPUTFILE=$OutputFile")
foreach ($name in @('FBE_SHELL_X86_SHA256','FBE_SHELL_X64_SHA256','FBE_SCHEMA_SHA256')) { $arguments += "/D$name=$($hashes[$name])" }
$arguments += 'MakeInstaller.nsi'
if (Test-Path -LiteralPath $OutputFile) { Remove-Item -LiteralPath $OutputFile -Force }
Push-Location $installerDir
try {
    & $compiler @arguments
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $OutputFile -PathType Leaf)) { throw "NSIS build failed (exit $LASTEXITCODE): $OutputFile" }
}
catch {
    if (Test-Path -LiteralPath $OutputFile) { Remove-Item -LiteralPath $OutputFile -Force }
    throw
}
finally { Pop-Location }
Write-Host "Installer built: $OutputFile"
