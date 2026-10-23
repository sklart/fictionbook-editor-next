[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
. (Join-Path $root 'tools\build\Import-VsDevEnvironment.ps1') -Arch x86 -HostArch x64 -PlatformToolset v143 -VcVarsVersion 14.44
$out = Join-Path $root ('out\tests\p2-resource-safety\' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $out -Force | Out-Null
$resource = Join-Path $out 'contributors.rc'
$contributors = (Join-Path $root 'runtime\contributors.txt').Replace('\', '\\')
[IO.File]::WriteAllText($resource, "ABOUT_FILE ABOUT_FILE `"$contributors`"`r`n", [Text.UTF8Encoding]::new($false))
& rc.exe /nologo ("/fo" + (Join-Path $out 'contributors.res')) $resource
if ($LASTEXITCODE -ne 0) { throw 'Contributors fixture compilation failed.' }
$exe = Join-Path $out 'p2-resource-safety-test.exe'
& cl.exe /nologo /EHsc /std:c++17 /MT /O2 /GL /W4 /WX /utf-8 /Zc:wchar_t- /DUNICODE /D_UNICODE /DWINVER=0x0601 /D_WIN32_WINNT=0x0601 `
    ("/Fo" + (Join-Path $out 'harness.obj')) "/Fe$exe" (Join-Path $PSScriptRoot 'p2-resource-safety-test.cpp') `
    (Join-Path $out 'contributors.res') /link /LTCG /SUBSYSTEM:CONSOLE,6.01 user32.lib gdi32.lib comctl32.lib ole32.lib shell32.lib uuid.lib
if ($LASTEXITCODE -ne 0) { throw 'P2 safety test compilation failed.' }
& $exe 2>&1 | Tee-Object -FilePath (Join-Path $out 'result.txt')
$code = $LASTEXITCODE
@{ ExitCode=$code; VCToolsVersion=$env:VCToolsVersion; ContributorsSHA256=(Get-FileHash (Join-Path $root 'runtime\contributors.txt')).Hash } |
    ConvertTo-Json | Set-Content (Join-Path $out 'result.json') -Encoding utf8
Write-Host "P2 safety test artifacts: $out"
if ($code -ne 0) { throw "P2 safety tests failed: $code" }
