[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
& (Join-Path $root 'tools\build\Import-VsDevEnvironment.ps1') -Arch x86 -HostArch x64 -PlatformToolset v143 -VcVarsVersion 14.44
$out = Join-Path $root ('out\tests\archive-metadata-numbers\' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $out -Force | Out-Null
$exe = Join-Path $out 'archive-metadata-numbers-test.exe'
& cl.exe /nologo /EHsc /std:c++17 /MT /W4 /WX /utf-8 /Zc:wchar_t- /DUNICODE /D_UNICODE `
    "/external:I$root\third_party\wtl" /external:W0 "/I$root\src\fbe" "/Fo$out\\" `
    (Join-Path $PSScriptRoot 'archive-metadata-numbers-test.cpp') `
    (Join-Path $root 'src\fbe\document\ArchiveRecentDocuments.cpp') `
    (Join-Path $root 'src\fbe\recovery\RecoveryStore.cpp') `
    /link /SUBSYSTEM:CONSOLE ole32.lib oleaut32.lib shell32.lib "/OUT:$exe"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $exe --installed $out
exit $LASTEXITCODE
