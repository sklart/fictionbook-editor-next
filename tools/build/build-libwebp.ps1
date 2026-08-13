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
$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$cmake = Get-ChildItem (Join-Path $vs "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin") -Filter cmake.exe -Recurse | Select-Object -First 1 -Expand FullName
if (-not $cmake) { throw "Не найден cmake.exe" }
$buildDir = Join-Path $repoRoot "build\libwebp\$Configuration"
& $cmake -S $sourceDir -B $buildDir -G "Visual Studio 17 2022" -A Win32 "-DCMAKE_INSTALL_PREFIX=$installDir" "-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded$<$<CONFIG:Debug>:Debug>" -DBUILD_SHARED_LIBS=OFF -DWEBP_BUILD_ANIM_UTILS=OFF -DWEBP_BUILD_CWEBP=OFF -DWEBP_BUILD_DWEBP=OFF -DWEBP_BUILD_GIF2WEBP=OFF -DWEBP_BUILD_IMG2WEBP=OFF -DWEBP_BUILD_VWEBP=OFF -DWEBP_BUILD_WEBPINFO=OFF -DWEBP_BUILD_WEBPMUX=OFF -DWEBP_BUILD_EXTRAS=OFF
if ($LASTEXITCODE) { exit $LASTEXITCODE }
& $cmake --build $buildDir --config $Configuration --target INSTALL
exit $LASTEXITCODE
