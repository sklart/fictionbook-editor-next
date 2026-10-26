[CmdletBinding()]
param(
 [Parameter(Mandatory)][string]$PythonExe,
 [Parameter(Mandatory)][string]$OldRuntimeDirectory,
 [Parameter(Mandatory)][string]$NewRuntimeDirectory,
 [Parameter(Mandatory)][string]$OutputDirectory,
 [string]$FixtureDirectory=(Join-Path $PSScriptRoot '../../out/perf/c5/fixtures'),
 [string[]]$Fixtures=@('many','large','control'),
 [int]$Repeats=7,[int]$ControlRepeats=3,
 [ValidateSet('none','layout')][string]$NewReleaseOnBody='none'
)
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$out=[IO.Path]::GetFullPath($OutputDirectory)
$runtimes=@{old=(Resolve-Path $OldRuntimeDirectory).Path;new=(Resolve-Path $NewRuntimeDirectory).Path}
New-Item -ItemType Directory -Force $out|Out-Null
$configuration=[ordered]@{old=$runtimes.old;new=$runtimes.new;fixtures=[IO.Path]::GetFullPath($FixtureDirectory);fixtureNames=$Fixtures;repeats=$Repeats;controlRepeats=$ControlRepeats;newReleaseOnBody=$NewReleaseOnBody}
$configurationPath=Join-Path $out 'configuration.json'
if(Test-Path $configurationPath){
 $saved=Get-Content $configurationPath -Raw|ConvertFrom-Json -AsHashtable
 if($saved.Count -ne $configuration.Count){throw 'Comparison configuration inventory changed'}
 foreach($key in $configuration.Keys){if((ConvertTo-Json -InputObject $configuration[$key] -Compress) -cne (ConvertTo-Json -InputObject $saved[$key] -Compress)){throw "Comparison configuration changed: $key"}}
}else{$configuration|ConvertTo-Json -Depth 4|Set-Content $configurationPath -Encoding utf8}
function Get-ComparisonHashes {
 $result=@{}
 foreach($implementation in 'old','new'){
  foreach($file in Get-ChildItem -LiteralPath $runtimes[$implementation] -Recurse -File){$result[$implementation+'/'+[IO.Path]::GetRelativePath($runtimes[$implementation],$file.FullName)]=(Get-FileHash -LiteralPath $file.FullName).Hash}
 }
 foreach($fixture in $Fixtures){$result['fixture/'+$fixture]=(Get-FileHash -LiteralPath (Join-Path $FixtureDirectory ($fixture+'.fb2'))).Hash}
 foreach($file in Get-ChildItem -LiteralPath (Join-Path $root 'src/fbe') -Recurse -File|Where-Object {$_.Extension -in '.cpp','.h','.inl'}){$result['source/'+[IO.Path]::GetRelativePath($root,$file.FullName)]=(Get-FileHash -LiteralPath $file.FullName).Hash}
 foreach($file in @('tools/perf/run-source-memory-c5.ps1','tools/perf/compare-source-memory-c5.ps1','tools/perf/check-binary-store-c41-roundtrip.py','tools/tests/RuntimeTestIsolation.ps1')){$result[$file]=(Get-FileHash -LiteralPath (Join-Path $root $file)).Hash}
 return $result
}
$hashes=Get-ComparisonHashes
$freezePath=Join-Path $out 'freeze.json'
if(Test-Path $freezePath){
 $saved=Get-Content $freezePath -Raw|ConvertFrom-Json -AsHashtable
 if($saved.Count -ne $hashes.Count){throw 'Frozen comparison inventory changed'}
 foreach($key in $hashes.Keys){if($saved[$key] -cne $hashes[$key]){throw "Frozen comparison input changed: $key"}}
}else{$hashes|ConvertTo-Json -Depth 4|Set-Content $freezePath -Encoding utf8}
$plan=@()
for($round=0;$round -le $Repeats;$round++){
 for($fi=0;$fi -lt $Fixtures.Count;$fi++){
  $fixture=$Fixtures[($fi+$round)%$Fixtures.Count]
  if($fixture -eq 'control' -and $round -gt $ControlRepeats){continue}
  $fixtureIndex=[Array]::IndexOf($Fixtures,$fixture)
  $order=if(($round+$fixtureIndex)%2){@('new','old')}else{@('old','new')}
  foreach($implementation in $order){$plan+=@{fixture=$fixture;implementation=$implementation;repeat=$round;warmup=($round -eq 0);directory=('{0}-{1}-{2:d2}' -f $fixture,$implementation,$round)}}
 }
}
$plan|ConvertTo-Json -Depth 5|Set-Content (Join-Path $out 'plan.json') -Encoding utf8
$completed=0;$artifacts=@{}
try {
 foreach($run in $plan){
  $directory=Join-Path $out $run.directory;$manifestPath=Join-Path $directory 'manifest.json'
  $release=if($run.implementation -eq 'new'){$NewReleaseOnBody}else{'none'}
  if(-not(Test-Path -LiteralPath $manifestPath)){
   & (Join-Path $PSScriptRoot 'run-source-memory-c5.ps1') -PythonExe $PythonExe -RuntimeDirectory $runtimes[$run.implementation] -FixtureDirectory $FixtureDirectory -OutputDirectory $directory -Cache document -Fixture $run.fixture -Repeat $run.repeat -Warmup:$run.warmup -ReleaseOnBody $release -Implementation $run.implementation
  }
  $manifest=Get-Content -LiteralPath $manifestPath -Raw|ConvertFrom-Json
  if($manifest.status -ne 'PASS' -or $manifest.equivalence -ne 'PASS'){throw "Incomplete/failed comparison process: $directory"}
  if($manifest.implementation -ne $run.implementation -or $manifest.fixture -ne $run.fixture -or $manifest.repeat -ne $run.repeat -or $manifest.warmup -ne $run.warmup -or $manifest.releaseOnBody -ne $release){throw 'Saved process configuration differs'}
  if((Get-FileHash -LiteralPath (Join-Path $directory 'report.txt')).Hash -cne $manifest.reportSha256){throw 'Saved native report changed'}
  if($manifest.inputSha256 -cne $hashes['fixture/'+$run.fixture]){throw 'Process used a different fixture'}
  foreach($file in 'FBE.exe','main.js','Scintilla.dll','Lexilla.dll'){if($manifest.runtimeHashes.$file -cne $hashes[$run.implementation+'/'+$file]){throw 'Process used a different runtime'}}
  $artifacts[$manifest.roundtripArtifact]=$manifest.roundtripSha256
  $completed++
  @{status='RUNNING';completed=$completed;total=$plan.Count;last=$run.directory}|ConvertTo-Json|Set-Content (Join-Path $out 'progress.json') -Encoding utf8
 }
 $after=Get-ComparisonHashes
 if($after.Count -ne $hashes.Count){throw 'Comparison input inventory changed'}
 foreach($key in $hashes.Keys){if($after[$key] -cne $hashes[$key]){throw "Comparison input changed: $key"}}
 foreach($artifact in $artifacts.Keys){if((Get-FileHash -LiteralPath $artifact).Hash -cne $artifacts[$artifact]){throw 'Roundtrip object changed'}}
 @{status='PASS';completed=$completed;total=$plan.Count;warmup=@($plan|Where-Object warmup).Count;measured=@($plan|Where-Object {-not$_.warmup}).Count;equivalence='PASS';hashes='PASS';hashCount=$hashes.Count;uniqueRoundtripArtifacts=$artifacts.Count}|ConvertTo-Json|Set-Content (Join-Path $out 'progress.json') -Encoding utf8
}catch{
 @{status='FAIL';completed=$completed;total=$plan.Count;error=$_.Exception.Message}|ConvertTo-Json|Set-Content (Join-Path $out 'progress.json') -Encoding utf8
 throw
}
