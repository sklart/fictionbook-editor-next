[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
. (Join-Path $root 'tools/build/Import-VsDevEnvironment.ps1') -Arch x86 -HostArch x64 -PlatformToolset v143 -VcVarsVersion 14.44
$output=Join-Path $root 'out/tests/status-bar-state';New-Item -ItemType Directory $output -Force | Out-Null
$exe=Join-Path $output 'status-bar-state.exe'
& cl.exe /nologo /EHsc /std:c++17 /W4 /WX /DUNICODE /D_UNICODE /external:W0 /I (Join-Path $root 'src/fbe') "/external:I$(Join-Path $root 'third_party/wtl')" (Join-Path $PSScriptRoot 'status-bar-state-test.cpp') (Join-Path $root 'src/fbe/StatusBarState.cpp') "/Fo$output\\" "/Fe$exe"
if($LASTEXITCODE -ne 0){throw 'Status state compilation failed'}
& $exe
if($LASTEXITCODE -ne 0){throw 'Status state regression failed'}
