[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$InputFb2,
    [Parameter(Mandatory)][string]$Report,
    [string]$FbeExe = (Join-Path $PSScriptRoot '../../out/Release/FBE.exe')
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$inputPath = (Resolve-Path -LiteralPath $InputFb2).Path
$reportPath = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($Report)
if (Test-Path -LiteralPath $reportPath) { throw 'Report must be a new file.' }
$iso = New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'real-source-selection'
$passed = $false
$oldMode = $env:FBE_NEXT_TEST_MODE; $oldScenario = $env:FBE_NEXT_TEST_SCENARIO
try {
    $null = New-IsolatedFbeSettingsSeed -Isolation $iso -StartupScriptsDirectory (Join-Path $iso.Root 'empty-scripts')
    $sourceHash = (Get-FileHash -LiteralPath $inputPath -Algorithm SHA256).Hash
    $env:FBE_NEXT_TEST_MODE = '1'; $env:FBE_NEXT_TEST_SCENARIO = 'real-book-source-selection'
    $process = Start-Process -FilePath $iso.Exe -WorkingDirectory $iso.Runtime -WindowStyle Hidden -ArgumentList @('--portable','-b',('"' + $reportPath + '"'),('"' + $inputPath + '"')) -PassThru
    $exitCode = Wait-IsolatedFbeProcess -Process $process -Isolation $iso -Scenario $env:FBE_NEXT_TEST_SCENARIO -Report $reportPath -TimeoutSeconds 180
    $text = if (Test-Path -LiteralPath $reportPath) { Get-Content -Raw -LiteralPath $reportPath } else { '<missing>' }
    if ($exitCode -ne 0 -or $text -notmatch '(?m)^perf-mid=1/1/1/1/1/1/1\r?$' -or $text -notmatch '(?m)^perf-last=1/1/1/1/1/1/1\r?$') { throw "Selection check failed: $text" }
    if ((Get-FileHash -LiteralPath $inputPath -Algorithm SHA256).Hash -ne $sourceHash) { throw 'Input changed.' }
    $passed = $true; Write-Host 'Real book source selection: PASS.'
} finally {
    $env:FBE_NEXT_TEST_MODE = $oldMode; $env:FBE_NEXT_TEST_SCENARIO = $oldScenario
    Complete-IsolatedFbeRuntime -Isolation $iso -Passed $passed
}
