[CmdletBinding()]
param([Parameter(Mandatory)][string]$DumpPath)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$DumpPath = (Resolve-Path -LiteralPath $DumpPath).Path
& (Join-Path $root 'tools\build\Import-VsDevEnvironment.ps1') -Arch x86 -HostArch x64
$directory = Join-Path $root 'out\tests\minidump-module-lookup'
New-Item -ItemType Directory -Force -Path $directory | Out-Null
$executable = Join-Path $directory 'minidump-module-lookup.exe'
& cl.exe /nologo /EHsc /std:c++14 /utf-8 /DUNICODE /D_UNICODE /W3 (Join-Path $PSScriptRoot 'minidump-module-lookup.cpp') '/link' 'dbghelp.lib' "/OUT:$executable"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $executable $DumpPath
if ($LASTEXITCODE -ne 0) { throw "Не удалось определить модуль из minidump, код $LASTEXITCODE." }
