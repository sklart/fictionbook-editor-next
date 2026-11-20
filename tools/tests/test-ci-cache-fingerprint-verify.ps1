[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$fixture = Join-Path $repo ('build/ci-fingerprint-' + [guid]::NewGuid().ToString('N'))
$build = Join-Path $fixture 'tree'; $install = Join-Path $fixture 'install'
$tool = Join-Path $repo 'tools/build/CmakeBuildTree.ps1'
$vs = [pscustomobject]@{ Generator='Visual Studio 17 2022'; InstallationPath=$repo; InstallationVersion='17.0'; VCToolsVersion='14.44'; GeneratorToolset='v143'; CMakeVersion='3.0' }
$arguments = @{DependencyName='fixture'; BuildDirectory=$build; InstallDirectory=$install; Vs=$vs; PlatformToolset='v143'}
try {
    New-Item -ItemType Directory -Force -Path $build,$install | Out-Null
    & $tool -Action Write @arguments | Out-Null
    & $tool -Action Verify @arguments | Out-Null
    $metadata = Join-Path $build '.fbe-cmake-fingerprint.json'
    $json = Get-Content -LiteralPath $metadata -Raw | ConvertFrom-Json; $json.vcToolsVersion = 'different'
    $json | ConvertTo-Json | Set-Content -LiteralPath $metadata -Encoding utf8
    $rejected = $false
    try { & $tool -Action Verify @arguments | Out-Null } catch { $rejected = $true }
    if (-not $rejected -or -not (Test-Path -LiteralPath $metadata) -or -not (Test-Path -LiteralPath $install)) { throw 'Incompatible cache must be rejected without deleting restored data.' }
    Write-Host 'Non-destructive CMake cache fingerprint verification passed.'
} finally {
    $metadata = Join-Path $build '.fbe-cmake-fingerprint.json'
    if (Test-Path -LiteralPath $metadata) { Remove-Item -LiteralPath $metadata -Force }
    foreach ($directory in @($build,$install,$fixture)) { if (Test-Path -LiteralPath $directory -PathType Container) { Remove-Item -LiteralPath $directory -Force } }
}
