[CmdletBinding()]
param([Parameter(Mandatory)][string]$CaseFile)
$ErrorActionPreference='Stop'
$case=Get-Content -Raw -LiteralPath $CaseFile | ConvertFrom-Json
$repoRoot=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$outputDir=$case.outputDirectory; $batchOutputDir=$outputDir
$Configuration='Release'; $PlatformToolset='v143'
$env:MSBUILDDISABLENODEREUSE='1'
Set-Location -LiteralPath $repoRoot
try {
    if($env:VCToolsVersion -notlike '14.44*' -or $env:VSCMD_ARG_TGT_ARCH -ne 'x86') { throw 'Sanitizer worker requires the prepared VC 14.44 x86 environment.' }
    # Harnesses request v143, latest, or explicit 14.44 through the shared
    # importer. All three must reuse this pinned diagnostic environment.
    # Re-running VsDevCmd inside every worker also spawns short-lived registry
    # probes unrelated to the runtime scenario under observation.
    foreach($version in @('v143','latest','14.44')) { [Environment]::SetEnvironmentVariable("FBE_VSDEV_x86_x64_${version}_INITIALIZED",'1','Process') }
    if($case.sanitizer -eq 'Undefined') {
        if(-not (Test-Path -LiteralPath $case.clangClPath)) { throw 'Recorded UBSan clang-cl is unavailable; no compiler fallback.' }
        $env:FBE_UBSAN_CLANG=$case.clangClPath
        if(-not $case.helperOptions.Count) { throw 'Missing explicit UBSan helper compiler options.' }
        $global:FbeUbsanHelperOptions=@($case.helperOptions)
        # Existing standalone test compilers share the same diagnostic toolchain.
        # MSVC /GL is incompatible with this separate non-LTO clang contour.
        function global:cl.exe {
            $forward=@($args | Where-Object {$_ -ne '/GL'} | ForEach-Object {if($_ -eq '/MD'){'/MT'}else{$_}})
            [string[]]$compilerOptions=$global:FbeUbsanHelperOptions
            & $env:FBE_UBSAN_CLANG --target=i686-pc-windows-msvc @compilerOptions @forward
            $global:LASTEXITCODE=$LASTEXITCODE
        }
    }
    $global:LASTEXITCODE=0
    & ([scriptblock]::Create($case.command))
    if ($LASTEXITCODE -ne 0) { throw "Runtime harness native exit: $LASTEXITCODE" }
} catch { Write-Error $_; exit 1 }
