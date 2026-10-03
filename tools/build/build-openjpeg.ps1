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
$vs = & (Join-Path $PSScriptRoot 'Resolve-VsCmake.ps1') -PlatformToolset $PlatformToolset
$cmake = $vs.CMake
$buildDir = Join-Path $repoRoot "build\openjpeg\$Configuration"
$treeArguments = @{ DependencyName = 'openjpeg'; BuildDirectory = $buildDir; InstallDirectory = $installDir; Vs = $vs; PlatformToolset = $PlatformToolset }
& (Join-Path $PSScriptRoot 'CmakeBuildTree.ps1') -Action Prepare @treeArguments
& $cmake -S $sourceDir -B $buildDir -G $vs.Generator -A Win32 -T $vs.GeneratorToolset "-DCMAKE_GENERATOR_INSTANCE=$($vs.GeneratorInstance)" "-DCMAKE_INSTALL_PREFIX=$installDir" '-DCMAKE_SYSTEM_VERSION=6.1' "-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded$<$<CONFIG:Debug>:Debug>" -DCMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP=TRUE -DBUILD_SHARED_LIBS=OFF -DBUILD_CODEC=OFF -DBUILD_JPIP=OFF -DBUILD_MJ2=OFF -DBUILD_TESTING=OFF
if ($LASTEXITCODE) { exit $LASTEXITCODE }
& (Join-Path $PSScriptRoot 'CmakeBuildTree.ps1') -Action Write @treeArguments
& $cmake --build $buildDir --config $Configuration --target INSTALL
exit $LASTEXITCODE