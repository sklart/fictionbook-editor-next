[CmdletBinding()]
param(
    [string]$FbeExe = (Join-Path $PSScriptRoot '../../out/Release/FBE.exe'),
    [ValidateSet('portable','installed')][string]$Mode = 'portable'
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$iso = New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'theme-policy'
$passed = $false
$names = @('FBE_NEXT_TEST_MODE','FBE_NEXT_TEST_SCENARIO','FBE_NEXT_TEST_SETTINGS_DIRECTORY')
$saved = @{}; foreach($name in $names) { $saved[$name] = [Environment]::GetEnvironmentVariable($name,'Process') }
try {
    $env:FBE_NEXT_TEST_MODE = '1'
    $env:FBE_NEXT_TEST_SETTINGS_DIRECTORY = if($Mode -eq 'installed') { $iso.DataPath } else { $null }
    $report = Join-Path $iso.DataPath 'Diagnostics/portable-state-report.txt'
    foreach($scenario in @('interface-theme-policy-runtime','interface-theme-policy-read')) {
        $env:FBE_NEXT_TEST_SCENARIO = $scenario
        Set-IsolatedRuntimeStage $iso "$scenario/$Mode"
        Remove-Item -LiteralPath $report -Force -ErrorAction SilentlyContinue
        $process = Start-Process $iso.Exe -ArgumentList "--$Mode" -WorkingDirectory $iso.Root -WindowStyle Hidden -PassThru
        $exitCode = Wait-IsolatedFbeProcess -Process $process -Isolation $iso -Scenario $scenario -Report $report -TimeoutSeconds 90
        $text = if(Test-Path -LiteralPath $report) { Get-Content -LiteralPath $report -Raw } else { '<missing report>' }
        Write-Host $text
        if($exitCode -ne 0 -or $text -notmatch '(?m)^result=pass\r?$') { throw "Theme policy runtime failed ($Mode/$scenario), exit=$exitCode" }
    }
    Write-Host "Theme policy runtime: PASS ($Mode; controlled platform availability, not a Windows 7 run)."
    $passed = $true
} finally {
    foreach($name in $names) { [Environment]::SetEnvironmentVariable($name,$saved[$name],'Process') }
    Complete-IsolatedFbeRuntime -Isolation $iso -Passed $passed
}
