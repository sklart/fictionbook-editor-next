<# Reports optional ImportEPUB manual-test prerequisites without changing state. #>
[CmdletBinding()]
param([string]$OutputDirectory, [string]$ReportPath)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path
if (-not $OutputDirectory) { $OutputDirectory = Join-Path $repoRoot 'out\Release' }
if (-not $ReportPath) { $ReportPath = Join-Path $repoRoot 'out\manual\import-epub\dependency-report.txt' }
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $ReportPath) | Out-Null
$lines = foreach ($name in @('ImportEPUB.dll', 'ImportEPUBBatch.exe', 'ImportEPUBLunaSVG.dll')) {
    $path = Join-Path $OutputDirectory $name
    if (Test-Path -LiteralPath $path -PathType Leaf) { "OK: $name ($((Get-Item -LiteralPath $path).Length) bytes)" }
    elseif ($name -eq 'ImportEPUBLunaSVG.dll') { "OPTIONAL MISSING: $name" } else { "MISSING: $name" }
}
foreach ($name in @('plutovg.lib', 'lunasvg.lib')) {
    if (Test-Path -LiteralPath (Join-Path $repoRoot "build\lib\lunasvg\Win32\Release\$name") -PathType Leaf) { "OK: build/lib/lunasvg/Win32/Release/$name" }
    else { "OPTIONAL MISSING: build/lib/lunasvg/Win32/Release/$name" }
}
$lines | Set-Content -LiteralPath $ReportPath -Encoding utf8
$lines | ForEach-Object { Write-Host $_ }
