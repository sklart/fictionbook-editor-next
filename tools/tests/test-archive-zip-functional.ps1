[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
& (Join-Path $root 'tools\build\Import-VsDevEnvironment.ps1') -Arch x86 -HostArch x64
$out = Join-Path $root 'out\tests'; New-Item -ItemType Directory -Force -Path $out | Out-Null
$exe = Join-Path $out 'archive-zip-functional-test.exe'
$includes = @("/I$root\src\fbe", "/I$root\third_party\wtl", "/I$root\build\libarchive\install\Release\include")
$sources = @("$PSScriptRoot\archive-zip-functional-test.cpp", "$root\src\fbe\archive\ArchiveReader.cpp", "$root\src\fbe\archive\ZipArchiveWriter.cpp")
& cl.exe /nologo /EHsc /std:c++17 /MT /DUNICODE /D_UNICODE /DWIN32 @includes $sources "/Fe$exe" "/link" "/LIBPATH:$root\build\libarchive\install\Release\lib" "/LIBPATH:$root\build\zlib\install\Release\lib" archive.lib zs.lib xmllite.lib
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
Write-Host 'Archive ZIP functional test passed.'
