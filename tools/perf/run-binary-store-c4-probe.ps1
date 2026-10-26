[CmdletBinding()]
param([string]$FixtureDirectory=(Join-Path $PSScriptRoot '../../out/perf/results-2f0113b2-final64-uiwait'),[switch]$ProfileOnly,[string[]]$Fixtures=@('control','many','large'),[string]$OutputDirectory=(Join-Path $PSScriptRoot '../../out/perf/c4/probes'))
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$out=[IO.Path]::GetFullPath($OutputDirectory);if(Test-Path (Join-Path $out 'manifest.json')){throw 'Use new probe output directory'}
New-Item -ItemType Directory -Force $out|Out-Null
. (Join-Path $root 'tools/tests/RuntimeTestIsolation.ps1')
$exe=Join-Path $root 'out/perf/c4/diagnostic-bin/FBE.exe'
$names=@('FBE_NEXT_TEST_MODE','FBE_NEXT_TEST_SCENARIO','FBE_NEXT_SCRIPT_API_ROOT','FBE_NEXT_TEST_SETTINGS_DIRECTORY','FBE_NEXT_TRACE','FBE_NEXT_TRACE_VERBOSE','FBE_C4_PROFILE_ONLY','TEMP','TMP')
$old=@{};foreach($name in $names){$old[$name]=[Environment]::GetEnvironmentVariable($name,'Process')}
$manifest=@{status='RUNNING';kind='TEST-only C4 ownership/Open profiling';base='2f0113b2566a3510c74e7e1c4536e9d10e4be0c8';exeSha256=(Get-FileHash $exe).Hash;mainJsSha256=(Get-FileHash (Join-Path (Split-Path $exe) 'main.js')).Hash;fixtures=$Fixtures;results=@()}
$manifest|ConvertTo-Json -Depth 6|Set-Content (Join-Path $out 'manifest.json') -Encoding utf8
try {
 $temp=Join-Path $root 'out/perf/c4/tmp';New-Item -ItemType Directory -Force $temp|Out-Null;$env:TEMP=$temp;$env:TMP=$temp
 foreach($name in $Fixtures){
  $iso=New-IsolatedFbeRuntime -FbeExe $exe -Name "c4-probe-$name";$passed=$false
  try {
   $null=New-IsolatedFbeSettingsSeed $iso -StartupScriptsDirectory (Join-Path $iso.Root 'perf-scripts')
   $fixture=Join-Path $iso.Root "$name.fb2";Copy-Item (Join-Path $FixtureDirectory "$name.fb2") $fixture
   $inputHash=(Get-FileHash $fixture).Hash
   [IO.File]::WriteAllText((Join-Path $iso.Root 'c4-probe.js'),'function Run() {}',[Text.UTF8Encoding]::new($false))
   $env:FBE_NEXT_TEST_MODE='1';$env:FBE_NEXT_TEST_SCENARIO='binary-store-c4';$env:FBE_NEXT_SCRIPT_API_ROOT=$iso.Root
   $env:FBE_NEXT_TEST_SETTINGS_DIRECTORY=Join-Path $iso.DataPath 'Settings';$env:FBE_NEXT_TRACE='1';$env:FBE_NEXT_TRACE_VERBOSE='0';$env:FBE_C4_PROFILE_ONLY=if($ProfileOnly -or $name -eq 'control'){'1'}else{'0'}
   $report=Join-Path $iso.Root 'c4-report.txt';Write-Host "C4 diagnostic probe: $name"
   $process=Start-Process $iso.Exe -WorkingDirectory $iso.Root -ArgumentList @('--portable','-b',$report,$fixture) -WindowStyle Hidden -PassThru
   $code=Wait-IsolatedFbeProcess -Process $process -Isolation $iso -Scenario binary-store-c4 -Report $report -TimeoutSeconds 600
   if(Test-Path $report){Copy-Item $report (Join-Path $out "$name-report.txt")}
   $trace=Get-ChildItem (Join-Path $iso.DataPath 'Diagnostics') -Filter 'fbe-trace-*.log' -ErrorAction SilentlyContinue|Sort-Object LastWriteTime -Descending|Select-Object -First 1
   if($trace){Copy-Item $trace.FullName (Join-Path $out "$name-trace.log")}
   if($code -ne 0 -or -not(Test-Path $report) -or (Get-Content $report -Raw) -notmatch '(?m)^result=PASS\r?$'){throw "C4 probe failed: $name / $code"}
   if((Get-FileHash $fixture).Hash -cne $inputHash){throw 'Probe modified fixture file'}
   $manifest.results+=@{fixture=$name;status='PASS';inputSha256=$inputHash;reportSha256=(Get-FileHash $report).Hash};$passed=$true
  }finally{if(-not [IO.Path]::GetFullPath($iso.Root).StartsWith([IO.Path]::GetFullPath($temp).TrimEnd('\')+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Unexpected isolation cleanup root'};Complete-IsolatedFbeRuntime -Isolation $iso -Passed $passed}
 }
 $manifest.status='PASS';$manifest|ConvertTo-Json -Depth 6|Set-Content (Join-Path $out 'manifest.json') -Encoding utf8
}finally{foreach($name in $names){[Environment]::SetEnvironmentVariable($name,$old[$name],'Process')}}
Write-Host 'C4 diagnostic probes PASS'
