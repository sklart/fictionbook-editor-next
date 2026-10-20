[CmdletBinding()]
param(
    [string]$FbeExe = (Join-Path $PSScriptRoot '../../out/Release/FBE.exe'),
    [ValidateSet('portable','installed')][string]$Mode = 'portable',
    [string]$CaptureDirectory,
    [int]$TimeoutSeconds = 90
)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
if(!$CaptureDirectory) { $CaptureDirectory = Join-Path $root "build/plugin-tabs-captures/$Mode" }
[void][IO.Directory]::CreateDirectory($CaptureDirectory)
$CaptureDirectory = (Resolve-Path $CaptureDirectory).Path
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$iso = New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'plugin-tabs'
$passed = $false
$names = @('FBE_NEXT_TEST_MODE','FBE_NEXT_TEST_SCENARIO','FBE_NEXT_TEST_SETTINGS_DIRECTORY','FBE_NEXT_TEST_PLUGIN_CAPTURES')
$saved = @{}; foreach($name in $names) { $saved[$name] = [Environment]::GetEnvironmentVariable($name,'Process') }
try {
    $null = New-IsolatedFbeSettingsSeed $iso
    $env:FBE_NEXT_TEST_MODE = '1'; $env:FBE_NEXT_TEST_SCENARIO = 'plugin-options-tabs-runtime'
    $env:FBE_NEXT_TEST_PLUGIN_CAPTURES = $CaptureDirectory
    $env:FBE_NEXT_TEST_SETTINGS_DIRECTORY = if($Mode -eq 'installed') { Join-Path $iso.DataPath 'Settings' } else { $null }
    $report = Join-Path $iso.Root 'plugin-tabs.log'
    $fixture = (Resolve-Path (Join-Path $PSScriptRoot 'fb2-validate-valid-smoke.fb2')).Path
    Set-IsolatedRuntimeStage $iso "plugin-options-tabs-runtime/$Mode"
    $process = Start-Process $iso.Exe -ArgumentList @("--$Mode",'-b',$report,$fixture) -WorkingDirectory $iso.Root -WindowStyle Hidden -PassThru
    $code = Wait-IsolatedFbeProcess -Process $process -Isolation $iso -Scenario 'plugin-options-tabs-runtime' -Report $report -TimeoutSeconds $TimeoutSeconds
    $text = if(Test-Path $report) { Get-Content $report -Raw } else { '<missing report>' }
    Copy-Item $report (Join-Path $CaptureDirectory 'runtime.log') -ErrorAction SilentlyContinue
    if($code -ne 0 -or $text -notmatch '(?m)^result=pass\r?$') { throw "Native tabbed plug-in windows failed ($Mode), exit=$code`n$text" }
    if(@(Get-ChildItem $CaptureDirectory -Filter '*.png').Count -lt 44) { throw 'Missing native tab captures.' }
    Write-Host $text
    Write-Host "Native plug-in tabs: PASS ($Mode), captures=$CaptureDirectory"
    $passed = $true
} finally {
    foreach($name in $names) { [Environment]::SetEnvironmentVariable($name,$saved[$name],'Process') }
    Complete-IsolatedFbeRuntime -Isolation $iso -Passed $passed
}
