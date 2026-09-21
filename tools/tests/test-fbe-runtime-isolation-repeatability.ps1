[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$FbeExe,
    [ValidateRange(1, 20)][int]$Runs = 10,
    [ValidateRange(1, 10)][int]$InterleavedCycles = 5
)

$ErrorActionPreference = 'Stop'
$mru = Join-Path $PSScriptRoot 'test-fbe-archive-mru-runtime.ps1'
$archive = Join-Path $PSScriptRoot 'test-fbe-archive-runtime.ps1'
for ($index = 1; $index -le $Runs; ++$index) { & $mru -FbeExe $FbeExe; Write-Host "MRU isolation repeat $index/$Runs passed." }
for ($index = 1; $index -le $Runs; ++$index) { & $archive -FbeExe $FbeExe; Write-Host "Archive isolation repeat $index/$Runs passed." }
for ($index = 1; $index -le $InterleavedCycles; ++$index) { & $archive -FbeExe $FbeExe; & $mru -FbeExe $FbeExe; Write-Host "Interleaved isolation cycle $index/$InterleavedCycles passed." }
Write-Host 'Runtime verification isolation repeatability passed.'
