[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
. (Join-Path $root 'tools/build/Import-VsDevEnvironment.ps1') -Arch x86 -HostArch x64 -PlatformToolset v143 -VcVarsVersion 14.44
. (Join-Path $PSScriptRoot 'SanitizerHarness.ps1')
$out=Join-Path $root ('out/tests/asan/self-test-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Force $out | Out-Null
$source=Join-Path $out 'probe.cpp'
@'
#ifndef __SANITIZE_ADDRESS__
#error AddressSanitizer is required
#endif
#include <cstdlib>
#include <new>
__declspec(noinline) void Release(int* block) {
#ifdef FBE_ASAN_PROCESS_HEAP
    ::operator delete(block);
#else
    std::free(block);
#endif
}
int main(int argc, char** argv) {
    if(argc>1 && argv[1][0]!='c') return 13;
#ifdef FBE_ASAN_PROCESS_HEAP
    int* block=static_cast<int*>(::operator new(32));
#else
    int* block=static_cast<int*>(std::malloc(32));
#endif
    if(!block) return 2;
    block[0]=argc;
    Release(block);
    if(argc>1) return 0;
    volatile int value=block[argc-1]; // Deliberate self-test UAF; never linked into FBE.
    return value;
}
'@ | Set-Content $source -Encoding utf8
$exe=Join-Path $out 'asan-probe.exe'
$runtime=Join-Path $env:VCToolsInstallDir 'bin/HostX64/x86/clang_rt.asan_dynamic-i386.dll'
Copy-Item -LiteralPath $runtime -Destination $out
Copy-Item -LiteralPath (Join-Path $env:VCToolsInstallDir 'bin/HostX64/x86/llvm-symbolizer.exe') -Destination $out
$worker=Join-Path $out 'masked-child.ps1'
@{source=$source;exe=$exe;directory=$out} | ConvertTo-Json | Set-Content (Join-Path $out 'compiler.json') -Encoding utf8
@'
$config=Get-Content -Raw (Join-Path $PSScriptRoot 'compiler.json') | ConvertFrom-Json
& cl.exe /nologo /EHsc /std:c++17 /MT /O2 /GL /Zi /fsanitize=address $config.source ("/Fo"+$config.directory+'/probe.obj') ("/Fe"+$config.exe) /link /DEBUG ("/PDB:"+$config.directory+'/probe.pdb') /INCREMENTAL:NO
if($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $config.exe clean
exit $LASTEXITCODE
'@ | Set-Content $worker -Encoding utf8
$clean=Invoke-SanitizerCase -Name 'infrastructure.clean' -Worker $worker -Directory (Join-Path $out 'clean') -Executable $exe -TimeoutSeconds 60
if($clean.status -ne 'PASS') { throw 'Clean instrumented probe did not pass complete process observation.' }
if(-not @($clean.allProcesses | Where-Object Name -eq 'cl.exe').Count -or -not @($clean.processes | Where-Object Instrumented).Count) { throw 'Compiler and freshly built instrumented child were not observed.' }
[IO.File]::WriteAllText($worker, "& `"$exe`" silent`r`nexit 0`r`n", [Text.UTF8Encoding]::new($true))
$silent=Invoke-SanitizerCase -Name 'infrastructure.masked-exit' -Worker $worker -Directory (Join-Path $out 'silent') -Executable $exe -TimeoutSeconds 60
if($silent.status -ne 'FAIL' -or $silent.exitCode -ne 0 -or -not @($silent.childFailures | Where-Object ExitCode -eq 13).Count) { throw 'Masked nonzero native exit was lost.' }
[IO.File]::WriteAllText($worker, "& `"$exe`"`r`nexit 0`r`n", [Text.UTF8Encoding]::new($true))
$result=Invoke-SanitizerCase -Name 'infrastructure.masked-uaf' -Worker $worker -Directory (Join-Path $out 'diagnostics') -Executable $exe -NativeNames @('asan-probe.exe') -TimeoutSeconds 60
if($result.status -ne 'FAIL' -or $result.exitCode -ne 0 -or -not $result.childFailures.Count -or -not $result.stackTraceFiles.Count) { throw 'Sanitizer runner failed to reject a masked child UAF.' }
$text=($result.stackTraceFiles | ForEach-Object { Get-Content -Raw -LiteralPath $_ }) -join "`n"
if($text -notmatch 'heap-use-after-free' -or $text -notmatch 'probe.cpp') { throw 'Symbolized ASan UAF diagnostic missing.' }
if(-not $result.dumpPaths.Count) { throw 'ASAN_SAVE_DUMPS did not produce a dump.' }
foreach($dumpPath in $result.dumpPaths) { Remove-Item -LiteralPath $dumpPath -Force }
$allocator=Join-Path $root 'src/fbe/MemoryAllocation.cpp'
# Keep the real allocator as a separate frame in this probe: VC 14.44's
# symbolizer has an unstable inline-PDB reader. Production retains /O2 /GL.
& cl.exe /nologo /c /EHsc /std:c++17 /MT /O2 /Ob0 /GL /Zi /fsanitize=address $allocator "/Fo$out/allocator.obj" "/Fd$out/allocator-compile.pdb"
if($LASTEXITCODE -ne 0) { throw 'FBE allocator probe object compilation failed.' }
$heapExe=Join-Path $out 'asan-probe-heap.exe'
& cl.exe /nologo /EHsc /std:c++17 /MT /O2 /GL /Zi /fsanitize=address /DFBE_ASAN_PROCESS_HEAP $source "$out/allocator.obj" "/Fo$out/heap-probe.obj" "/Fd$out/heap-probe-compile.pdb" "/Fe$heapExe" /link /DEBUG "/PDB:$out/heap-probe.pdb" /INCREMENTAL:NO
if($LASTEXITCODE -ne 0) { throw 'FBE allocator probe compilation failed.' }
[IO.File]::WriteAllText($worker, "& `"$heapExe`"`r`nexit 0`r`n", [Text.UTF8Encoding]::new($true))
$heap=Invoke-SanitizerCase -Name 'infrastructure.fbe-process-heap-uaf' -Worker $worker -Directory (Join-Path $out 'process-heap') -Executable $heapExe -TimeoutSeconds 60 -OfflineSymbolization
$heapText=($heap.stackTraceFiles | ForEach-Object { Get-Content -Raw -LiteralPath $_ }) -join "`n"
if($heap.status -ne 'FAIL' -or $heap.exitCode -ne 0 -or $heapText -notmatch 'heap-use-after-free' -or $heapText -notmatch 'MemoryAllocation.cpp') { throw 'The actual FBE process-heap allocator was not covered by ASan.' }
if(-not @($heap.stackTraceFiles | Where-Object {$_ -like '*.symbolized.log'}).Count) { throw 'Offline symbolization of the actual allocator stack was not exercised.' }
foreach($dumpPath in $heap.dumpPaths) { Remove-Item -LiteralPath $dumpPath -Force }
@{status='PASS';probeResult=$result;clean=$clean;maskedExit=$silent;fbeAllocator=$heap;instrumentationMacro=$true;expectedDumpVerifiedAndRemoved=$true} | ConvertTo-Json -Depth 9 | Set-Content (Join-Path $out 'summary.json') -Encoding utf8
Write-Host "PASS: ASan macro, symbolized UAF, dump, and masked-child FAIL classification. $out"
