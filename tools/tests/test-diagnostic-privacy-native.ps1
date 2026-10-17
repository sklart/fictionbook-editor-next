[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$work=Join-Path $root ('build/diagnostic-privacy-native-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $work | Out-Null
. (Join-Path $root 'tools/build/Import-VsDevEnvironment.ps1') -Arch x86 -HostArch x64 -PlatformToolset v143 -VcVarsVersion 14.44
$source=Join-Path $PSScriptRoot 'diagnostic-privacy-native.cpp'
& cl /nologo /std:c++17 /Zc:wchar_t- /EHsc /MT /W4 /WX /utf-8 $source "/Fo$work/probe.obj" "/Fe$work/probe.exe" /link shell32.lib ole32.lib user32.lib advapi32.lib
if($LASTEXITCODE -ne 0){throw 'Native diagnostic probe compilation failed'}
foreach($copy in 'portable-a','portable-b') {
 $directory=Join-Path $work $copy;New-Item -ItemType Directory -Path $directory | Out-Null
 Copy-Item -LiteralPath (Join-Path $work 'probe.exe') -Destination $directory
 & (Join-Path $directory 'probe.exe') --portable
 if($LASTEXITCODE -ne 0){throw "Portable probe failed: $LASTEXITCODE"}
 $log=Get-Content -LiteralPath (Join-Path $directory 'Data/Diagnostics/fbe-diagnostic.log') -Raw
 if($log -match 'PRIVATE_USER|book.fb2'){throw 'Standalone logger leaked free text'}
 if($log -notmatch 'session_id=unavailable.*FD203.*hr=0x80070005'){throw 'Standalone fallback lacks technical metadata'}
}
Write-Host "PASS: two independent portable copies. Evidence: $work"
