[CmdletBinding()]
param([string]$LlvmRoot)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$explicit = if($LlvmRoot) { $LlvmRoot } else { $env:FBE_LLVM_ROOT }
$candidates = @()
if($explicit) { $candidates += Join-Path $explicit 'bin/clang-cl.exe' }
else {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    if(Test-Path $vswhere) {
        foreach($vs in @(& $vswhere -all -products '*' -property installationPath)) {
            $candidates += Join-Path $vs 'VC/Tools/Llvm/x64/bin/clang-cl.exe'
            $candidates += Join-Path $vs 'VC/Tools/Llvm/bin/clang-cl.exe'
        }
    }
    $candidates += Join-Path $env:ProgramFiles 'LLVM/bin/clang-cl.exe'
    $candidates += @(Get-Command clang-cl.exe -All -ErrorAction SilentlyContinue | ForEach-Object Source)
    foreach($cache in @(Get-ChildItem (Join-Path $root 'out/tools') -Directory -Filter 'llvm-*' -ErrorAction SilentlyContinue | Sort-Object Name -Descending)) {
        $candidates += Join-Path $cache.FullName 'win32/bin/clang-cl.exe'
    }
}
$reasons = @()
foreach($compiler in @($candidates | Select-Object -Unique)) {
    if(-not (Test-Path -LiteralPath $compiler -PathType Leaf)) { continue }
    $compiler = (Resolve-Path -LiteralPath $compiler).Path
    $versionText = (& $compiler --version 2>&1) -join "`n"
    if($LASTEXITCODE -ne 0 -or $versionText -notmatch 'clang version (\d+\.\d+\.\d+)') { $reasons += "$compiler : no clang version"; continue }
    $version = $Matches[1]
    if([version]$version -lt [version]'21.1.8') { $reasons += "$compiler : LLVM $version predates the supported 21.1.8 baseline"; continue }
    $resource = ((& $compiler -print-resource-dir 2>&1) -join '').Trim()
    $runtime = Join-Path $resource 'lib/windows/clang_rt.ubsan_standalone-i386.lib'
    $cxx = Join-Path $resource 'lib/windows/clang_rt.ubsan_standalone_cxx-i386.lib'
    $symbolizer = Join-Path (Split-Path $compiler) 'llvm-symbolizer.exe'
    $linker = Join-Path (Split-Path $compiler) 'lld-link.exe'
    if(-not (Test-Path -LiteralPath $runtime) -or -not (Test-Path -LiteralPath $cxx) -or -not (Test-Path -LiteralPath $symbolizer) -or -not(Test-Path -LiteralPath $linker)) {
        $reasons += "$compiler : missing native i386 UBSan runtime, symbolizer or lld-link in $resource"; continue
    }
    $llvmText=(& $symbolizer --version 2>&1) -join "`n"
    if($LASTEXITCODE -ne 0 -or $llvmText -notmatch 'LLVM version (\d+\.\d+\.\d+)' -or $Matches[1] -ne $version) { $reasons += "$compiler : compiler/symbolizer LLVM version mismatch"; continue }
    $linkerText=(& $linker --version 2>&1) -join "`n"
    if($LASTEXITCODE -ne 0 -or $linkerText -notmatch 'LLD (\d+\.\d+\.\d+)' -or $Matches[1] -ne $version) { $reasons += "$compiler : compiler/linker LLVM version mismatch"; continue }
    return [pscustomobject]@{
        compiler='clang-cl'; compilerPath=$compiler; compilerVersion=$version; versionText=$versionText
        compilerSha256=(Get-FileHash $compiler).Hash
        llvmVersion=$version
        linker='lld-link'; linkerPath=$linker; linkerVersion=$version; linkerSha256=(Get-FileHash $linker).Hash
        target='i686-pc-windows-msvc'; architecture='Win32'; resourceDirectory=$resource
        runtimeDirectory=(Split-Path $runtime); runtimeLibrary=$runtime; cxxRuntimeLibrary=$cxx
        runtimeSha256=(Get-FileHash $runtime).Hash; symbolizerPath=$symbolizer
    }
}
throw "UBSan build = BLOCKED: native clang-cl with i386 UBSan runtime and llvm-symbolizer required. $($reasons -join '; ') Run tools/build/prepare-ubsan-llvm.ps1 or set FBE_LLVM_ROOT. No MSVC fallback."
