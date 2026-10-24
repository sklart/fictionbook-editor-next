<# Finalise an isolated diagnostic build; never used by packaging. #>
[CmdletBinding()]
param([Parameter(Mandatory)][string]$OutputDirectory, [ValidateSet('Address','Undefined')][string]$Sanitizer='Address', [string]$LlvmRoot)
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$output=(Resolve-Path -LiteralPath $OutputDirectory).Path
$folder=if($Sanitizer -eq 'Address') {'asan'} else {'ubsan'}
if ($output -ne (Join-Path $root "out\$folder\Release")) { throw 'Unexpected sanitizer output directory.' }
$compilerBin=Join-Path $env:VCToolsInstallDir 'bin\HostX64\x86'
$toolchain=$null
if($Sanitizer -eq 'Address') {
    $runtime=Join-Path $compilerBin 'clang_rt.asan_dynamic-i386.dll'
    if (-not (Test-Path -LiteralPath $runtime)) { throw 'Install the VC 14.44 x86 AddressSanitizer component.' }
    Copy-Item -LiteralPath $runtime -Destination $output -Force
    $symbolizer=Join-Path $compilerBin 'llvm-symbolizer.exe'
} else {
    $toolchain=& (Join-Path $PSScriptRoot 'Resolve-UbsanToolchain.ps1') -LlvmRoot $LlvmRoot
    Write-Host "LLVM version=$($toolchain.llvmVersion); target=$($toolchain.target); architecture=$($toolchain.architecture); runtime=$($toolchain.runtimeLibrary)"
    $symbolizer=$toolchain.symbolizerPath
    $toolchain | ConvertTo-Json | Set-Content (Join-Path $output 'fbe-ubsan-toolchain.json') -Encoding utf8
}
if (Test-Path -LiteralPath $symbolizer) { Copy-Item -LiteralPath $symbolizer -Destination $output -Force }
$modules=@('FBE.exe','FBV.exe','FBShell.dll','Plugins\ExportHTML.dll','Plugins\ExportDOCX.dll','Plugins\ExportEPUB.dll','Plugins\ImportEPUB.dll','Plugins\ImportEPUBLunaSVG.dll','ExportDOCXBatch.exe','ExportEPUBBatch.exe','ImportEPUBBatch.exe')
$coverage=@(foreach ($name in $modules) {
    $path=Join-Path $output $name
    if (-not (Test-Path -LiteralPath $path)) { throw "Missing sanitizer module: $name" }
    $table=if($Sanitizer -eq 'Address') {'/imports'} else {'/exports'}
    $imports=(& (Join-Path $compilerBin 'dumpbin.exe') $table $path) -join "`n"
    $runtimePattern=if($Sanitizer -eq 'Address') {'clang_rt\.asan_dynamic-i386\.dll'} else {'__ubsan_handle_.*_abort'}
    if ($LASTEXITCODE -ne 0 -or $imports -notmatch $runtimePattern) { throw "Sanitizer runtime evidence missing: $name" }
    $symbols=[IO.Path]::ChangeExtension($path,'.pdb')
    if (-not (Test-Path -LiteralPath $symbols)) { throw "Missing sanitizer symbols: $name" }
    @{path=$name;sha256=(Get-FileHash $path).Hash;symbols=[IO.Path]::GetFileName($symbols);instrumented=$true}
})
$commit=(& git -C $root rev-parse HEAD).Trim()
$compileLogName=if($Sanitizer -eq 'Address') {'CL.command.1.tlog'} else {'clang-cl.command.1.tlog'}
$units=@(foreach($log in Get-ChildItem (Join-Path $root "build/$folder/obj") -Recurse -Filter $compileLogName) {
    $lines=[IO.File]::ReadAllLines($log.FullName,[Text.Encoding]::Unicode)
    for($index=0;$index -lt $lines.Length-1;++$index) {
        if(-not $lines[$index].StartsWith('^')) { continue }
        $command=$lines[$index+1]
        $required=if($Sanitizer -eq 'Address') {'(?i)(^|\s)/fsanitize=address(\s|$)'} else {'(^|\s)-fsanitize=undefined(\s|$)'}
        if($command -notmatch $required -or ($Sanitizer -eq 'Undefined' -and $command -notmatch '-fno-sanitize-recover=undefined')) { throw "Uninstrumented translation unit in $($log.FullName): $($lines[$index])" }
        foreach($source in $lines[$index].Substring(1).Split('|')) {
            @{source=$source;commandLog=$log.FullName;sanitizer=$Sanitizer;asan=($Sanitizer -eq 'Address');optimized=($command -match '(?i)/O2|/O1');wholeProgram=($command -match '(?i)/GL')}
        }
    }
})
if($units.Count -lt 200) { throw 'Incomplete sanitizer compilation evidence.' }
if($Sanitizer -eq 'Undefined') {
    $linkLogs=@(Get-ChildItem (Join-Path $root 'build/ubsan/obj') -Recurse -Filter 'lld-link.command.1.tlog')
    if($linkLogs.Count -ne $modules.Count) { throw 'Incomplete LLD link evidence for first-party modules.' }
    # buildstamp.c is a custom compiler invocation rather than a ClCompile item.
    $stamp=Join-Path $root 'build/ubsan/obj/FBE/Win32/Release/buildstamp.obj'
    $directives=(& (Join-Path $compilerBin 'dumpbin.exe') /directives $stamp) -join "`n"
    if($LASTEXITCODE -ne 0 -or $directives -notmatch 'clang_rt\.ubsan_standalone-i386\.lib') { throw 'The custom buildstamp C compilation lacks clang UBSan runtime directives.' }
    $units+=@(@{source=(Join-Path $root 'src/fbe/buildstamp.c');object=$stamp;sanitizer='Undefined';compilerEvidence='COFF UBSan runtime directives';asan=$false})
}
@{schemaVersion=1;sanitizer=$Sanitizer;compiler=$(if($toolchain){'clang-cl'}else{'MSVC'});compilerVersion=$(if($toolchain){$toolchain.compilerVersion}else{$env:VCToolsVersion});toolchain=$toolchain;vendorExceptionPolicy=$(if($toolchain){@{path='tools/msbuild/ubsan.ignorelist';sha256=(Get-FileHash -LiteralPath (Join-Path $root 'tools/msbuild/ubsan.ignorelist')).Hash;check='signed-integer-overflow';sdkHeaderSha256=(Get-FileHash -LiteralPath (Join-Path $env:VCToolsInstallDir 'atlmfc/include/atlwin.h')).Hash}}else{$null});configuration='Release';platform='Win32';platformToolset='v143';vcToolsVersion=$env:VCToolsVersion;gitCommit=$commit;minimumDiagnosticOS='Windows 10';publishable=$false;modules=$coverage;translationUnits=$units;linkCommandLogs=$(if($toolchain){@($linkLogs.FullName)}else{$null});stlContainerAnnotations=$(if($toolchain){$null}else{$false});uninstrumented=@('Scintilla','Lexilla','PCRE2','Hunspell','libarchive','image stack','lunasvg/plutovg vendor libraries','Windows/COM/MSHTML')} |
    ConvertTo-Json -Depth 6 | Set-Content (Join-Path $output 'fbe-sanitizer.json') -Encoding utf8
Write-Host "Isolated $Sanitizer sanitizer build ready: $output"
