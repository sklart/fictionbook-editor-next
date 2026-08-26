[CmdletBinding()]
param(
    [ValidateSet('Win32')][string]$Platform = 'Win32',
    [string]$ArtifactsDirectory,
    [switch]$SkipInstaller,
    [switch]$AllowLegacyWin7Aliases
)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
if (-not $ArtifactsDirectory) { $ArtifactsDirectory = Join-Path $repoRoot 'out\artifacts' }
$ArtifactsDirectory = (Resolve-Path -LiteralPath $ArtifactsDirectory).Path
$versionText = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\version.h')
$match = [regex]::Match($versionText, 'FBE_VERSION_STRING\s+"(?<version>\d+\.\d+\.\d+)"')
if (-not $match.Success) { throw 'Не найден FBE_VERSION_STRING.' }
$version = $match.Groups['version'].Value
$architecture = $Platform.ToLowerInvariant()
$expected = @(
    "FictionBookEditorNext-$version-$architecture-setup.exe",
    "FictionBookEditorNext-$version-$architecture-portable.zip",
    "FictionBookEditorNext-$version-$architecture-symbols.zip",
    'SHA256SUMS.txt'
)
if ($AllowLegacyWin7Aliases) {
    $expected += @(
        "FictionBookEditorNext-$version-win7-$architecture-setup.exe",
        "FictionBookEditorNext-$version-win7-$architecture-portable.zip"
    )
}
$actual = @(Get-ChildItem -LiteralPath $ArtifactsDirectory -File | Select-Object -ExpandProperty Name | Sort-Object)
if (($actual -join '|') -ne (($expected | Sort-Object) -join '|')) {
    throw "Ожидается ровно один unified release: $($expected -join ', '). Получено: $($actual -join ', ')."
}
if (-not $AllowLegacyWin7Aliases -and ($actual | Where-Object { $_ -match '-win7-' })) { throw 'Unified release не должен содержать -win7- artifacts.' }
$checksums = @{}
foreach ($line in Get-Content -LiteralPath (Join-Path $ArtifactsDirectory 'SHA256SUMS.txt')) {
    if ($line -match '^(?<hash>[0-9A-Fa-f]{64})\s\s(?<name>.+)$') { $checksums[$Matches.name] = $Matches.hash }
}
foreach ($name in $expected | Where-Object { $_ -ne 'SHA256SUMS.txt' }) {
    if ($checksums[$name] -ne (Get-FileHash -LiteralPath (Join-Path $ArtifactsDirectory $name) -Algorithm SHA256).Hash) {
        throw "SHA256SUMS.txt не соответствует $name."
    }
}
if ($AllowLegacyWin7Aliases) {
    foreach ($pair in @(
        @("FictionBookEditorNext-$version-$architecture-setup.exe", "FictionBookEditorNext-$version-win7-$architecture-setup.exe"),
        @("FictionBookEditorNext-$version-$architecture-portable.zip", "FictionBookEditorNext-$version-win7-$architecture-portable.zip")
    )) {
        if ((Get-FileHash -LiteralPath (Join-Path $ArtifactsDirectory $pair[0]) -Algorithm SHA256).Hash -ne (Get-FileHash -LiteralPath (Join-Path $ArtifactsDirectory $pair[1]) -Algorithm SHA256).Hash) {
            throw "Legacy migration alias is not byte-identical: $($pair[1])"
        }
    }
}
if ((Get-Item -LiteralPath (Join-Path $ArtifactsDirectory $expected[0])).Length -eq 0) {
    throw 'Setup artifact is empty.'
}
Add-Type -AssemblyName System.IO.Compression
$portableZip = Join-Path $ArtifactsDirectory "FictionBookEditorNext-$version-$architecture-portable.zip"
$symbolsZip = Join-Path $ArtifactsDirectory "FictionBookEditorNext-$version-$architecture-symbols.zip"
$archive = [IO.Compression.ZipFile]::OpenRead($portableZip)
try {
    $entries = @($archive.Entries | ForEach-Object { $_.FullName.Replace('/', '\').TrimEnd('\') })
    $manifest = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'packaging\package-manifest.json') | ConvertFrom-Json
    foreach ($required in @($manifest.core.required) + @($manifest.portable.required)) {
        if ($entries -notcontains $required) { throw "Portable archive is missing required file: $required" }
    }
    foreach ($directory in @($manifest.core.runtimeDirectories)) {
        if (-not ($entries | Where-Object { $_ -like "$directory\*" })) { throw "Portable archive is missing runtime directory: $directory" }
    }
    foreach ($forbidden in @($manifest.core.forbidden) + @($manifest.portable.forbidden)) {
        if ($entries -contains $forbidden -or ($entries | Where-Object { $_ -like "$forbidden\*" })) { throw "Portable archive contains forbidden payload: $forbidden" }
    }
    if ($entries | Where-Object { $_ -match '\.(pdb|lib|exp|obj)$' }) { throw 'Portable archive contains build artifacts.' }
    foreach ($name in @('Scintilla.dll', 'Lexilla.dll', 'ExportDOCXBatch.exe', 'ExportEPUBBatch.exe', 'ImportEPUBBatch.exe', 'Utilities\ArchHandler\ZipHandler.exe', 'Utilities\ArchHandler\RarHandler.exe', 'portable.ini')) {
        if ($entries -notcontains $name) { throw "Portable archive is missing release component: $name" }
    }
}
finally { $archive.Dispose() }
$symbolArchive = [IO.Compression.ZipFile]::OpenRead($symbolsZip)
try {
    if (-not ($symbolArchive.Entries | Where-Object { $_.FullName -match '\.pdb$' })) { throw 'Symbols archive contains no PDB files.' }
}
finally { $symbolArchive.Dispose() }
Write-Host "Unified release artifacts verified: $ArtifactsDirectory"
