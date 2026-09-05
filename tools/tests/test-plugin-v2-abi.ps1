[CmdletBinding()] param([string]$Configuration = 'Release')
$ErrorActionPreference = 'Stop'; $root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
& (Join-Path $root 'tools\build\Import-VsDevEnvironment.ps1') -Arch x86 -HostArch x64 -PlatformToolset v143
$out = Join-Path $root "out\tests\plugin-v2\Win32\$Configuration"; New-Item -ItemType Directory -Force $out | Out-Null
& cl.exe /nologo /EHsc /std:c++14 /DUNICODE /D_UNICODE /LD (Join-Path $PSScriptRoot 'plugin-v2-fixture.cpp') (Join-Path $root 'src\fbe\FBE_i.c') /link oleaut32.lib "/OUT:$out\plugin-v2-fixture.dll"
if ($LASTEXITCODE -ne 0) { throw 'Synthetic Plugin API v2 fixture did not compile.' }
& cl.exe /nologo /EHsc /std:c++14 /DUNICODE /D_UNICODE (Join-Path $PSScriptRoot 'plugin-v2-abi-harness.cpp') (Join-Path $root 'src\fbe\FBE_i.c') /link ole32.lib oleaut32.lib "/OUT:$out\plugin-v2-abi-harness.exe"
if ($LASTEXITCODE -ne 0) { throw 'Plugin API v2 ABI harness did not compile.' }
& "$out\plugin-v2-abi-harness.exe" "$out\plugin-v2-fixture.dll"
if ($LASTEXITCODE -ne 0) { throw "Plugin API v2 ABI harness failed with exit code $LASTEXITCODE." }
