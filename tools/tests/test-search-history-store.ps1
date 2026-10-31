[CmdletBinding()]
param([string]$PlatformToolset = 'v143')
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
& (Join-Path $root 'tools\build\Import-VsDevEnvironment.ps1') -Arch x86 -HostArch x64 -PlatformToolset $PlatformToolset
$testDir = Join-Path $root 'out\tests\search-history-store'
New-Item -ItemType Directory -Force -Path $testDir | Out-Null
$exe = Join-Path $testDir 'search-history-store.exe'
& cl.exe /nologo /EHsc /std:c++17 /MT /DUNICODE /D_UNICODE /utf-8 `
    "/I$(Join-Path $root 'third_party\wtl')" "/I$(Join-Path $root 'src\fbe')" "/Fo$testDir\\" `
    (Join-Path $PSScriptRoot 'search-history-store.cpp') /link /SUBSYSTEM:CONSOLE shlwapi.lib "/OUT:$exe"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
$file = Join-Path $testDir ("history-{0}.dat" -f [guid]::NewGuid().ToString('N'))
& $exe $file
if ($LASTEXITCODE -ne 0) { throw "Search history test failed: $LASTEXITCODE" }
& $exe $file read
if ($LASTEXITCODE -ne 0) { throw "Search history reload in a new process failed: $LASTEXITCODE" }
Write-Host 'Search history store: PASS.'
