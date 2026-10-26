[CmdletBinding()]
param(
 [ValidateSet('timeline','content','source')][string]$Experiment='timeline',
 [string[]]$Fixtures=@(),[int]$Repeats=3,[string]$ScriptOverlay,[string]$MainScriptPath,
 [string]$DiagnosticDirectory=(Join-Path $PSScriptRoot '../../out/perf/c41/diagnostic-bin'),
 [string]$OutputDirectory=(Join-Path $PSScriptRoot '../../out/perf/c41/timeline')
)
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$out=[IO.Path]::GetFullPath($OutputDirectory)
if(Test-Path (Join-Path $out 'manifest.json')){throw 'Use a new output directory'}
if($Repeats -lt 1){throw 'Repeats must be positive'}
New-Item -ItemType Directory -Force $out|Out-Null
. (Join-Path $root 'tools/tests/RuntimeTestIsolation.ps1')
if(-not $Fixtures.Count){$Fixtures=if($Experiment -eq 'content'){@('many-identical','many-unique')}else{@('many','large')}}
$paths=@{many='out/perf/results-2f0113b2-final64-uiwait/many.fb2';large='out/perf/results-2f0113b2-final64-uiwait/large.fb2';control='out/perf/results-2f0113b2-final64-uiwait/control.fb2';'many-identical'='out/perf/c4/scaling-open-only/n500-32m.fb2';'many-unique'='out/perf/c4/scaling-unique/n500-32m.fb2'}
$names=@('FBE_NEXT_TEST_MODE','FBE_NEXT_TEST_SCENARIO','FBE_NEXT_SCRIPT_API_ROOT','FBE_NEXT_TEST_SETTINGS_DIRECTORY','FBE_NEXT_TRACE','FBE_NEXT_TRACE_VERBOSE','FBE_C41_INPUT','FBE_C41_MODE','TEMP','TMP')
$old=@{};foreach($name in $names){$old[$name]=[Environment]::GetEnvironmentVariable($name,'Process')}
$exe=Join-Path $DiagnosticDirectory 'FBE.exe'
$manifest=@{status='RUNNING';experiment=$Experiment;repeats=$Repeats;fixtures=$Fixtures;base='2f0113b2566a3510c74e7e1c4536e9d10e4be0c8';exeSha256=(Get-FileHash $exe).Hash;mainJsSha256=(Get-FileHash (Join-Path $DiagnosticDirectory 'main.js')).Hash;results=@()}
$manifest|ConvertTo-Json -Depth 5|Set-Content (Join-Path $out 'manifest.json') -Encoding utf8
try {
 $temp=Join-Path $root 'out/perf/c41/tmp';New-Item -ItemType Directory -Force $temp|Out-Null;$env:TEMP=$temp;$env:TMP=$temp
 for($repeat=1;$repeat -le $Repeats;++$repeat){
  $order=if($repeat%2){$Fixtures}else{@($Fixtures|Sort-Object -Descending)}
  foreach($fixtureName in $order){
   if(-not $paths.ContainsKey($fixtureName)){throw "Unknown fixture $fixtureName"}
   $source=Join-Path $root $paths[$fixtureName];$hash=(Get-FileHash $source).Hash
   $iso=New-IsolatedFbeRuntime -FbeExe $exe -Name "c41-$fixtureName-$repeat";$passed=$false
   try {
    $null=New-IsolatedFbeSettingsSeed $iso -StartupScriptsDirectory (Join-Path $iso.Root 'perf-scripts')
    if($MainScriptPath){Copy-Item -LiteralPath $MainScriptPath -Destination (Join-Path $iso.Root 'main.js') -Force}
    if($ScriptOverlay){Add-Content -LiteralPath (Join-Path $iso.Root 'main.js') -Value ([IO.File]::ReadAllText((Resolve-Path $ScriptOverlay).Path)) -Encoding utf8}
    $book=Join-Path $iso.Root 'run-input.fb2';Copy-Item $source $book
    $env:FBE_NEXT_TEST_MODE='1';$env:FBE_NEXT_TEST_SCENARIO='binary-store-c41';$env:FBE_NEXT_SCRIPT_API_ROOT=$iso.Root
    $env:FBE_NEXT_TEST_SETTINGS_DIRECTORY=Join-Path $iso.DataPath 'Settings';$env:FBE_NEXT_TRACE='1';$env:FBE_NEXT_TRACE_VERBOSE='0'
    $env:FBE_C41_INPUT=$book;$env:FBE_C41_MODE=switch($Experiment){content{'content-only'}source{'source-only'}default{'timeline'}}
    $report=Join-Path $iso.Root 'c41-report.txt';Write-Host "C4.1 $Experiment $fixtureName repeat=$repeat"
    $process=Start-Process $iso.Exe -WorkingDirectory $iso.Root -ArgumentList @('--portable','-b',$report) -WindowStyle Hidden -PassThru
    $code=Wait-IsolatedFbeProcess -Process $process -Isolation $iso -Scenario binary-store-c41 -Report $report -TimeoutSeconds 600
    $saved=Join-Path $out "$fixtureName-$repeat-report.txt"
    if(Test-Path $report){Copy-Item $report $saved}
    $trace=Get-ChildItem (Join-Path $iso.DataPath 'Diagnostics') -Filter 'fbe-trace-*.log' -ErrorAction SilentlyContinue|Sort-Object LastWriteTime -Descending|Select-Object -First 1
    if($trace){Copy-Item $trace.FullName (Join-Path $out "$fixtureName-$repeat-trace.log")}
    if($code -ne 0 -or -not(Test-Path $report) -or (Get-Content $report -Raw) -notmatch '(?m)^result=PASS\r?$'){throw "C4.1 probe failed: $fixtureName/$repeat exit=$code"}
    if((Get-FileHash $book).Hash -cne $hash -or (Get-FileHash $source).Hash -cne $hash){throw 'Fixture changed'}
    if($Experiment -eq 'source'){Copy-Item -LiteralPath (Join-Path $iso.Root 'c41-roundtrip.fb2') -Destination (Join-Path $out "$fixtureName-$repeat-roundtrip.fb2")}
    $stages=@(Get-Content $report|Where-Object {$_ -like 'stage=*'}|ForEach-Object {$row=[ordered]@{};foreach($part in ($_ -split ';')){$key,$value=$part -split '=',2;$row[$key]=if($key -eq 'stage'){$value}else{[long]::Parse($value,[Globalization.CultureInfo]::InvariantCulture)}};[pscustomobject]$row})
    $stages|ConvertTo-Json -Depth 4|Set-Content (Join-Path $out "$fixtureName-$repeat-stages.json") -Encoding utf8
    $manifest.results+=@{fixture=$fixtureName;repeat=$repeat;status='PASS';inputSha256=$hash;reportSha256=(Get-FileHash $saved).Hash;stages=$stages.Count;processId=$process.Id;heapTrace=$false;effectiveMainJsSha256=(Get-FileHash (Join-Path $iso.Root 'main.js')).Hash;overlaySha256=if($ScriptOverlay){(Get-FileHash $ScriptOverlay).Hash}else{$null};runtimePath=$null};$passed=$true
    $manifest|ConvertTo-Json -Depth 6|Set-Content (Join-Path $out 'manifest.json') -Encoding utf8
   }finally{
    if(-not [IO.Path]::GetFullPath($iso.Root).StartsWith([IO.Path]::GetFullPath($temp).TrimEnd('\')+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Unexpected cleanup root'}
    Complete-IsolatedFbeRuntime -Isolation $iso -Passed $passed
   }
  }
 }
 $manifest.status='PASS'
}catch{$manifest.status='FAIL';$manifest.error=$_.Exception.Message;throw}
finally{$manifest|ConvertTo-Json -Depth 6|Set-Content (Join-Path $out 'manifest.json') -Encoding utf8;foreach($name in $names){[Environment]::SetEnvironmentVariable($name,$old[$name],'Process')}}
Write-Host 'C4.1 probes PASS'
