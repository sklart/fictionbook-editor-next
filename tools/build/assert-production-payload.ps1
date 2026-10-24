<# Reject diagnostic binaries before any release package is accepted. #>
[CmdletBinding()]
param([Parameter(Mandatory)][string]$Directory)
$ErrorActionPreference='Stop'
$root=(Resolve-Path -LiteralPath $Directory).Path
foreach($file in Get-ChildItem -LiteralPath $root -Recurse -File) {
    if($file.Name -in @('fbe-sanitizer.json','fbe-ubsan-toolchain.json') -or $file.Name -like 'clang_rt.asan*' -or $file.Name -like 'clang_rt.ubsan*') { throw "Sanitizer artifact is forbidden in production payload: $($file.FullName)" }
    if($file.Extension -in @('.exe','.dll')) {
        # MSVC's runtime import is present even if the diagnostic marker/DLL
        # was removed before copying the instrumented executable to a stage.
        $bytes=[IO.File]::ReadAllBytes($file.FullName)
        $text=[Text.Encoding]::ASCII.GetString($bytes)
        if($text.IndexOf('clang_rt.asan',[StringComparison]::OrdinalIgnoreCase) -ge 0 -or
           $text.IndexOf('__ubsan_handle_',[StringComparison]::Ordinal) -ge 0 -or
           $text.IndexOf('UndefinedBehaviorSanitizer',[StringComparison]::Ordinal) -ge 0) { throw "Instrumented binary is forbidden in production payload: $($file.FullName)" }
    }
}
