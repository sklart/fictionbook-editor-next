<# Builds only the static decoder library required by FBE. #>
[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release")][string]$Configuration = "Release",
    [string]$PlatformToolset
)
$ErrorActionPreference = "Stop"
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$sourceDir = Join-Path $repoRoot "third_party\libwebp"
$installDir = Join-Path $repoRoot "build\libwebp\install\$Configuration"
if (-not (Test-Path $sourceDir)) { throw "Не найден libwebp: $sourceDir" }
$vs = & (Join-Path $PSScriptRoot 'Resolve-VsCmake.ps1') -PlatformToolset $PlatformToolset
$cmake = $vs.CMake
$buildDir = Join-Path $repoRoot "build\libwebp\$Configuration"
$treeArguments = @{ DependencyName = 'libwebp'; BuildDirectory = $buildDir; InstallDirectory = $installDir; Vs = $vs; PlatformToolset = $PlatformToolset }
& (Join-Path $PSScriptRoot 'CmakeBuildTree.ps1') -Action Prepare @treeArguments
& $cmake -S $sourceDir -B $buildDir -G $vs.Generator -A Win32 -T $vs.GeneratorToolset "-DCMAKE_GENERATOR_INSTANCE=$($vs.GeneratorInstance)" "-DCMAKE_INSTALL_PREFIX=$installDir" '-DCMAKE_SYSTEM_VERSION=6.1' "-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded$<$<CONFIG:Debug>:Debug>" -DBUILD_SHARED_LIBS=OFF -DWEBP_BUILD_ANIM_UTILS=OFF -DWEBP_BUILD_CWEBP=OFF -DWEBP_BUILD_DWEBP=OFF -DWEBP_BUILD_GIF2WEBP=OFF -DWEBP_BUILD_IMG2WEBP=OFF -DWEBP_BUILD_VWEBP=OFF -DWEBP_BUILD_WEBPINFO=OFF -DWEBP_BUILD_WEBPMUX=OFF -DWEBP_BUILD_EXTRAS=OFF
if ($LASTEXITCODE) { exit $LASTEXITCODE }
& (Join-Path $PSScriptRoot 'CmakeBuildTree.ps1') -Action Write @treeArguments
& $cmake --build $buildDir --config $Configuration --target INSTALL
exit $LASTEXITCODE