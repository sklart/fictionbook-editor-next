<# Builds the static AV1 decoder used only by libheif. Generic CPU avoids an external assembler. #>
[CmdletBinding()]
param([ValidateSet('Debug','Release')][string]$Configuration = 'Release', [string]$PlatformToolset)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$vs = & (Join-Path $PSScriptRoot 'Resolve-VsCmake.ps1') -PlatformToolset $PlatformToolset
$cmake = $vs.CMake
$source = Join-Path $root 'third_party\aom'; $suffix = if ($PlatformToolset) { "-$PlatformToolset" } else { '-default' }; $build = Join-Path $root "build\aom\$Configuration$suffix"; $install = Join-Path $root "build\aom\install\$Configuration"
$treeArguments = @{ DependencyName = 'aom'; BuildDirectory = $build; InstallDirectory = $install; Vs = $vs; PlatformToolset = $PlatformToolset }
& (Join-Path $PSScriptRoot 'CmakeBuildTree.ps1') -Action Prepare @treeArguments
& $cmake -S $source -B $build -G $vs.Generator -A Win32 -T $vs.GeneratorToolset "-DCMAKE_GENERATOR_INSTANCE=$($vs.GeneratorInstance)" "-DCMAKE_INSTALL_PREFIX=$install" '-DCMAKE_SYSTEM_VERSION=6.1' '-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded$<$<CONFIG:Debug>:Debug>' '-DCMAKE_C_FLAGS_RELEASE=/MT /O2 /Ob2 /DNDEBUG /DWINVER=0x0601 /D_WIN32_WINNT=0x0601' '-DCMAKE_CXX_FLAGS_RELEASE=/MT /O2 /Ob2 /DNDEBUG /DWINVER=0x0601 /D_WIN32_WINNT=0x0601' -DBUILD_SHARED_LIBS=OFF -DENABLE_TESTS=OFF -DENABLE_EXAMPLES=OFF -DENABLE_TOOLS=OFF -DCONFIG_AV1_ENCODER=0 -DCONFIG_AV1_DECODER=1 -DAOM_TARGET_CPU=generic
if ($LASTEXITCODE) { exit $LASTEXITCODE }; & (Join-Path $PSScriptRoot 'CmakeBuildTree.ps1') -Action Write @treeArguments; & $cmake --build $build --config $Configuration --target INSTALL; exit $LASTEXITCODE
