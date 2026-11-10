<# Check the legacy userCmd src observed by a real running script. #>
[CmdletBinding()]
param([Parameter(Mandatory)][string]$FbeExe, [int]$TimeoutSeconds = 90)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$root = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$iso = New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'usercmd-path'
$passed = $false
$names = @('FBE_NEXT_TEST_MODE','FBE_NEXT_TEST_SCENARIO','FBE_NEXT_SCRIPT_API_ROOT','FBE_NEXT_TEST_SETTINGS_DIRECTORY')
$old = @{}; foreach ($name in $names) { $old[$name] = [Environment]::GetEnvironmentVariable($name, 'Process') }
try {
    $null = New-IsolatedFbeSettingsSeed $iso -StartupScriptsDirectory (Join-Path $iso.Root 'ProbeScripts')
    Copy-Item -LiteralPath (Join-Path $root 'runtime/blank.fb2') -Destination (Join-Path $iso.Root 'fixture.fb2')
    $script = @'
var compatResult="NOT RUN";
function CompatResult(){return compatResult;}
function Run(){
 var node=document.getElementById("userCmd"), actual=node && node.getAttribute("src");
 var expected="file:///"+window.external.GetCurrentScriptPath();
 compatResult=String(actual)===expected ? "PASS" : "FAIL: userCmd src="+String(actual)+" expected="+expected;
}
'@
    [IO.File]::WriteAllText((Join-Path $iso.Root 'путь.js'), $script, [Text.UTF8Encoding]::new($false))
    [IO.File]::WriteAllText((Join-Path $iso.Root 'suite.xml'), '<suite><step name="usercmd-src" script="путь.js"/></suite>', [Text.UTF8Encoding]::new($false))
    $env:FBE_NEXT_TEST_MODE='1'; $env:FBE_NEXT_TEST_SCENARIO='binary-scripting-compatibility'; $env:FBE_NEXT_SCRIPT_API_ROOT=$iso.Root
    $env:FBE_NEXT_TEST_SETTINGS_DIRECTORY=Join-Path $iso.DataPath 'Settings'
    $report=Join-Path $iso.Root 'report.txt'
    $process=Start-Process $iso.Exe -WorkingDirectory $iso.Root -ArgumentList @('--portable','-b',$report,(Join-Path $iso.Root 'fixture.fb2')) -WindowStyle Hidden -PassThru
    $code=Wait-IsolatedFbeProcess -Process $process -Isolation $iso -Scenario 'usercmd-path' -Report $report -TimeoutSeconds $TimeoutSeconds
    $text=if(Test-Path -LiteralPath $report){Get-Content -Raw -LiteralPath $report}else{'missing report'}
    if($code -ne 0 -or $text -notmatch 'result=PASS'){throw "userCmd path failed: $text"}
    $passed=$true; Write-Host 'Legacy userCmd source path passed.'
} finally {
    foreach ($name in $names) { [Environment]::SetEnvironmentVariable($name,$old[$name],'Process') }
    Complete-IsolatedFbeRuntime -Isolation $iso -Passed $passed
}
