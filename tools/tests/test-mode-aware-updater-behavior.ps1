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
foreach ($forbidden in @('Modern', 'Win7', 'CompatibilityTarget', 'win7-win32')) {
    if ($selector.Contains($forbidden)) { throw "New update selector must not contain legacy profile '$forbidden'." }
}

function Assert-Artifact([string]$Mode, [string[]]$Arguments, [string]$ExpectedType, [string]$ExpectedSuffix) {
    $output = Join-Path $root "out\tests\mode-aware-updater-$Mode.json"
    Remove-Item -LiteralPath $output -Force -ErrorAction SilentlyContinue
    $process = Start-Process -FilePath $FbeExecutable -ArgumentList $Arguments -Wait -PassThru -NoNewWindow -RedirectStandardOutput $output
    if ($process.ExitCode -ne 0) { throw "Production $Mode artifact selector exited with $($process.ExitCode)." }
    $artifact = Get-Content -Raw -LiteralPath $output | ConvertFrom-Json
    if ($artifact.type -ne $ExpectedType -or $artifact.fileName -notmatch $ExpectedSuffix -or $artifact.fileName -match 'win7-win32') {
        throw "$Mode mode selected the wrong unified artifact: $($artifact | ConvertTo-Json -Compress)"
    }
}

Assert-Artifact 'portable' @('--portable', '--print-update-artifact') 'Portable' '-win32-portable\.zip$'
Assert-Artifact 'installed' @('--installed', '--print-update-artifact') 'Setup' '-win32-setup\.exe$'
Write-Host 'Installed and portable updater selectors chose only unified artifacts (no download).'
