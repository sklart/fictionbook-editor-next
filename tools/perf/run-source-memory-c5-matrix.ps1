[CmdletBinding()]
param(
 [Parameter(Mandatory)][string]$PythonExe,
 [Parameter(Mandatory)][string]$RuntimeDirectory,
 [Parameter(Mandatory)][string]$OutputDirectory,
 [string]$FixtureDirectory=(Join-Path $PSScriptRoot '../../out/perf/c5/fixtures'),
 [string[]]$Policies=@('document','page','caret','none'),
 [string[]]$Fixtures=@('many','large','control'),
 [int]$Repeats=7,[int]$ControlRepeats=3,
 [ValidateSet('none','layout')][string]$ReleaseOnBody='none'
)
$ErrorActionPreference='Stop'
$out=[IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Force $out|Out-Null
$configuration=[ordered]@{runtime=[IO.Path]::GetFullPath($RuntimeDirectory);fixtures=[IO.Path]::GetFullPath($FixtureDirectory);policies=$Policies;fixtureNames=$Fixtures;repeats=$Repeats;controlRepeats=$ControlRepeats;releaseOnBody=$ReleaseOnBody}
$configurationPath=Join-Path $out 'configuration.json'
if(Test-Path $configurationPath){
 $savedConfiguration=Get-Content $configurationPath -Raw|ConvertFrom-Json -AsHashtable
 if($savedConfiguration.Count -ne $configuration.Count){throw 'Matrix configuration inventory changed'}
 foreach($key in $configuration.Keys){if((ConvertTo-Json -InputObject $configuration[$key] -Compress) -cne (ConvertTo-Json -InputObject $savedConfiguration[$key] -Compress)){throw "Matrix configuration changed: $key"}}
}else{$configuration|ConvertTo-Json -Depth 4|Set-Content $configurationPath -Encoding utf8}
function Get-FrozenHashes {
 $result=@{}
 foreach($file in Get-ChildItem $RuntimeDirectory -Recurse -File){$result['runtime/'+[IO.Path]::GetRelativePath($RuntimeDirectory,$file.FullName)]=(Get-FileHash $file.FullName).Hash}
 foreach($fixture in $Fixtures){$result['fixture/'+$fixture]=(Get-FileHash (Join-Path $FixtureDirectory ($fixture+'.fb2'))).Hash}
 $result['workload']=(Get-FileHash (Join-Path $PSScriptRoot '../../src/fbe/testing/RuntimeTestSourceMemoryC5.inl')).Hash
 return $result
}
$freezePath=Join-Path $out 'freeze.json'
$hashes=Get-FrozenHashes
if(Test-Path $freezePath){
 $frozen=Get-Content $freezePath -Raw|ConvertFrom-Json -AsHashtable
 if($frozen.Count -ne $hashes.Count){throw 'Frozen input inventory changed'}
 foreach($key in $hashes.Keys){if($hashes[$key] -cne $frozen[$key]){throw "Frozen input changed: $key"}}
}else{$hashes|ConvertTo-Json -Depth 5|Set-Content $freezePath -Encoding utf8}
$plan=@()
for($round=0;$round -le $Repeats;$round++){
 for($fi=0;$fi -lt $Fixtures.Count;$fi++){
  $fixture=$Fixtures[($fi+$round)%$Fixtures.Count]
  if($fixture -eq 'control' -and $round -gt $ControlRepeats){continue}
  for($pi=0;$pi -lt $Policies.Count;$pi++){
   $policy=$Policies[($pi+$round+$fi)%$Policies.Count]
   $plan+=@{fixture=$fixture;cache=$policy;repeat=$round;warmup=($round -eq 0);directory=('{0}-{1}-{2:d2}' -f $fixture,$policy,$round)}
  }
 }
}
$plan|ConvertTo-Json -Depth 5|Set-Content (Join-Path $out 'plan.json') -Encoding utf8
$completed=0
foreach($run in $plan){
 $directory=Join-Path $out $run.directory;$manifestPath=Join-Path $directory 'manifest.json'
 if(Test-Path $manifestPath){
  $manifest=Get-Content $manifestPath -Raw|ConvertFrom-Json
  if($manifest.status -ne 'PASS' -or $manifest.equivalence -ne 'PASS'){throw "Interrupted/failed run requires review: $directory"}
  if($manifest.cache -ne $run.cache -or $manifest.fixture -ne $run.fixture -or $manifest.repeat -ne $run.repeat -or $manifest.releaseOnBody -ne $ReleaseOnBody){throw 'Saved process configuration changed'}
  if((Get-FileHash (Join-Path $directory 'report.txt')).Hash -cne $manifest.reportSha256){throw 'Saved report changed'}
 }else{
  & (Join-Path $PSScriptRoot 'run-source-memory-c5.ps1') -PythonExe $PythonExe -RuntimeDirectory $RuntimeDirectory -FixtureDirectory $FixtureDirectory -OutputDirectory $directory -Cache $run.cache -Fixture $run.fixture -Repeat $run.repeat -Warmup:$run.warmup -ReleaseOnBody $ReleaseOnBody
 }
 $completed++
 @{status='RUNNING';completed=$completed;total=$plan.Count;last=$run.directory}|ConvertTo-Json|Set-Content (Join-Path $out 'progress.json') -Encoding utf8
}
$after=Get-FrozenHashes
foreach($key in $hashes.Keys){if($after[$key] -cne $hashes[$key]){throw "Frozen input changed: $key"}}
@{status='PASS';completed=$completed;total=$plan.Count;equivalence='PASS';hashes='PASS';warmup=@($plan|Where-Object warmup).Count;measured=@($plan|Where-Object {-not$_.warmup}).Count}|ConvertTo-Json|Set-Content (Join-Path $out 'progress.json') -Encoding utf8
