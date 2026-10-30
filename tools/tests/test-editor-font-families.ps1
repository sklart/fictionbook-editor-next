$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
. (Join-Path $root 'tools\build\Import-VsDevEnvironment.ps1') -Arch x86 -HostArch x64 -PlatformToolset v143 -VcVarsVersion 14.44
$output = Join-Path $root 'build\editor-font-test'
New-Item -ItemType Directory -Path $output -Force | Out-Null
& cl.exe /nologo /EHsc /std:c++17 /DUNICODE /D_UNICODE "/I$root\third_party\wtl" (Join-Path $PSScriptRoot 'editor-font-families-harness.cpp') "/Fo:$output\test.obj" "/Fe:$output\test.exe" /link user32.lib gdi32.lib comctl32.lib
if ($LASTEXITCODE -ne 0) { throw 'Font families harness did not compile.' }
& (Join-Path $output 'test.exe')
if ($LASTEXITCODE -ne 0) { throw 'Font families cache or selection validation failed.' }
Write-Host 'Font enumeration scope, selection isolation and refresh: PASS.'
