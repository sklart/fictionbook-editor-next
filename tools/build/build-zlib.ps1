<# Builds the static zlib required for deflated ZIP entries in libarchive. #>
[CmdletBinding()]
param([ValidateSet('Debug','Release')][string]$Configuration = 'Release', [string]$PlatformToolset)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$source = Join-Path $root 'third_party\zlib'
if (-not (Test-Path -LiteralPath (Join-Path $source 'CMakeLists.txt'))) { throw "Не найден исходный zlib: $source" }
$vs = & (Join-Path $PSScriptRoot 'Resolve-VsCmake.ps1') -PlatformToolset $PlatformToolset
$suffix = if ($PlatformToolset) { "-$PlatformToolset" } else { '-default' }
$build = Join-Path $root "build\zlib\$Configuration$suffix"
$install = Join-Path $root "build\zlib\install\$Configuration"
$tree = @{ DependencyName = 'zlib'; BuildDirectory = $build; InstallDirectory = $install; Vs = $vs; PlatformToolset = $PlatformToolset }
& (Join-Path $PSScriptRoot 'CmakeBuildTree.ps1') -Action Prepare @tree
& $vs.CMake -S $source -B $build -G $vs.Generator -A Win32 -T $vs.GeneratorToolset "-DCMAKE_GENERATOR_INSTANCE=$($vs.GeneratorInstance)" "-DCMAKE_INSTALL_PREFIX=$install" '-DCMAKE_SYSTEM_VERSION=6.1' '-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded$<$<CONFIG:Debug>:Debug>' -DBUILD_SHARED_LIBS=OFF -DZLIB_BUILD_SHARED=OFF -DZLIB_BUILD_STATIC=ON -DZLIB_BUILD_TESTING=OFF
if ($LASTEXITCODE) { throw "zlib configuration failed with exit code $LASTEXITCODE." }
& (Join-Path $PSScriptRoot 'CmakeBuildTree.ps1') -Action Write @tree
& $vs.CMake --build $build --config $Configuration --target INSTALL
if ($LASTEXITCODE) { throw "zlib build failed with exit code $LASTEXITCODE." }
