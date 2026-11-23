[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
if($env:GITHUB_ACTIONS -ne 'true' -or $env:RUNNER_ENVIRONMENT -ne 'github-hosted'){throw 'This diagnostic requires a disposable hosted runner.'}
$root=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$out=Join-Path $root 'out/tests/rc3-ubsan-image-ab'
New-Item -ItemType Directory -Force $out | Out-Null
$clang=Join-Path $root 'out/tools/llvm-21.1.8/win32/bin/clang-cl.exe'
& (Join-Path $root 'tools/build/Import-VsDevEnvironment.ps1') -Arch x86 -HostArch x64 -PlatformToolset v143 -VcVarsVersion 14.44
$vs=& (Join-Path $root 'tools/build/Resolve-VsCmake.ps1') -PlatformToolset v143
$ninja=Join-Path $vs.GeneratorInstance 'Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe'
if(!(Test-Path $ninja) -or !(Test-Path $clang)){throw 'Clang or Ninja is missing.'}
$build=Join-Path $root 'build/rc3-ubsan-image-ab/libheif';$install=Join-Path $root 'build/rc3-ubsan-image-ab/install'
$prefix=(Join-Path $root 'build/libde265/install/Release')+';'+(Join-Path $root 'build/aom/install/Release')
& $vs.CMake -S (Join-Path $root 'third_party/libheif') -B $build -G Ninja "-DCMAKE_MAKE_PROGRAM=$ninja" "-DCMAKE_C_COMPILER=$clang" "-DCMAKE_CXX_COMPILER=$clang" '-DCMAKE_BUILD_TYPE=Release' "-DCMAKE_INSTALL_PREFIX=$install" '-DCMAKE_CXX_STANDARD=20' '-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded' '-DCMAKE_C_FLAGS_RELEASE=/MT /O2 /Ob2 /DNDEBUG /DWINVER=0x0601 /D_WIN32_WINNT=0x0601' '-DCMAKE_CXX_FLAGS_RELEASE=/MT /O2 /Ob2 /DNDEBUG /EHsc /DLIBDE265_STATIC_BUILD /DWINVER=0x0601 /D_WIN32_WINNT=0x0601' "-DCMAKE_PREFIX_PATH=$prefix" -DBUILD_SHARED_LIBS=OFF -DBUILD_TESTING=OFF -DWITH_EXAMPLES=OFF -DENABLE_PLUGIN_LOADING=OFF -DWITH_LIBDE265=ON -DWITH_LIBDE265_PLUGIN=OFF -DWITH_AOM_DECODER=ON -DWITH_AOM_DECODER_PLUGIN=OFF -DWITH_AOM_ENCODER=OFF -DWITH_X264=OFF -DWITH_X265=OFF -DWITH_LIBSHARPYUV=OFF -DWITH_JPEG_DECODER=OFF -DWITH_JPEG_ENCODER=OFF -DWITH_OpenJPEG_DECODER=OFF -DWITH_OpenJPEG_ENCODER=OFF -DWITH_FFMPEG_DECODER=OFF -DWITH_OpenH264_DECODER=OFF -DWITH_GDK_PIXBUF=OFF -DWITH_UNCOMPRESSED_CODEC=OFF
if($LASTEXITCODE){throw 'Experimental libheif configure failed.'}
& $vs.CMake --build $build --target install --parallel 2
if($LASTEXITCODE){throw 'Experimental libheif build failed.'}
$source=Join-Path $root 'tools/tests/test-image-import-native.ps1'
$sourceHash=(Get-FileHash $source -Algorithm SHA256).Hash
$text=[IO.File]::ReadAllText($source)
$old='"/LIBPATH:$repoRoot\build\libheif\install\$Configuration\lib"'
$new='"/LIBPATH:$repoRoot\build\rc3-ubsan-image-ab\install\lib"'
if(($text.Split(@($old),[StringSplitOptions]::None).Count-1) -ne 1){throw 'Expected exactly one libheif link path.'}
$temporary=Join-Path (Split-Path $source) ('image-ab-'+[guid]::NewGuid().ToString('N')+'.ps1')
$savedClang=$env:FBE_UBSAN_CLANG;$savedOptions=$env:UBSAN_OPTIONS
$env:FBE_UBSAN_CLANG=$clang
$global:FbeUbsanHelperOptions=@(('-fsanitize-ignorelist='+(Join-Path $root 'tools/msbuild/ubsan.ignorelist')),'-fuse-ld=lld','-fsanitize=undefined','-fno-sanitize-recover=undefined','/clang:-fno-omit-frame-pointer','/clang:-fno-sanitize-merge','/Zi','/DFBE_CLANG_TYPELIBS',('/I'+(Join-Path $root 'build/ubsan/generated/type-libraries')))
$results=[Collections.Generic.List[object]]::new()
try {
    $self=Join-Path $out 'self-test';New-Item -ItemType Directory -Force $self | Out-Null
    $selfSource=Join-Path $self 'probe.cpp';$selfExe=Join-Path $self 'probe.exe'
    [IO.File]::WriteAllText($selfSource,'#include <climits>' + "`n" + 'int main(){volatile int x=INT_MAX;return x+1;}')
    & $clang --target=i686-pc-windows-msvc -fuse-ld=lld -fsanitize=undefined -fno-sanitize-recover=undefined /nologo /MT $selfSource ('/Fo'+(Join-Path $self 'probe.obj')) ('/Fe'+$selfExe)
    if($LASTEXITCODE){throw 'UBSan report self-test did not compile.'}
    $env:UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1:log_path=out/tests/rc3-ubsan-image-ab/self-test/ubsan'
    $probe=Start-Process -FilePath $selfExe -WorkingDirectory $root -WindowStyle Hidden -Wait -PassThru -RedirectStandardError (Join-Path $self 'stderr.log')
    $reports=@(Get-ChildItem $self -Filter 'ubsan.*' -File)
    if($probe.ExitCode -eq 0 -or $reports.Count -ne 1 -or (Get-Content -Raw $reports[0].FullName) -notmatch 'signed integer overflow') {throw 'UBSan report self-test did not capture the expected failure.'}
    foreach($mode in @('stock-msvc','experimental-clang')) {
        $case=Join-Path $out $mode;New-Item -ItemType Directory -Force $case | Out-Null
        $variant=if($mode -eq 'stock-msvc'){$text}else{$text.Replace($old,$new)}
        [IO.File]::WriteAllText($temporary,$variant,[Text.UTF8Encoding]::new($true))
        # Sanitizer options use ':' as a separator, including on Windows;
        # an absolute drive-letter path is parsed as a malformed option.
        # The helper is explicitly launched with $root as working directory.
        $env:UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1:log_path=out/tests/rc3-ubsan-image-ab/'+$mode+'/ubsan'
        $status='PASS';$details=''
        try { & $temporary -Configuration Release -PlatformToolset v143 *>&1 | Out-File (Join-Path $case 'smoke.log') -Encoding utf8 }
        catch {$status='FAIL';$details=$_.Exception.Message}
        $reports=@(Get-ChildItem $case -Filter 'ubsan.*' -File)
        if($reports.Count){$status='FAIL';$details+=' UBSan diagnostics were emitted.'}
        $lib=if($mode -eq 'stock-msvc'){Join-Path $root 'build/libheif/install/Release/lib/heif.lib'}else{Join-Path $install 'lib/heif.lib'}
        $results.Add([pscustomobject]@{mode=$mode;status=$status;details=$details;librarySha256=(Get-FileHash $lib -Algorithm SHA256).Hash;diagnosticCount=$reports.Count})
    }
} finally {
    $env:FBE_UBSAN_CLANG=$savedClang;$env:UBSAN_OPTIONS=$savedOptions
    Remove-Variable FbeUbsanHelperOptions -Scope Global -ErrorAction SilentlyContinue
    if(Test-Path -LiteralPath $temporary){Remove-Item -LiteralPath $temporary -Force}
}
if((Get-FileHash $source -Algorithm SHA256).Hash -cne $sourceHash){throw 'Original test script changed.'}
@{scope='partial A/B diagnostic, not release gate';sourceSha256=$sourceHash;results=@($results)} | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $out 'summary.json') -Encoding utf8
if(@($results|Where-Object status -ne 'PASS').Count){throw 'At least one ImageImport A/B variant failed; inspect evidence.'}
