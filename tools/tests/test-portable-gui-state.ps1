<# Full-only behavioral persistence test for a materialised portable package. #>
[CmdletBinding()]
param([Parameter(Mandatory)][string]$PackageDirectory)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$package = (Resolve-Path -LiteralPath $PackageDirectory).Path
$testRoot = Join-Path $root 'out\tests\portable-gui-state'
Remove-Item -LiteralPath $testRoot -Recurse -Force -ErrorAction SilentlyContinue
Copy-Item -LiteralPath $package -Destination $testRoot -Recurse -Force

function Get-FileTreeSnapshot([string]$Directory) {
    if (-not (Test-Path -LiteralPath $Directory -PathType Container)) { return '<absent>' }
    return (Get-ChildItem -LiteralPath $Directory -Recurse -File | Sort-Object FullName | ForEach-Object {
        "$($_.FullName)|$($_.Length)|$($_.LastWriteTimeUtc.Ticks)|$((Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash)"
    }) -join "`n"
}
function Invoke-PortableGui([string[]]$Arguments) {
    $process = Start-Process -FilePath (Join-Path $testRoot 'FBE.exe') -ArgumentList $Arguments -WorkingDirectory $testRoot -PassThru
    try {
        Start-Sleep -Seconds 8
        $process.Refresh(); if ($process.HasExited) { throw "Portable GUI exited during startup with $($process.ExitCode)." }
    } finally {
        $process.Refresh()
        if (-not $process.HasExited) {
            $process.CloseMainWindow() | Out-Null
            if (-not $process.WaitForExit(15000)) { Stop-Process -Id $process.Id -Force; throw 'Portable GUI did not exit cleanly.' }
        }
    }
}

$installedData = Join-Path $env:LOCALAPPDATA 'FBE Next'
$installedBefore = Get-FileTreeSnapshot $installedData
$fixture = Join-Path $testRoot 'portable-state-smoke.fb2'
Copy-Item -LiteralPath (Join-Path $root 'tools\tests\fb2-metadata-cyrillic-smoke.fb2') -Destination $fixture
Invoke-PortableGui @($fixture)
foreach ($name in @('Settings.xml','Hotkeys.xml','Words.xml')) {
    if (-not (Test-Path -LiteralPath (Join-Path $testRoot "Data\Settings\$name") -PathType Leaf)) { throw "Portable GUI did not persist $name in Data\\Settings." }
}
Invoke-PortableGui @()
foreach ($name in @('Settings.xml','Hotkeys.xml','Words.xml')) {
    if (-not (Test-Path -LiteralPath (Join-Path $testRoot "Data\Settings\$name") -PathType Leaf)) { throw "Portable state $name was not retained across restart." }
}
if ((Get-FileTreeSnapshot $installedData) -cne $installedBefore) { throw '%LOCALAPPDATA%\\FBE Next changed during portable GUI state test.' }
& (Join-Path $root 'tools\tests\test-portable-registry-isolation.ps1') -FbeExecutable (Join-Path $testRoot 'FBE.exe')
Write-Host 'Portable GUI state persistence and isolation passed.'
