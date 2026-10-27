[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
. (Join-Path $root 'tools\build\Import-VsDevEnvironment.ps1') -PlatformToolset v143 -Arch x86 -HostArch x64
$output = Join-Path $root 'out\tests\container-pane-layout'
New-Item -ItemType Directory -Force $output | Out-Null
$exe = Join-Path $output 'container-pane-layout-test.exe'
& cl.exe /nologo /utf-8 /EHsc /std:c++17 /Zc:wchar_t- /MT /DUNICODE /D_UNICODE /W4 /WX "/I$root\third_party\wtl" "/I$root\src\fbe" "/Fo$output\\" (Join-Path $PSScriptRoot 'container-pane-layout-test.cpp') "/Fe$exe" /link /SUBSYSTEM:CONSOLE user32.lib gdi32.lib comctl32.lib
if ($LASTEXITCODE -ne 0) { throw 'Container pane layout test compilation failed.' }
& $exe
if ($LASTEXITCODE -ne 0) { throw 'Container pane layout test failed.' }
