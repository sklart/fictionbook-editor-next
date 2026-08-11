<# Builds the static HEVC decoder used only by libheif. #>
[CmdletBinding()]
param([ValidateSet('Debug','Release')][string]$Configuration = 'Release', [string]$PlatformToolset)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$cmake = Get-ChildItem (Join-Path ${env:ProgramFiles} 'Microsoft Visual Studio\18\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin') -Filter cmake.exe -Recurse | Select-Object -First 1 -ExpandProperty FullName
if (!$cmake) { throw 'Не найден cmake.exe' }
$source = Join-Path $root 'third_party\libde265'; $build = Join-Path $root "build\libde265\$Configuration"; $install = Join-Path $root "build\libde265\install\$Configuration"
& $cmake -S $source -B $build -G 'Visual Studio 17 2022' -A Win32 "-DCMAKE_INSTALL_PREFIX=$install" -DCMAKE_SYSTEM_VERSION=6.1 '-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded$<$<CONFIG:Debug>:Debug>' '-DCMAKE_C_FLAGS_RELEASE=/MT /O2 /Ob2 /DNDEBUG /DWINVER=0x0601 /D_WIN32_WINNT=0x0601' '-DCMAKE_CXX_FLAGS_RELEASE=/MT /O2 /Ob2 /DNDEBUG /DWINVER=0x0601 /D_WIN32_WINNT=0x0601' '-DCMAKE_POLICY_VERSION_MINIMUM=3.5' -DBUILD_SHARED_LIBS=OFF -DENABLE_DECODER=ON -DENABLE_ENCODER=OFF -DENABLE_SDL=OFF
if ($LASTEXITCODE) { exit $LASTEXITCODE }; & $cmake --build $build --config $Configuration --target INSTALL; exit $LASTEXITCODE
