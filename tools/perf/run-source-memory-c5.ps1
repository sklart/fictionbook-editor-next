[CmdletBinding()]
param(
 [ValidateSet('document','page','caret','none')][string]$Cache='document',
 [string]$Fixture='control',[int]$Repeat=1,[switch]$Warmup,
 [ValidateSet('none','layout')][string]$ReleaseOnBody='none',
 [string]$Implementation='current',
 [Parameter(Mandatory)][string]$PythonExe,
 [string]$RuntimeDirectory=(Join-Path $PSScriptRoot '../../out/perf/c5/diagnostic/runtime'),
 [string]$FixtureDirectory=(Join-Path $PSScriptRoot '../../out/perf/c5/fixtures'),
 [Parameter(Mandatory)][string]$OutputDirectory
)
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$out=[IO.Path]::GetFullPath($OutputDirectory)
if(Test-Path (Join-Path $out 'manifest.json')){throw 'Use a new C5 run output directory'}
New-Item -ItemType Directory -Force $out|Out-Null
. (Join-Path $root 'tools/tests/RuntimeTestIsolation.ps1')
$inputFile=Join-Path $FixtureDirectory ($Fixture+'.fb2');$inputHash=(Get-FileHash $inputFile).Hash
$manifest=@{status='RUNNING';fixture=$Fixture;cache=$Cache;wrap='word';repeat=$Repeat;warmup=[bool]$Warmup;releaseOnBody=$ReleaseOnBody;implementation=$Implementation;inputSha256=$inputHash;runtimeHashes=@{}}
foreach($file in @('FBE.exe','main.js','Scintilla.dll','Lexilla.dll')){$manifest.runtimeHashes[$file]=(Get-FileHash (Join-Path $RuntimeDirectory $file)).Hash}
$manifest|ConvertTo-Json -Depth 5|Set-Content (Join-Path $out 'manifest.json') -Encoding utf8
$names=@('FBE_NEXT_TEST_MODE','FBE_NEXT_TEST_SCENARIO','FBE_NEXT_SCRIPT_API_ROOT','FBE_NEXT_TEST_SETTINGS_DIRECTORY','FBE_NEXT_TRACE','FBE_NEXT_TRACE_VERBOSE','FBE_C5_INPUT','FBE_C5_LAYOUT_CACHE','FBE_C5_RELEASE_BODY','TEMP','TMP')
$old=@{};foreach($name in $names){$old[$name]=[Environment]::GetEnvironmentVariable($name,'Process')}
$iso=$null;$passed=$false
try {
 $temp=Join-Path $root 'out/perf/c5/tmp';New-Item -ItemType Directory -Force $temp|Out-Null;$env:TEMP=$temp;$env:TMP=$temp
 $iso=New-IsolatedFbeRuntime -FbeExe (Join-Path $RuntimeDirectory 'FBE.exe') -Name "c5-$Cache-$Fixture-$Repeat"
 $settingsPath=New-IsolatedFbeSettingsSeed $iso -StartupScriptsDirectory (Join-Path $iso.Root 'perf-scripts')
 [xml]$settings=Get-Content -LiteralPath $settingsPath -Raw
 $wrap=$settings.CreateElement('XMLSrcWrap');$wrap.InnerText='true'
 $null=$settings.SelectSingleNode('/FBE/Settings[@ID="0"]').AppendChild($wrap)
 $settings.Save($settingsPath)
 $book=Join-Path $iso.Root 'c5-input.fb2';Copy-Item -LiteralPath $inputFile -Destination $book
 $env:FBE_NEXT_TEST_MODE='1';$env:FBE_NEXT_TEST_SCENARIO='source-memory-c5';$env:FBE_NEXT_SCRIPT_API_ROOT=$iso.Root
 $env:FBE_NEXT_TEST_SETTINGS_DIRECTORY=Join-Path $iso.DataPath 'Settings';$env:FBE_NEXT_TRACE='1';$env:FBE_NEXT_TRACE_VERBOSE='0'
 $env:FBE_C5_INPUT=$book;$env:FBE_C5_LAYOUT_CACHE=$Cache;$env:FBE_C5_RELEASE_BODY=$ReleaseOnBody
 $report=Join-Path $iso.Root 'c5-report.txt'
 Write-Host "C5 Source implementation=$Implementation fixture=$Fixture cache=$Cache repeat=$Repeat warmup=$Warmup release=$ReleaseOnBody"
 $process=Start-Process $iso.Exe -WorkingDirectory $iso.Root -ArgumentList @('--portable','-b',$report) -WindowStyle Hidden -PassThru
 $manifest.processId=$process.Id
 $code=Wait-IsolatedFbeProcess -Process $process -Isolation $iso -Scenario source-memory-c5 -Report $report -TimeoutSeconds 600
 if(Test-Path $report){Copy-Item -LiteralPath $report -Destination (Join-Path $out 'report.txt')}
 $failedSource=Join-Path $iso.Root 'unexpected-source.xml'
 if(Test-Path -LiteralPath $failedSource){Copy-Item -LiteralPath $failedSource -Destination (Join-Path $out 'unexpected-source.xml')}
 $trace=Get-ChildItem (Join-Path $iso.DataPath 'Diagnostics') -Filter 'fbe-trace-*.log' -ErrorAction SilentlyContinue|Sort-Object LastWriteTime -Descending|Select-Object -First 1
 if($trace){Copy-Item -LiteralPath $trace.FullName -Destination (Join-Path $out 'trace.log')}
 if($code -ne 0 -or -not(Test-Path $report) -or (Get-Content $report -Raw) -notmatch '(?m)^result=PASS\r?$'){throw "C5 Source runtime failed: exit=$code"}
 if((Get-FileHash $book).Hash -cne $inputHash -or (Get-FileHash $inputFile).Hash -cne $inputHash){throw 'C5 fixture changed'}
 Copy-Item -LiteralPath (Join-Path $iso.Root 'c5-roundtrip.fb2') -Destination (Join-Path $out 'roundtrip.fb2')
 & $PythonExe (Join-Path $PSScriptRoot 'check-binary-store-c41-roundtrip.py') $inputFile (Join-Path $out 'roundtrip.fb2') --output (Join-Path $out 'equivalence.json')
 if($LASTEXITCODE){throw 'C5 independent equivalence failed'}
 # Keep complete output bytes once per SHA, rather than one 40+ MiB copy per process.
 $roundtrip=Join-Path $out 'roundtrip.fb2'
 $roundtripHash=(Get-FileHash -LiteralPath $roundtrip).Hash
 $objects=Join-Path $root 'out/perf/c5/roundtrip-objects'
 New-Item -ItemType Directory -Force $objects|Out-Null
 $artifact=Join-Path $objects ($roundtripHash.ToLowerInvariant()+'.fb2')
 if(Test-Path -LiteralPath $artifact){
  if((Get-FileHash -LiteralPath $artifact).Hash -cne $roundtripHash){throw 'Stored roundtrip object changed'}
  Remove-Item -LiteralPath $roundtrip -Force
 }else{Move-Item -LiteralPath $roundtrip -Destination $artifact}
 $manifest.roundtripSha256=$roundtripHash;$manifest.roundtripArtifact=$artifact
 $stages=@();$timings=@{};$shape=@{}
 foreach($line in Get-Content $report){
  if($line -notmatch '^(stage|time|shape)='){continue}
  $row=[ordered]@{}
  foreach($part in ($line -split ';')){$key,$value=$part -split '=',2;if($key -in @('stage','time','shape')){$row[$key]=$value}else{$row[$key]=[double]::Parse($value.Replace(',','.'),[Globalization.CultureInfo]::InvariantCulture)}}
  if($row.Contains('stage')){$stages += [pscustomobject]$row}elseif($row.Contains('time')){$timings[$row.time]=$row.milliseconds}else{$shape[$row.shape]=$row.value}
 }
 $wanted=@{document=3;page=2;caret=1;none=0}[$Cache]
 foreach($stage in $stages|Where-Object {$_.stage -in @('source_stable','second_source_stable')}){if($stage.layout_cache -ne $wanted){throw 'Source configuration reset the selected cache policy'}}
 foreach($required in @('open_body','source_generated','initial_layout_complete','source_stable','after_scroll','after_caret','after_find','after_replace','after_small_edit','body_after_source','body_idle','second_source_stable','return_body_idle','after_close')){if(-not($stages|Where-Object stage -eq $required)){throw "Missing C5 state: $required"}}
 foreach($required in @('body_to_source','first_layout','scroll_workload','caret_workload','find_workload','replace','small_edit','source_to_body','second_body_to_source')){if(-not$timings.ContainsKey($required)){throw "Missing C5 time: $required"}}
 @{stages=$stages;timings=$timings;shape=$shape}|ConvertTo-Json -Depth 6|Set-Content (Join-Path $out 'measurements.json') -Encoding utf8
 $manifest.status='PASS';$manifest.equivalence='PASS';$manifest.states=$stages.Count;$manifest.reportSha256=(Get-FileHash (Join-Path $out 'report.txt')).Hash;$passed=$true
}catch{$manifest.status='FAIL';$manifest.error=$_.Exception.Message;throw}
finally{
 $manifest|ConvertTo-Json -Depth 6|Set-Content (Join-Path $out 'manifest.json') -Encoding utf8
 foreach($name in $names){[Environment]::SetEnvironmentVariable($name,$old[$name],'Process')}
 if($iso){Complete-IsolatedFbeRuntime -Isolation $iso -Passed $passed}
}
Write-Host 'C5 Source workload and independent equivalence PASS'
