<# Ensures local compiler/test by-products never enter the repository index. #>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$forbidden = @(
    'plugin-v2-fixture.exp', 'plugin-v2-fixture.lib',
    'ci-current.log', 'ci-failure.log', 'ci-latest.log',
    'src/export-html/build/obj/export-html/Win32/Release/ExportHTML.Build.CppClean.log',
    'src/export-html/build/obj/export-html/Win32/Release/ExportHTML.tlb',
    'src/export-html/build/obj/export-html/Win32/Release/vc143.pdb'
)
$tracked = @(& git -C $root ls-files)
$found = @($forbidden | Where-Object { $tracked -contains $_ })
if ($found.Count) { throw "Tracked local build artifacts: $($found -join ', ')" }
Write-Host 'Tracked local build artifact policy passed.'
