[CmdletBinding()]
param(
    [string]$Reference,
    [string]$BatchOutputDirectory = 'out\Release'
)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$releaseRoot = Join-Path $repoRoot 'out\Release'
$batchRoot = if ([IO.Path]::IsPathRooted($BatchOutputDirectory)) { $BatchOutputDirectory } else { Join-Path $repoRoot $BatchOutputDirectory }
$binaries = @(
    @{ Label = 'FBE.exe'; Root = $releaseRoot; Path = 'FBE.exe' },
    @{ Label = 'FBV.exe'; Root = $releaseRoot; Path = 'FBV.exe' },
    @{ Label = 'FBShell.dll'; Root = $releaseRoot; Path = 'FBShell.dll' },
    @{ Label = 'Plugins\ExportHTML.dll'; Root = $releaseRoot; Path = 'Plugins\ExportHTML.dll' },
    @{ Label = 'Plugins\ExportDOCX.dll'; Root = $releaseRoot; Path = 'Plugins\ExportDOCX.dll' },
    @{ Label = 'Plugins\ExportEPUB.dll'; Root = $releaseRoot; Path = 'Plugins\ExportEPUB.dll' },
    @{ Label = 'Plugins\ImportEPUB.dll'; Root = $releaseRoot; Path = 'Plugins\ImportEPUB.dll' },
    @{ Label = 'Plugins\ImportEPUBLunaSVG.dll'; Root = $releaseRoot; Path = 'Plugins\ImportEPUBLunaSVG.dll' },
    @{ Label = 'ExportDOCXBatch.exe'; Root = $batchRoot; Path = 'ExportDOCXBatch.exe' },
    @{ Label = 'ExportEPUBBatch.exe'; Root = $batchRoot; Path = 'ExportEPUBBatch.exe' },
    @{ Label = 'ImportEPUBBatch.exe'; Root = $batchRoot; Path = 'ImportEPUBBatch.exe' }
)
if ($Reference -and -not (Test-Path -LiteralPath $Reference -PathType Container)) { throw "Reference directory does not exist: $Reference" }

Write-Host 'Release binary sizes:'
foreach ($entry in $binaries) {
    $binary = Join-Path $entry.Root $entry.Path
    if (-not (Test-Path -LiteralPath $binary -PathType Leaf)) { Write-Warning "Missing: $($entry.Label)"; continue }
    $size = (Get-Item -LiteralPath $binary).Length
    $line = '{0,-34} {1,8:N2} MiB ({2:N0} bytes)' -f $entry.Label, ($size / 1MB), $size
    if ($Reference) {
        $referenceBinary = Join-Path $Reference $entry.Path
        if (Test-Path -LiteralPath $referenceBinary -PathType Leaf) {
            $before = (Get-Item -LiteralPath $referenceBinary).Length; $delta = $size - $before
            $percent = if ($before) { 100 * $delta / $before } else { $null }
            $line += ('  delta {0:+0.00;-0.00;0.00} MiB ({1:+0.00;-0.00;0.00}%)' -f ($delta / 1MB), $percent)
        } else { $line += '  (not present in reference)' }
    }
    Write-Host $line
}
