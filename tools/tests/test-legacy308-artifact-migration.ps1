<# Exercises the 3.0.8 migration aliases without creating a tag or release. #>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$version = ([regex]::Match((Get-Content -Raw (Join-Path $root 'src\version.h')), 'FBE_VERSION_STRING\s+"(?<v>\d+\.\d+\.\d+)"')).Groups['v'].Value
. (Join-Path $root 'tools\build\UpdateVersion.ps1')
if (-not (Test-FbeLegacy308MigrationRequired $version)) {
    Write-Host "3.0.8 migration artifact fixture skipped for version $version."
    return
}
$releaseVersion = "$version-rc.2"
$assetVersion = Get-FbeAssetVersion $releaseVersion
$fixture = Join-Path $root 'out\tests\legacy308-artifact-migration'
Remove-Item -LiteralPath $fixture -Force -Recurse -ErrorAction SilentlyContinue
$payload = Join-Path $fixture 'portable'
New-Item -ItemType Directory -Path $payload -Force | Out-Null
$manifest = Get-Content -Raw -LiteralPath (Join-Path $root 'packaging\package-manifest.json') | ConvertFrom-Json
foreach ($relative in @($manifest.core.required) + @($manifest.portable.required)) {
    $path = Join-Path $payload $relative
    if ([IO.Path]::GetExtension($path)) {
        New-Item -ItemType Directory -Path (Split-Path -Parent $path) -Force | Out-Null
        [IO.File]::WriteAllText($path, $relative)
    } else {
        New-Item -ItemType Directory -Path $path -Force | Out-Null
        [IO.File]::WriteAllText((Join-Path $path '.keep'), 'directory')
    }
}
foreach ($directory in @($manifest.core.runtimeDirectories)) {
    $path = Join-Path $payload $directory
    New-Item -ItemType Directory -Path $path -Force | Out-Null
    [IO.File]::WriteAllText((Join-Path $path '.keep'), 'runtime')
}
foreach ($relative in @('Utilities\ArchHandler\ZipHandler.exe', 'Utilities\ArchHandler\RarHandler.exe')) {
    $path = Join-Path $payload $relative
    New-Item -ItemType Directory -Path (Split-Path -Parent $path) -Force | Out-Null
    [IO.File]::WriteAllText($path, $relative)
}
New-Item -ItemType Directory -Path $fixture -Force | Out-Null
$setup = Join-Path $fixture "FictionBookEditorNext-$assetVersion-win32-setup.exe"
[IO.File]::WriteAllText($setup, 'fixture setup')
$portable = Join-Path $fixture "FictionBookEditorNext-$assetVersion-win32-portable.zip"
Compress-Archive -Path (Join-Path $payload '*') -DestinationPath $portable -CompressionLevel Optimal
$symbolsRoot = Join-Path $fixture 'symbols'
New-Item -ItemType Directory -Path $symbolsRoot -Force | Out-Null
foreach ($name in @('FBE.pdb', 'FBV.pdb', 'ExportHTML.pdb', 'ExportDOCX.pdb', 'ExportEPUB.pdb', 'ImportEPUB.pdb', 'ImportEPUBLunaSVG.pdb', 'ExportDOCXBatch.pdb', 'ExportEPUBBatch.pdb', 'ImportEPUBBatch.pdb', 'FBShell.propertyhandler.win32.pdb', 'FBShell.propertyhandler.x64.pdb')) {
    [IO.File]::WriteAllText((Join-Path $symbolsRoot $name), $name)
}
$symbols = Join-Path $fixture "FictionBookEditorNext-$assetVersion-win32-symbols.zip"
Compress-Archive -Path (Join-Path $symbolsRoot '*') -DestinationPath $symbols -CompressionLevel Optimal
$legacySetup = Join-Path $fixture "FictionBookEditorNext-$version-win32-setup.exe"
$legacyPortable = Join-Path $fixture "FictionBookEditorNext-$version-win32-portable.zip"
Copy-Item -LiteralPath $setup -Destination $legacySetup
Copy-Item -LiteralPath $portable -Destination $legacyPortable
Copy-Item -LiteralPath $legacySetup -Destination (Join-Path $fixture "FictionBookEditorNext-$version-win7-win32-setup.exe")
Copy-Item -LiteralPath $legacyPortable -Destination (Join-Path $fixture "FictionBookEditorNext-$version-win7-win32-portable.zip")
$checksums = Get-ChildItem -LiteralPath $fixture -File | Sort-Object Name | ForEach-Object {
    "{0}  {1}" -f (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash, $_.Name
}
[IO.File]::WriteAllLines((Join-Path $fixture 'SHA256SUMS.txt'), @($checksums), [Text.Encoding]::ASCII)
& (Join-Path $root 'tools\build\verify-artifacts.ps1') -Platform Win32 -ArtifactsDirectory $fixture -ReleaseTag "v$releaseVersion" -AllowLegacyWin7Aliases
$candidate = Join-Path $fixture 'update.xml'
& (Join-Path $root 'tools\build\new-update-manifest-candidate.ps1') -ArtifactsRoot $fixture -OutputPath $candidate -ReleaseTag "v$releaseVersion"
& (Join-Path $root 'tools\build\validate-update-manifest.ps1') -ManifestPath $candidate -ExpectedReleaseTag "v$releaseVersion" -Feed PrereleaseFeed
[xml]$migrationManifest = Get-Content -Raw -LiteralPath $candidate
if ($migrationManifest.FBE.Artifacts.SetupUrl -notlike "*$assetVersion-win32-setup.exe" -or
    $migrationManifest.FBE.Artifacts.Modern.SetupUrl -notlike "*$version-win32-setup.exe" -or
    $migrationManifest.FBE.Artifacts.Win7.SetupUrl -notlike "*$version-win7-win32-setup.exe" -or
    $migrationManifest.FBE.Artifacts.Modern.SetupSHA256 -ne $migrationManifest.FBE.Artifacts.SetupSHA256 -or
    $migrationManifest.FBE.Artifacts.Win7.PortableSHA256 -ne $migrationManifest.FBE.Artifacts.PortableSHA256) {
    throw 'v3.0.8-rc.1 compatibility updater did not receive the expected base-version aliases.'
}

# Stable 3.0.8 must continue to validate independently of the prerelease
# fixture; its canonical setup/portable names are already the base aliases.
$stableFixture = Join-Path $root 'out\tests\legacy308-artifact-migration-stable'
Remove-Item -LiteralPath $stableFixture -Force -Recurse -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Path $stableFixture -Force | Out-Null
$stableSetup = Join-Path $stableFixture "FictionBookEditorNext-$version-win32-setup.exe"
$stablePortable = Join-Path $stableFixture "FictionBookEditorNext-$version-win32-portable.zip"
$stableSymbols = Join-Path $stableFixture "FictionBookEditorNext-$version-win32-symbols.zip"
Copy-Item -LiteralPath $setup -Destination $stableSetup
Copy-Item -LiteralPath $portable -Destination $stablePortable
Copy-Item -LiteralPath $symbols -Destination $stableSymbols
Copy-Item -LiteralPath $stableSetup -Destination (Join-Path $stableFixture "FictionBookEditorNext-$version-win7-win32-setup.exe")
Copy-Item -LiteralPath $stablePortable -Destination (Join-Path $stableFixture "FictionBookEditorNext-$version-win7-win32-portable.zip")
$stableChecksums = Get-ChildItem -LiteralPath $stableFixture -File | Sort-Object Name | ForEach-Object {
    "{0}  {1}" -f (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash, $_.Name
}
[IO.File]::WriteAllLines((Join-Path $stableFixture 'SHA256SUMS.txt'), @($stableChecksums), [Text.Encoding]::ASCII)
& (Join-Path $root 'tools\build\verify-artifacts.ps1') -Platform Win32 -ArtifactsDirectory $stableFixture -ReleaseTag "v$version" -AllowLegacyWin7Aliases
Write-Host '3.0.8 prerelease migration and stable artifact fixtures passed.'
