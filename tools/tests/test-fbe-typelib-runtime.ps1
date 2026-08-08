[CmdletBinding()]
param([string]$Configuration = 'Release')
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
& (Join-Path $root 'tools\build\Import-VsDevEnvironment.ps1') -Arch x86 -HostArch x64
$out = Join-Path $root 'out\tests'; New-Item -ItemType Directory -Path $out -Force | Out-Null
$exe = Join-Path $out 'fbe-typelib-runtime.exe'
& cl.exe /nologo /EHsc /std:c++17 /MT (Join-Path $PSScriptRoot 'fbe-typelib-runtime.cpp') "/Fo:$out\fbe-typelib-runtime.obj" "/Fd:$out\fbe-typelib-runtime.pdb" /link oleaut32.lib "/OUT:$exe"
if($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $exe (Join-Path $root "out\$Configuration\FBE.exe")
if($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
