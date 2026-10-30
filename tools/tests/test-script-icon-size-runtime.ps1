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
        if($scenario -eq 'script-toolbar-runtime-size' -and $text -notmatch '(?m)^tree-fixture-16px-pixels=1\r?$') { throw '16px script tree icon pixels differ from the source icon.' }
        if($scenario -eq 'script-toolbar-runtime-size' -and (($text -notmatch '(?m)^dual-bitmap-preferred=1\r?$') -or ($text -match '(?m)^dual-bitmap-preferred=0\r?$') -or ($text -notmatch '(?m)^dual-larger-ico-preferred=1\r?$') -or ($text -match '(?m)^dual-larger-ico-preferred=0\r?$'))) { throw 'BMP/ICO sidecar priority is inconsistent.' }
        if($scenario -eq 'script-toolbar-runtime-size' -and (($text -notmatch '(?m)^tree-dual-bitmap=1\r?$') -or ($text -notmatch '(?m)^tree-dual-larger-ico=1\r?$'))) { throw 'BMP/ICO tree sidecar priority is inconsistent.' }
        if($scenario -eq 'script-toolbar-runtime-size' -and (($text -notmatch '(?m)^bitmap-only-16=1\r?$') -or ($text -match '(?m)^bitmap-only-16=0\r?$') -or ($text -notmatch '(?m)^bitmap-only-20=1\r?$') -or ($text -match '(?m)^bitmap-only-20=0\r?$'))) { throw 'BMP-only icon rendering or native-size centering failed.' }
        if($scenario -eq 'script-toolbar-runtime-size' -and (($text -notmatch '(?m)^tree-bitmap-only-16=1\r?$') -or ($text -notmatch '(?m)^tree-bitmap-only-20=1\r?$'))) { throw 'BMP-only tree icon rendering or native-size centering failed.' }
        if($scenario -eq 'script-toolbar-runtime-size' -and (($text -notmatch '(?m)^rect-bitmap-16=1\r?$') -or ($text -match '(?m)^rect-bitmap-16=0\r?$') -or ($text -notmatch '(?m)^rect-bitmap-20=1\r?$') -or ($text -match '(?m)^rect-bitmap-20=0\r?$') -or ($text -notmatch '(?m)^tree-rect-16=1\r?$') -or ($text -notmatch '(?m)^tree-rect-20=1\r?$'))) { throw 'Rectangular BMP icon rendering or centering failed.' }
    }
    $passed = $true
    Write-Host "Script icon size runtime: PASS ($Mode)."
} finally {
    foreach($name in $names) { [Environment]::SetEnvironmentVariable($name,$saved[$name],'Process') }
    Complete-IsolatedFbeRuntime -Isolation $iso -Passed $passed
}
