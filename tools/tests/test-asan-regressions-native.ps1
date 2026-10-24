[CmdletBinding()]
param([ValidateSet('None','Address')][string]$Sanitizer='None')
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
. (Join-Path $root 'tools/build/Import-VsDevEnvironment.ps1') -Arch x86 -HostArch x64 -PlatformToolset v143 -VcVarsVersion 14.44
$relative=if($Sanitizer -eq 'Address') { 'out/tests/asan/regressions-' } else { 'out/tests/memory-regressions/' }
$out=Join-Path $root ($relative+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Force $out | Out-Null
$flags=@('/nologo','/W4','/WX','/external:env:INCLUDE','/external:W0','/EHsc','/MT','/O2','/GL','/Zi','/std:c++17','/utf-8','/Zc:wchar_t-','/DUNICODE','/D_UNICODE','/DWINVER=0x0601','/D_WIN32_WINNT=0x0601')
if($Sanitizer -eq 'Address') { $flags+=@('/fsanitize=address','/D_DISABLE_VECTOR_ANNOTATION','/D_DISABLE_STRING_ANNOTATION'); Copy-Item (Join-Path $env:VCToolsInstallDir 'bin/HostX64/x86/clang_rt.asan_dynamic-i386.dll') -Destination $out }
$savedDump=$env:ASAN_SAVE_DUMPS; $savedOptions=$env:ASAN_OPTIONS
$results=@(); $status='FAIL'
try {
foreach($name in @('image-data-stream','toolbar-caption-lifetime')) {
    $exe=Join-Path $out ($name+'.exe')
    & cl.exe @flags ("/I"+(Join-Path $root 'third_party/wtl')) ("/Fo"+(Join-Path $out ($name+'.obj'))) ("/Fd"+(Join-Path $out ($name+'-compile.pdb'))) "/Fe$exe" (Join-Path $PSScriptRoot ($name+'-test.cpp')) `
        /link /LTCG /DEBUG /INCREMENTAL:NO /SUBSYSTEM:CONSOLE,6.01 ("/PDB:"+(Join-Path $out ($name+'.pdb'))) /MANIFEST:EMBED ("/MANIFESTINPUT:"+(Join-Path $root 'src/fbe/res/FBE.exe.manifest')) ole32.lib oleaut32.lib user32.lib gdi32.lib gdiplus.lib comctl32.lib shell32.lib uuid.lib
    if($LASTEXITCODE -ne 0) { throw "Regression host compilation failed: $name" }
    if($Sanitizer -eq 'Address') {
        $env:ASAN_SAVE_DUMPS=Join-Path $out ($name+'.dmp')
        $env:ASAN_OPTIONS='halt_on_error=1:abort_on_error=1:windows_hook_rtl_allocators=1:alloc_dealloc_mismatch=1:symbolize=1:symbolize_inline_frames=0'
    }
    & $exe 2>&1 | Tee-Object (Join-Path $out ($name+'.log'))
    $code=$LASTEXITCODE
    $results+=@{name=$name;sanitizer=$Sanitizer;executable=$exe;exitCode=$code;log=(Join-Path $out ($name+'.log'));dumpPaths=@(Get-ChildItem $out -Filter '*.dmp' | ForEach-Object FullName)}
    if($code -ne 0) { throw "Regression host failed: $name" }
}
$status='PASS'
} finally {
    $env:ASAN_SAVE_DUMPS=$savedDump; $env:ASAN_OPTIONS=$savedOptions
    @{status=$status;sanitizer=$Sanitizer;commit=(& git -C $root rev-parse HEAD).Trim();results=$results} | ConvertTo-Json -Depth 6 | Set-Content (Join-Path $out 'summary.json') -Encoding utf8
}
Write-Host "Native memory regressions PASS ($Sanitizer): $out"
