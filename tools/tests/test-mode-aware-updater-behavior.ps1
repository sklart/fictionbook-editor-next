<# Calls FBE's production UpdateArtifact selector without downloading anything. #>
[CmdletBinding()]
param([string]$FbeExecutable)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
if (-not $FbeExecutable) { $FbeExecutable = Join-Path $root 'out\Release\FBE.exe' }
$FbeExecutable = (Resolve-Path -LiteralPath $FbeExecutable).Path
$selector = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\UpdateArtifact.h')
$updater = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\AboutBox.cpp')
if ($selector -notmatch 'SelectUpdateArtifact' -or $updater -notmatch 'SelectUpdateArtifact') { throw 'Updater must use the shared production artifact selector.' }

$output = Join-Path $root 'out\tests\mode-aware-updater-behavior.json'
Remove-Item -LiteralPath $output -Force -ErrorAction SilentlyContinue
$process = Start-Process -FilePath $FbeExecutable -ArgumentList @('--portable', '--print-update-artifact') -Wait -PassThru -NoNewWindow -RedirectStandardOutput $output
if ($process.ExitCode -ne 0) { throw "Production artifact selector exited with $($process.ExitCode)." }
$artifact = Get-Content -Raw -LiteralPath $output | ConvertFrom-Json
if ($artifact.type -ne 'Portable' -or $artifact.extension -ne '.zip' -or $artifact.fileName -notmatch '-win32-portable\.zip$') { throw "Modern portable selected the wrong artifact: $($artifact | ConvertTo-Json -Compress)" }
if ($artifact.fileName -match 'setup\.exe') { throw 'Modern portable selector chose setup.exe.' }
Write-Host 'Modern portable updater production selector chose the portable ZIP (no download).'
