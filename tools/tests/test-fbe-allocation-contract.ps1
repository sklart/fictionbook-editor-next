[CmdletBinding()]
param(
    [string]$AllocatorSource,
    [switch]$ExpectFailure
)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
if (-not $AllocatorSource) { $AllocatorSource = Join-Path $root 'src\fbe\MemoryAllocation.cpp' }
$AllocatorSource = (Resolve-Path -LiteralPath $AllocatorSource).Path
. (Join-Path $root 'tools\build\Import-VsDevEnvironment.ps1') -Arch x86 -HostArch x64 -PlatformToolset v143 -VcVarsVersion 14.44
$out = Join-Path $root ('out\tests\allocation-contract\' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $out -Force | Out-Null
$allocator = Join-Path $out 'allocator.obj'
$harness = Join-Path $out 'harness.obj'
$exe = Join-Path $out 'allocation-contract-test.exe'
$common = @('/nologo','/c','/EHsc','/std:c++17','/MT','/O2','/GL','/W4','/WX','/utf-8','/Zc:wchar_t-','/DWINVER=0x0601','/D_WIN32_WINNT=0x0601')
& cl.exe @common ("/FI" + (Join-Path $PSScriptRoot 'allocation-fault-api.h')) "/Fo$allocator" $AllocatorSource
if ($LASTEXITCODE -ne 0) { throw 'Allocator compilation failed' }
& cl.exe @common "/Fo$harness" (Join-Path $PSScriptRoot 'allocation-contract-test.cpp')
if ($LASTEXITCODE -ne 0) { throw 'Allocation test compilation failed' }
& link.exe /nologo /LTCG /SUBSYSTEM:CONSOLE,6.01 "/OUT:$exe" $allocator $harness
if ($LASTEXITCODE -ne 0) { throw 'Allocation test link failed' }
& $exe 2>&1 | Tee-Object -FilePath (Join-Path $out 'result.txt')
$code = $LASTEXITCODE
@{ Source=$AllocatorSource; SourceSHA256=(Get-FileHash $AllocatorSource).Hash; ExitCode=$code; ExpectedFailure=[bool]$ExpectFailure; VCToolsVersion=$env:VCToolsVersion } |
    ConvertTo-Json | Set-Content (Join-Path $out 'result.json') -Encoding utf8
Write-Host "Allocation contract artifacts: $out"
if ($ExpectFailure) {
    if ($code -ne 1) { throw "Expected regression exit 1, got $code" }
    Write-Host 'Original allocator regression reproduced.'
} elseif ($code -ne 0) { throw "Allocation contract failed with exit $code" }
