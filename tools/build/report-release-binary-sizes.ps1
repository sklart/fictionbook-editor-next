[CmdletBinding()]
param(
    [string]$Reference
)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$outputRoot = Join-Path $repoRoot 'out\Release'
$binaries = @(
    'FBE.exe', 'FBV.exe', 'FBShell.dll',
    'Plugins\ExportHTML.dll', 'Plugins\ExportDOCX.dll', 'Plugins\ExportEPUB.dll',
    'Plugins\ImportEPUB.dll', 'Plugins\ImportEPUBLunaSVG.dll',
    'ExportDOCXBatch.exe', 'ExportEPUBBatch.exe', 'ImportEPUBBatch.exe'
)

if ($Reference -and -not (Test-Path -LiteralPath $Reference -PathType Container)) {
    throw "Reference directory does not exist: $Reference"
}

foreach ($relativePath in $binaries) {
    $binary = Join-Path $outputRoot $relativePath
    if (-not (Test-Path -LiteralPath $binary -PathType Leaf)) {
        Write-Warning "Missing: $relativePath"
        continue
    }
    $size = (Get-Item -LiteralPath $binary).Length
    $line = '{0,-34} {1,10:N2} MiB ({2:N0} bytes)' -f $relativePath, ($size / 1MB), $size
    if ($Reference) {
        $referenceBinary = Join-Path $Reference $relativePath
        if (Test-Path -LiteralPath $referenceBinary -PathType Leaf) {
            $delta = $size - (Get-Item -LiteralPath $referenceBinary).Length
            $line += ('  {0:+#,##0;-#,##0;0} bytes' -f $delta)
        } else {
            $line += '  (not present in reference)'
        }
    }
    Write-Host $line
}
