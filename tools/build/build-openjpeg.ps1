<# Builds the static OpenJPEG decoder used by FBE's FB2 image importer. #>
[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release")][string]$Configuration = "Release",
    [string]$PlatformToolset
)
$ErrorActionPreference = "Stop"
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$sourceDir = Join-Path $repoRoot "third_party\openjpeg"
$installDir = Join-Path $repoRoot "build\openjpeg\install\$Configuration"
if (-not (Test-Path -LiteralPath $sourceDir)) { throw "Не найден OpenJPEG: $sourceDir" }
$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$cmake = Get-ChildItem (Join-Path $vs "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin") -Filter cmake.exe -Recurse | Select-Object -First 1 -Expand FullName
if (-not $cmake) { throw "Не найден cmake.exe" }
$buildDir = Join-Path $repoRoot "build\openjpeg\$Configuration"
& $cmake -S $sourceDir -B $buildDir -G "Visual Studio 17 2022" -A Win32 "-DCMAKE_INSTALL_PREFIX=$installDir" "-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded$<$<CONFIG:Debug>:Debug>" -DCMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP=TRUE -DBUILD_SHARED_LIBS=OFF -DBUILD_CODEC=OFF -DBUILD_JPIP=OFF -DBUILD_MJ2=OFF -DBUILD_TESTING=OFF
if ($LASTEXITCODE) { exit $LASTEXITCODE }
& $cmake --build $buildDir --config $Configuration --target INSTALL
exit $LASTEXITCODE
