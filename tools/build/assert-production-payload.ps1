<# Reject diagnostic binaries before any release package is accepted. #>
[CmdletBinding()]
param([Parameter(Mandatory)][string]$Directory)
$ErrorActionPreference='Stop'
$root=(Resolve-Path -LiteralPath $Directory).Path
foreach($file in Get-ChildItem -LiteralPath $root -Recurse -File) {
    if($file.Name -eq 'fbe-sanitizer.json' -or $file.Name -like 'clang_rt.asan*') { throw "Sanitizer artifact is forbidden in production payload: $($file.FullName)" }
    if($file.Extension -in @('.exe','.dll')) {
        # MSVC's runtime import is present even if the diagnostic marker/DLL
        # was removed before copying the instrumented executable to a stage.
        $bytes=[IO.File]::ReadAllBytes($file.FullName)
        if([Text.Encoding]::ASCII.GetString($bytes).IndexOf('clang_rt.asan',[StringComparison]::OrdinalIgnoreCase) -ge 0) { throw "Instrumented binary is forbidden in production payload: $($file.FullName)" }
    }
}
