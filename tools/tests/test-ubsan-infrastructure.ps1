[CmdletBinding()]
param([string]$LlvmRoot)
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$toolchain=& (Join-Path $root 'tools/build/Resolve-UbsanToolchain.ps1') -LlvmRoot $LlvmRoot
. (Join-Path $root 'tools/build/Import-VsDevEnvironment.ps1') -Arch x86 -HostArch x64 -PlatformToolset v143 -VcVarsVersion 14.44
. (Join-Path $PSScriptRoot 'SanitizerHarness.ps1')
$out=Join-Path $root ('out/tests/ubsan/self-test-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory $out | Out-Null
$source=Join-Path $out 'probe.cpp'; $exe=Join-Path $out 'ubsan-probe.exe'
@'
#include <climits>
#include "__WINDOW_RECTANGLE_HEADER__"
#include <windows.h>
volatile int sink;
__declspec(noinline) bool __stdcall Callback(bool value) { return value; }
typedef bool (__stdcall *CallbackType)(bool);
CallbackType volatile callback = Callback;
__declspec(noinline) int __stdcall WrongCallback(int value) { return value; }
__declspec(noinline) int Add(int value) { return value + 1; }
__declspec(noinline) int Shift(unsigned value) { return 1 << value; }
__declspec(noinline) int Load(int* value) { return *value; }
int main(int argc, char** argv) {
    const char mode=argc>1 ? argv[1][0] : 'c';
    if(mode=='d') { ::Sleep(10000); return 0; }
    if(mode=='c') { sink=Add(1); return sink==2 && callback(true) && WindowRectangle::Span(0, CW_USEDEFAULT)==CW_USEDEFAULT ? 0 : 2; }
    if(mode=='o') sink=Add(INT_MAX);
    if(mode=='s') sink=Shift(32);
    if(mode=='a') { alignas(8) char bytes[16]={}; sink=Load(reinterpret_cast<int*>(bytes+1)); }
    if(mode=='n') sink=Load(nullptr);
    if(mode=='f') { callback=reinterpret_cast<CallbackType>(WrongCallback); sink=callback(true); }
    return 0;
}
'@.Replace('__WINDOW_RECTANGLE_HEADER__', (Join-Path $root 'src/fbe/utils/WindowRectangle.h').Replace('\','/')) | Set-Content $source -Encoding utf8
$toolchain | Add-Member -NotePropertyName ignorelistPath -NotePropertyValue (Join-Path $root 'tools/msbuild/ubsan.ignorelist')
$toolchain | Add-Member -NotePropertyName atlProbeSource -NotePropertyValue (Join-Path $PSScriptRoot 'ubsan-atl-default-test.cpp')
$toolchain | Add-Member -NotePropertyName paneProbeSource -NotePropertyValue (Join-Path $PSScriptRoot 'pane-close-button-test.cpp')
$toolchain | Add-Member -NotePropertyName wtlIncludeDirectory -NotePropertyValue (Join-Path $root 'third_party/wtl')
$toolchain | Add-Member -NotePropertyName virtualAddressProbeSource -NotePropertyValue (Join-Path $PSScriptRoot 'virtual-address-range-test.cpp')
$toolchain | Add-Member -NotePropertyName borrowedVariantProbeSource -NotePropertyValue (Join-Path $PSScriptRoot 'borrowed-variant-test.cpp')
$toolchain | ConvertTo-Json | Set-Content (Join-Path $out 'toolchain.json')
$worker=Join-Path $out 'worker.ps1'
@'
$ErrorActionPreference='Stop'
$config=Get-Content (Join-Path $PSScriptRoot 'toolchain.json') -Raw|ConvertFrom-Json
& $config.compilerPath --target=i686-pc-windows-msvc -fuse-ld=lld /guard:cf /nologo /EHsc /std:c++17 /MT /O1 /Zi ("-fsanitize-ignorelist="+$config.ignorelistPath) -fsanitize=undefined -fno-sanitize-recover=undefined /clang:-fno-omit-frame-pointer /clang:-fno-sanitize-merge (Join-Path $PSScriptRoot 'probe.cpp') ("/Fo"+$PSScriptRoot+'/probe.obj') ("/Fe"+$PSScriptRoot+'/ubsan-probe.exe') /link /guard:cf /DEBUG /INCREMENTAL:NO
if($LASTEXITCODE){exit $LASTEXITCODE}
& (Join-Path $PSScriptRoot 'ubsan-probe.exe') clean
exit $LASTEXITCODE
'@ | Set-Content $worker -Encoding utf8
$oldSymbolizer=$env:UBSAN_SYMBOLIZER_PATH
try {
    $env:UBSAN_SYMBOLIZER_PATH=$toolchain.symbolizerPath
    $clean=Invoke-SanitizerCase -Sanitizer Undefined -Name 'infrastructure.clean' -Worker $worker -Directory (Join-Path $out clean) -Executable $exe -TimeoutSeconds 90
    if($clean.status -ne 'PASS' -or -not @($clean.allProcesses|Where-Object Name -eq 'clang-cl.exe').Count -or -not @($clean.processes|Where-Object Instrumented).Count) { throw 'Clean freshly compiled clang-cl UBSan probe or instrumentation observation failed.' }
    $findings=@()
    foreach($case in @(@{name='overflow';pattern='signed integer overflow'},@{name='shift';pattern='shift exponent'},@{name='alignment';pattern='misaligned address'},@{name='null';pattern='null pointer'},@{name='function';pattern='incorrect function type'})) {
        [IO.File]::WriteAllText($worker,"& `"$exe`" $($case.name)`r`nexit 0`r`n",[Text.UTF8Encoding]::new($true))
        $result=Invoke-SanitizerCase -Sanitizer Undefined -Name ('infrastructure.'+$case.name) -Worker $worker -Directory (Join-Path $out $case.name) -Executable $exe -TimeoutSeconds 60
        $diagnostic=($result.stackTraceFiles|ForEach-Object {Get-Content $_ -Raw}) -join "`n"
        if($result.status -ne 'FAIL' -or $result.exitCode -ne 0 -or -not $result.childFailures.Count -or $diagnostic -notmatch $case.pattern -or $diagnostic -notmatch 'probe.cpp:\d+' -or $diagnostic -notmatch '#0.*(Add|Shift|Load|main)') { throw "Missing fail-closed symbolized $($case.name) diagnostic." }
        $findings+=@($result)
    }
    # A recovering UB exits zero; diagnostics must independently reject it.
    @'
$config=Get-Content (Join-Path $PSScriptRoot 'toolchain.json') -Raw|ConvertFrom-Json
& $config.compilerPath --target=i686-pc-windows-msvc -fuse-ld=lld /guard:cf /nologo /EHsc /std:c++17 /MT /O1 /Zi ("-fsanitize-ignorelist="+$config.ignorelistPath) -fsanitize=undefined /clang:-fno-omit-frame-pointer /clang:-fno-sanitize-merge (Join-Path $PSScriptRoot 'probe.cpp') ("/Fo"+$PSScriptRoot+'/recover.obj') ("/Fe"+$PSScriptRoot+'/recover.exe') /link /guard:cf /DEBUG /INCREMENTAL:NO
if($LASTEXITCODE){exit $LASTEXITCODE}
$env:UBSAN_OPTIONS=$env:UBSAN_OPTIONS.Replace('halt_on_error=1','halt_on_error=0')
& (Join-Path $PSScriptRoot 'recover.exe') overflow
exit 0
'@ | Set-Content $worker -Encoding utf8
    $recover=Invoke-SanitizerCase -Sanitizer Undefined -Name 'infrastructure.zero-exit-diagnostic' -Worker $worker -Directory (Join-Path $out recover) -Executable (Join-Path $out recover.exe) -TimeoutSeconds 90
    if($recover.status -ne 'FAIL' -or $recover.exitCode -ne 0 -or $recover.childFailures.Count -or -not $recover.stackTraceFiles.Count -or $recover.timedOut -or $recover.observationError) { throw 'Zero-exit UBSan diagnostic was lost or another failure masked classification.' }
    $helper=Join-Path $out 'helper-route.exe'
    $helperObject=Join-Path $out 'helper-route.obj'
    $quote={param([string]$value) "'"+$value.Replace("'","''")+"'"}
    $command='& cl.exe /nologo /EHsc /std:c++17 /MD /GL /guard:cf /O1 /Zi '+(& $quote $source)+' '+(& $quote ("/Fo:"+$helperObject))+' '+(& $quote ("/Fe:"+$helper))+' /link /guard:cf /DEBUG /INCREMENTAL:NO'+"`nif(`$LASTEXITCODE){throw 'Helper compilation failed'}`n& "+(& $quote $helper)+" clean`nif(`$LASTEXITCODE){throw 'Helper clean branch failed'}"
    $caseFile=Join-Path $out 'helper-case.json'
    @{sanitizer='Undefined';clangClPath=$toolchain.compilerPath;outputDirectory=$out;command=$command;helperOptions=@(('-fsanitize-ignorelist='+$toolchain.ignorelistPath),'-fuse-ld=lld','-fsanitize=undefined','-fno-sanitize-recover=undefined','/clang:-fno-omit-frame-pointer','/clang:-fno-sanitize-merge')} | ConvertTo-Json | Set-Content $caseFile
    $helperResult=Invoke-SanitizerCase -Sanitizer Undefined -Name 'infrastructure.worker-compiler-routing' -Worker (Join-Path $PSScriptRoot 'SanitizerWorker.ps1') -WorkerArguments @('-CaseFile',$caseFile) -Directory (Join-Path $out 'helper-routing') -Executable $helper -NativeNames @('helper-route.exe') -TimeoutSeconds 90
    if($helperResult.status -ne 'PASS' -or -not @($helperResult.processes|Where-Object { $_.Name -eq 'helper-route.exe' -and $_.Instrumented }).Count -or -not @($helperResult.allProcesses|Where-Object { $_.Name -eq 'clang-cl.exe' -and $_.Executable -eq $toolchain.compilerPath }).Count) { throw 'Explicit compiler options through the actual sanitizer worker were not instrumented.' }
    # The exact vendor-function exception must not suppress an adjacent own UB.
    $atlExe=Join-Path $out 'atl-default.exe'
    @'
$config=Get-Content (Join-Path $PSScriptRoot 'toolchain.json') -Raw|ConvertFrom-Json
& $config.compilerPath --target=i686-pc-windows-msvc -fuse-ld=lld /guard:cf /nologo /EHsc /std:c++17 /MT /O2 /Zi /DUNICODE /D_UNICODE /Zc:wchar_t- -fsanitize=undefined -fno-sanitize-recover=undefined ("-fsanitize-ignorelist="+$config.ignorelistPath) /clang:-fno-omit-frame-pointer /clang:-fno-sanitize-merge $config.atlProbeSource ("/Fo"+$PSScriptRoot+'/atl-default.obj') ("/Fe"+$PSScriptRoot+'/atl-default.exe') /link /guard:cf /DEBUG /INCREMENTAL:NO ole32.lib oleaut32.lib user32.lib gdi32.lib comctl32.lib uuid.lib
if($LASTEXITCODE){exit $LASTEXITCODE}
& (Join-Path $PSScriptRoot 'atl-default.exe')
if($LASTEXITCODE){exit $LASTEXITCODE}
& $config.compilerPath --target=i686-pc-windows-msvc -fuse-ld=lld /guard:cf /nologo /EHsc /std:c++17 /MT /O2 /Zi /DUNICODE /D_UNICODE /Zc:wchar_t- -fsanitize=undefined -fno-sanitize-recover=undefined ("-fsanitize-ignorelist="+$config.ignorelistPath) /clang:-fno-omit-frame-pointer /clang:-fno-sanitize-merge ("/I"+$config.wtlIncludeDirectory) $config.paneProbeSource ("/Fo"+$PSScriptRoot+'/pane-close-button.obj') ("/Fe"+$PSScriptRoot+'/pane-close-button.exe') /link /guard:cf /DEBUG /INCREMENTAL:NO ole32.lib oleaut32.lib user32.lib gdi32.lib comctl32.lib uuid.lib
if($LASTEXITCODE){exit $LASTEXITCODE}
& (Join-Path $PSScriptRoot 'pane-close-button.exe')
if($LASTEXITCODE){exit $LASTEXITCODE}
& $config.compilerPath --target=i686-pc-windows-msvc -fuse-ld=lld /guard:cf /nologo /EHsc /std:c++17 /MT /O2 /Zi -fsanitize=undefined -fno-sanitize-recover=undefined ("-fsanitize-ignorelist="+$config.ignorelistPath) /clang:-fno-omit-frame-pointer /clang:-fno-sanitize-merge $config.virtualAddressProbeSource ("/Fo"+$PSScriptRoot+'/virtual-address-range.obj') ("/Fe"+$PSScriptRoot+'/virtual-address-range.exe') /link /guard:cf /DEBUG /INCREMENTAL:NO
if($LASTEXITCODE){exit $LASTEXITCODE}
& (Join-Path $PSScriptRoot 'virtual-address-range.exe')
if($LASTEXITCODE){exit $LASTEXITCODE}
& $config.compilerPath --target=i686-pc-windows-msvc -fuse-ld=lld /guard:cf /nologo /EHsc /std:c++17 /MT /O2 /Zi -fsanitize=undefined -fno-sanitize-recover=undefined ("-fsanitize-ignorelist="+$config.ignorelistPath) /clang:-fno-omit-frame-pointer /clang:-fno-sanitize-merge $config.borrowedVariantProbeSource ("/Fo"+$PSScriptRoot+'/borrowed-variant.obj') ("/Fe"+$PSScriptRoot+'/borrowed-variant.exe') /link /guard:cf /DEBUG /INCREMENTAL:NO oleaut32.lib
if($LASTEXITCODE){exit $LASTEXITCODE}
& (Join-Path $PSScriptRoot 'borrowed-variant.exe')
exit $LASTEXITCODE
'@ | Set-Content $worker -Encoding utf8
    $atlClean=Invoke-SanitizerCase -Sanitizer Undefined -Name 'infrastructure.atl-default-exact-exception' -Worker $worker -Directory (Join-Path $out 'atl-clean') -Executable $atlExe -TimeoutSeconds 90
    if($atlClean.status -ne 'PASS' -or -not @($atlClean.processes|Where-Object Instrumented).Count) { throw 'Exact ATL exception did not permit the real SDK default frame.' }
    if(-not @($atlClean.processes|Where-Object {$_.Name -eq 'pane-close-button.exe' -and $_.Instrumented}).Count) { throw 'The actual WTL pane close-button regression was not instrumented.' }
    if(-not @($atlClean.processes|Where-Object {$_.Name -eq 'virtual-address-range.exe' -and $_.Instrumented}).Count) { throw 'Virtual address range regression was not instrumented.' }
    if(-not @($atlClean.processes|Where-Object {$_.Name -eq 'borrowed-variant.exe' -and $_.Instrumented}).Count) { throw 'Borrowed VARIANT regression was not instrumented.' }
    [IO.File]::WriteAllText($worker,"& `"$atlExe`" own`r`nexit 0`r`n",[Text.UTF8Encoding]::new($true))
    $atlOwn=Invoke-SanitizerCase -Sanitizer Undefined -Name 'infrastructure.atl-exception-own-overflow' -Worker $worker -Directory (Join-Path $out 'atl-own-overflow') -Executable $atlExe -TimeoutSeconds 60
    $atlOwnText=($atlOwn.stackTraceFiles|ForEach-Object {Get-Content -LiteralPath $_ -Raw}) -join "`n"
    if($atlOwn.status -ne 'FAIL' -or $atlOwn.exitCode -ne 0 -or -not $atlOwn.childFailures.Count -or $atlOwnText -notmatch 'signed integer overflow' -or $atlOwnText -notmatch 'OwnOverflow' -or $atlOwnText -notmatch 'ubsan-atl-default-test.cpp:\d+') { throw 'ATL exception concealed own signed overflow.' }
    # A similarly named process outside the selected compiler directories is
    # never treated as a cleanup candidate, even when its parent compiled code.
    $lookalike=Join-Path $out 'mspdbsrv.exe'
    # Deliberately uninstrumented negative fixture, never a product output.
    $negativeCompile='& '+(& $quote $toolchain.compilerPath)+' --target=i686-pc-windows-msvc -fuse-ld=lld /guard:cf /nologo /EHsc /std:c++17 /MT /O1 '+(& $quote $source)+' '+(& $quote ("/Fo"+(Join-Path $out 'lookalike.obj')))+' '+(& $quote ("/Fe"+$lookalike))+' /link /INCREMENTAL:NO'
    ($negativeCompile+"`nif(`$LASTEXITCODE){exit `$LASTEXITCODE}`nStart-Process -FilePath "+(& $quote $lookalike)+" -ArgumentList daemon | Out-Null`nexit 0") | Set-Content $worker
    $lookalikeResult=Invoke-SanitizerCase -Sanitizer Undefined -Name 'infrastructure.unrelated-daemon' -Worker $worker -Directory (Join-Path $out 'unrelated-daemon') -Executable $lookalike -TimeoutSeconds 10
    if($lookalikeResult.status -ne 'FAIL' -or -not $lookalikeResult.timedOut -or $lookalikeResult.infrastructureCleanup.Count -or -not @($lookalikeResult.allProcesses|Where-Object { $_.Executable -eq $lookalike -and -not $_.Instrumented }).Count) { throw 'Unrelated daemon was incorrectly accepted or cleaned as compiler infrastructure.' }
    @{status='PASS';toolchain=$toolchain;clean=$clean;intentionalFindings=$findings;zeroExitDiagnostic=$recover;helperRouting=$helperResult;atlExceptionClean=$atlClean;atlExceptionOwnOverflow=$atlOwn;ignorelistSha256=(Get-FileHash -LiteralPath $toolchain.ignorelistPath).Hash;unrelatedDaemon=$lookalikeResult;extraChecks=$false} | ConvertTo-Json -Depth 10 | Set-Content (Join-Path $out summary.json) -Encoding utf8
    Write-Host "PASS: clang-cl Win32, clean probe, overflow/shift/alignment/null source stacks and zero-exit diagnostic classification. $out"
} finally { $env:UBSAN_SYMBOLIZER_PATH=$oldSymbolizer }
