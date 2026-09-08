<# Reports ImportEPUB manual-test prerequisites for the official output layout. #>
[CmdletBinding()]
param(
    [string]$PluginOutputDirectory,
    [string]$BatchOutputDirectory,
    [string]$ReportPath
)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path
if (-not $PluginOutputDirectory) { $PluginOutputDirectory = Join-Path $repoRoot 'out\Release\Plugins' }
if (-not $BatchOutputDirectory) { $BatchOutputDirectory = Join-Path $repoRoot 'out\Release' }
if (-not $ReportPath) { $ReportPath = Join-Path $repoRoot 'out\manual\import-epub\dependency-report.txt' }
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $ReportPath) | Out-Null

$lines = [Collections.Generic.List[string]]::new()
function Report-File([string]$Directory, [string]$Name, [bool]$Required) {
    $path = Join-Path $Directory $Name
    if (Test-Path -LiteralPath $path -PathType Leaf) { $lines.Add("OK: $Name ($((Get-Item -LiteralPath $path).Length) bytes) at $path") }
    elseif ($Required) { $lines.Add("MISSING: $Name expected at $path") }
    else { $lines.Add("OPTIONAL MISSING: $Name expected at $path") }
}

Report-File $PluginOutputDirectory 'ImportEPUB.dll' $true
Report-File $PluginOutputDirectory 'ImportEPUBLunaSVG.dll' $false
Report-File $BatchOutputDirectory 'ImportEPUBBatch.exe' $true
foreach ($name in @('plutovg.lib', 'lunasvg.lib')) {
    $path = Join-Path $repoRoot "build\lib\lunasvg\Win32\Release\$name"
    if (Test-Path -LiteralPath $path -PathType Leaf) { $lines.Add("OK: $path") } else { $lines.Add("OPTIONAL MISSING: $path") }
}
$lines | Set-Content -LiteralPath $ReportPath -Encoding utf8
$lines | ForEach-Object { Write-Host $_ }
if (@($lines | Where-Object { $_ -like 'MISSING:*' }).Count) { throw 'Required ImportEPUB build artifacts are missing.' }
