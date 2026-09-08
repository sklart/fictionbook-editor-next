<# Builds libheif with only static HEVC/AV1 decoding backends. #>
[CmdletBinding()]
param([ValidateSet('Debug','Release')][string]$Configuration = 'Release', [string]$PlatformToolset)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
& (Join-Path $PSScriptRoot 'build-libde265.ps1') -Configuration $Configuration -PlatformToolset $PlatformToolset
if ($LASTEXITCODE) { throw "libde265 build failed with exit code $LASTEXITCODE." }; & (Join-Path $PSScriptRoot 'build-aom.ps1') -Configuration $Configuration -PlatformToolset $PlatformToolset
if ($LASTEXITCODE) { throw "AOM build failed with exit code $LASTEXITCODE." }
$vs = & (Join-Path $PSScriptRoot 'Resolve-VsCmake.ps1') -PlatformToolset $PlatformToolset
$cmake = $vs.CMake
$source = Join-Path $root 'third_party\libheif'; $suffix = if ($PlatformToolset) { "-$PlatformToolset" } else { '-default' }; $build = Join-Path $root "build\libheif\$Configuration$suffix"; $install = Join-Path $root "build\libheif\install\$Configuration"; $prefix = (Join-Path $root "build\libde265\install\$Configuration") + ';' + (Join-Path $root "build\aom\install\$Configuration")
$treeArguments = @{ DependencyName = 'libheif'; BuildDirectory = $build; InstallDirectory = $install; Vs = $vs; PlatformToolset = $PlatformToolset }
& (Join-Path $PSScriptRoot 'CmakeBuildTree.ps1') -Action Prepare @treeArguments
# FBE only reads HEIC/AVIF through libheif.  x264 is an AVC encoder, while
# libsharpyuv implements high-quality RGB->YCbCr conversion for encoding;
# neither participates in heif_decode_image(... RGB/RGBA ...) used by FBE.
# Explicitly disable both so CMake does not probe optional dependencies or add
# a needless static library to the image-import path.
& $cmake -S $source -B $build -G $vs.Generator -A Win32 -T $vs.GeneratorToolset "-DCMAKE_GENERATOR_INSTANCE=$($vs.GeneratorInstance)" "-DCMAKE_INSTALL_PREFIX=$install" '-DCMAKE_SYSTEM_VERSION=6.1' '-DCMAKE_CXX_STANDARD=20' '-DCMAKE_CXX_STANDARD_REQUIRED=ON' '-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded$<$<CONFIG:Debug>:Debug>' '-DCMAKE_CXX_FLAGS_DEBUG=/Od /RTC1 /Zi /FS /EHsc /DLIBDE265_STATIC_BUILD /DWINVER=0x0601 /D_WIN32_WINNT=0x0601' '-DCMAKE_C_FLAGS_RELEASE=/MT /O2 /Ob2 /DNDEBUG /DWINVER=0x0601 /D_WIN32_WINNT=0x0601' '-DCMAKE_CXX_FLAGS_RELEASE=/MT /O2 /Ob2 /DNDEBUG /EHsc /DLIBDE265_STATIC_BUILD /DWINVER=0x0601 /D_WIN32_WINNT=0x0601' "-DCMAKE_PREFIX_PATH=$prefix" -DBUILD_SHARED_LIBS=OFF -DBUILD_TESTING=OFF -DWITH_EXAMPLES=OFF -DENABLE_PLUGIN_LOADING=OFF -DWITH_LIBDE265=ON -DWITH_LIBDE265_PLUGIN=OFF -DWITH_AOM_DECODER=ON -DWITH_AOM_DECODER_PLUGIN=OFF -DWITH_AOM_ENCODER=OFF -DWITH_X264=OFF -DWITH_X265=OFF -DWITH_LIBSHARPYUV=OFF -DWITH_JPEG_DECODER=OFF -DWITH_JPEG_ENCODER=OFF -DWITH_OpenJPEG_DECODER=OFF -DWITH_OpenJPEG_ENCODER=OFF -DWITH_FFMPEG_DECODER=OFF -DWITH_UNCOMPRESSED_CODEC=OFF
if ($LASTEXITCODE) { throw "libheif configuration failed with exit code $LASTEXITCODE." }; & (Join-Path $PSScriptRoot 'CmakeBuildTree.ps1') -Action Write @treeArguments; & $cmake --build $build --config $Configuration --target INSTALL
if ($LASTEXITCODE) { throw "libheif build failed with exit code $LASTEXITCODE." }
