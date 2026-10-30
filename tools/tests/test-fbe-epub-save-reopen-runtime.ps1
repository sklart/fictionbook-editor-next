[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$InputFb2,
    [Parameter(Mandatory)][string]$OutputFb2,
    [string]$FbeExe
)
$ErrorActionPreference = 'Stop'
if (-not $FbeExe) { $FbeExe = Join-Path $PSScriptRoot '../../out/Release/FBE.exe' }
$inputPath = (Resolve-Path -LiteralPath $InputFb2).Path
$outputPath = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputFb2)
if ($inputPath -eq $outputPath -or (Test-Path -LiteralPath $outputPath)) { throw 'Output must be a new file distinct from the input.' }
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$iso = New-IsolatedFbeRuntime -FbeExe $FbeExe -Name epub-save-reopen
$passed = $false
$names = @('FBE_NEXT_TEST_MODE','FBE_NEXT_TEST_SCENARIO','FBE_NEXT_TEST_EPUB_SAVE_PATH')
$old = @{}; foreach ($name in $names) { $old[$name] = [Environment]::GetEnvironmentVariable($name) }
try {
    $null = New-IsolatedFbeSettingsSeed -Isolation $iso -StartupScriptsDirectory (Join-Path $iso.Root empty-scripts)
    $env:FBE_NEXT_TEST_MODE = '1'; $env:FBE_NEXT_TEST_SCENARIO = 'epub-save-reopen-runtime'; $env:FBE_NEXT_TEST_EPUB_SAVE_PATH = $outputPath
    $report = Join-Path $iso.Root report.txt
    $process = Start-Process -FilePath $iso.Exe -WorkingDirectory $iso.Runtime -WindowStyle Hidden -ArgumentList @('--portable','-b',('"' + $report + '"'),('"' + $inputPath + '"')) -PassThru
    $code = Wait-IsolatedFbeProcess -Process $process -Isolation $iso -Scenario $env:FBE_NEXT_TEST_SCENARIO -Report $report -TimeoutSeconds 180
    $text = Get-Content -LiteralPath $report -Raw
    if ($code -ne 0 -or $text -notmatch '(?m)^saved=1\r?$' -or $text -notmatch '(?m)^reopened=1\r?$') { throw "Save/reopen failed: $text" }
    $passed = $true; Write-Host 'FBE save and reopen passed.'
} finally {
    foreach ($name in $names) { [Environment]::SetEnvironmentVariable($name, $old[$name]) }
    Complete-IsolatedFbeRuntime -Isolation $iso -Passed $passed
}
