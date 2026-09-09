<# Builds the minimal static libarchive contour used by FBE archive documents. #>
[CmdletBinding()]
param([ValidateSet('Debug','Release')][string]$Configuration = 'Release', [string]$PlatformToolset)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
& (Join-Path $PSScriptRoot 'build-zlib.ps1') -Configuration $Configuration -PlatformToolset $PlatformToolset
if ($LASTEXITCODE) { throw "zlib build failed with exit code $LASTEXITCODE." }
$source = Join-Path $root 'third_party\libarchive'
$zlib = Join-Path $root "build\zlib\install\$Configuration"
if (-not (Test-Path -LiteralPath (Join-Path $source 'CMakeLists.txt'))) { throw "Не найден исходный libarchive: $source" }
$vs = & (Join-Path $PSScriptRoot 'Resolve-VsCmake.ps1') -PlatformToolset $PlatformToolset
$suffix = if ($PlatformToolset) { "-$PlatformToolset" } else { '-default' }
$build = Join-Path $root "build\libarchive\$Configuration$suffix"
$install = Join-Path $root "build\libarchive\install\$Configuration"
$tree = @{ DependencyName = 'libarchive'; BuildDirectory = $build; InstallDirectory = $install; Vs = $vs; PlatformToolset = $PlatformToolset }
& (Join-Path $PSScriptRoot 'CmakeBuildTree.ps1') -Action Prepare @tree
& $vs.CMake -S $source -B $build -G $vs.Generator -A Win32 -T $vs.GeneratorToolset "-DCMAKE_GENERATOR_INSTANCE=$($vs.GeneratorInstance)" "-DCMAKE_INSTALL_PREFIX=$install" "-DCMAKE_PREFIX_PATH=$zlib" "-DZLIB_LIBRARY_DEBUG=$(Join-Path $zlib 'lib\zsd.lib')" "-DZLIB_LIBRARY_RELEASE=$(Join-Path $zlib 'lib\zs.lib')" "-DZLIB_INCLUDE_DIR=$(Join-Path $zlib 'include')" '-DCMAKE_SYSTEM_VERSION=6.1' '-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded$<$<CONFIG:Debug>:Debug>' -DBUILD_SHARED_LIBS=OFF -DWINDOWS_VERSION=WIN7 -DENABLE_TEST=OFF -DENABLE_TAR=OFF -DENABLE_CPIO=OFF -DENABLE_CAT=OFF -DENABLE_UNZIP=OFF -DENABLE_OPENSSL=OFF -DENABLE_BZip2=OFF -DENABLE_LZ4=OFF -DENABLE_LZMA=OFF -DENABLE_ZSTD=OFF -DENABLE_ZLIB=ON
if ($LASTEXITCODE) { throw "libarchive configuration failed with exit code $LASTEXITCODE." }
& (Join-Path $PSScriptRoot 'CmakeBuildTree.ps1') -Action Write @tree
& $vs.CMake --build $build --config $Configuration --target INSTALL
if ($LASTEXITCODE) { throw "libarchive build failed with exit code $LASTEXITCODE." }
