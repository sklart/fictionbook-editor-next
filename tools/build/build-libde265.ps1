<# Builds the static HEVC decoder used only by libheif. #>
[CmdletBinding()]
param([ValidateSet('Debug','Release')][string]$Configuration = 'Release', [string]$PlatformToolset)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$vs = & (Join-Path $PSScriptRoot 'Resolve-VsCmake.ps1') -PlatformToolset $PlatformToolset
$cmake = $vs.CMake
$source = Join-Path $root 'third_party\libde265'; $suffix = if ($PlatformToolset) { "-$PlatformToolset" } else { '-default' }; $build = Join-Path $root "build\libde265\$Configuration$suffix"; $install = Join-Path $root "build\libde265\install\$Configuration"
& $cmake -S $source -B $build -G $vs.Generator -A Win32 "-DCMAKE_INSTALL_PREFIX=$install" -DCMAKE_SYSTEM_VERSION=6.1 '-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded$<$<CONFIG:Debug>:Debug>' '-DCMAKE_C_FLAGS_RELEASE=/MT /O2 /Ob2 /DNDEBUG /DWINVER=0x0601 /D_WIN32_WINNT=0x0601' '-DCMAKE_CXX_FLAGS_RELEASE=/MT /O2 /Ob2 /DNDEBUG /DWINVER=0x0601 /D_WIN32_WINNT=0x0601' '-DCMAKE_POLICY_VERSION_MINIMUM=3.5' -DBUILD_SHARED_LIBS=OFF -DENABLE_DECODER=ON -DENABLE_ENCODER=OFF -DENABLE_SDL=OFF
if ($LASTEXITCODE) { exit $LASTEXITCODE }; & $cmake --build $build --config $Configuration --target INSTALL; exit $LASTEXITCODE
