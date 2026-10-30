[CmdletBinding()]
param(
    [string]$FbeExe = (Join-Path $PSScriptRoot '../../out/Release/FBE.exe'),
    [ValidateSet('portable','installed')][string]$Mode = 'portable'
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$iso = New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'script-icon-size'
$passed = $false
$names = @('FBE_NEXT_TEST_MODE','FBE_NEXT_TEST_SCENARIO','FBE_NEXT_TEST_SETTINGS_DIRECTORY')
$saved = @{}; foreach($name in $names) { $saved[$name] = [Environment]::GetEnvironmentVariable($name,'Process') }
try {
    if($Mode -eq 'installed') { $env:FBE_NEXT_TEST_SETTINGS_DIRECTORY = $iso.DataPath }
    else { $env:FBE_NEXT_TEST_SETTINGS_DIRECTORY = $null }
    $env:FBE_NEXT_TEST_MODE = '1'
    $report = Join-Path $iso.DataPath 'Diagnostics/portable-state-report.txt'
    foreach($scenario in @('script-toolbar-runtime-size','script-icon-size-read','scripts-toolbar-customize-ctrl-f-runtime')) {
        $env:FBE_NEXT_TEST_SCENARIO = $scenario
        Set-IsolatedRuntimeStage $iso "$scenario/$Mode"
        Remove-Item -LiteralPath $report -Force -ErrorAction SilentlyContinue
        $process = Start-Process $iso.Exe -ArgumentList "--$Mode" -WorkingDirectory $iso.Root -WindowStyle Hidden -PassThru
        $exitCode = Wait-IsolatedFbeProcess -Process $process -Isolation $iso -Scenario $scenario -Report $report -TimeoutSeconds 120
        $text = if(Test-Path -LiteralPath $report) { Get-Content -LiteralPath $report -Raw } else { '<missing report>' }
        Write-Host $text
        if($exitCode -ne 0 -or $text -notmatch '(?m)^result=pass\r?$') { throw "Script icon size runtime failed ($Mode/$scenario), exit=$exitCode" }
        if($scenario -eq 'script-toolbar-runtime-size' -and ($text -notmatch '(?m)^size-switches-pass=1\r?$' -or $text -notmatch '(?m)^initial-icon-size=16\r?$')) { throw 'Missing default size or 100-switch coverage' }
        if($scenario -eq 'script-toolbar-runtime-size' -and ($text -notmatch '(?m)^fixture-16px-pixels=1\r?$' -or $text -match '(?m)^fixture-16px-pixels=0\r?$')) { throw '16px script icon pixels differ from the source icon.' }
    }
    $passed = $true
    Write-Host "Script icon size runtime: PASS ($Mode)."
} finally {
    foreach($name in $names) { [Environment]::SetEnvironmentVariable($name,$saved[$name],'Process') }
    Complete-IsolatedFbeRuntime -Isolation $iso -Passed $passed
}
