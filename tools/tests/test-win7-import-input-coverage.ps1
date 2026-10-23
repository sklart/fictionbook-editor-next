[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
& (Join-Path $root 'tools/build/Import-VsDevEnvironment.ps1') -Arch x86 -HostArch x64
$fixture=Join-Path $root ('out/tests/win7-input-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path (Join-Path $fixture 'Plugins') -Force | Out-Null
$safeSource=Join-Path $fixture 'safe.cpp'
$modernSource=Join-Path $fixture 'modern.cpp'
[IO.File]::WriteAllText($safeSource,'#include <windows.h>' + "`r`n" + 'extern "C" __declspec(dllexport) DWORD Probe() { return GetCurrentProcessId(); }')
[IO.File]::WriteAllText($modernSource,'#include <windows.h>' + "`r`n" + 'extern "C" __declspec(dllimport) UINT WINAPI GetDpiForWindow(HWND);' + "`r`n" + 'extern "C" __declspec(dllexport) UINT Probe(HWND window) { return GetDpiForWindow(window); }')
Push-Location $fixture
try {
    & cl.exe /nologo /LD /W4 /WX $safeSource /link /OUT:Safe.dll
    if($LASTEXITCODE -ne 0){throw 'Safe PE fixture compilation failed'}
    & cl.exe /nologo /LD /W4 /WX $modernSource user32.lib /link /OUT:Plugins/Modern.dll
    if($LASTEXITCODE -ne 0){throw 'Nested PE fixture compilation failed'}
} finally {Pop-Location}
$checker=Join-Path $PSScriptRoot 'check-win7-imports.ps1'
foreach($case in @(
    @{Names=@('Safe.dll');Fail=$false;Pattern=''},
    @{Names=@('Safe.dll','Plugins/Modern.dll');Fail=$true;Pattern='GetDpiForWindow'},
    @{Names=@('Safe.dll','Plugins/Missing.dll');Fail=$true;Pattern='Requested PE input is missing'}
)) {
    $quoted=@($case.Names | ForEach-Object {"'"+$_.Replace("'","''")+"'"}) -join ','
    $command="& '"+$checker.Replace("'","''")+"' -OutputDirectory '"+$fixture.Replace("'","''")+"' -IncludeNames @($quoted)"
    $output=& (Join-Path $PSHOME 'pwsh.exe') -NoProfile -Command $command 2>&1 | Out-String
    $exitCode=$LASTEXITCODE
    if(($exitCode -ne 0) -ne $case.Fail -or ($case.Pattern -and $output -notmatch $case.Pattern)) {throw "PE selection regression: exit=$exitCode; $output"}
    Write-Host "PE input case $($case.Names -join ','): exit=$exitCode; expected rejection=$($case.Fail)"
}
Write-Host "Windows 7 PE input coverage: PASS; fixtures=$fixture (not executed)."
