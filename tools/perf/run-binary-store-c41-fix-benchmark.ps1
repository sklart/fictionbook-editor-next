[CmdletBinding()]
param([Parameter(Mandatory)][string]$PythonExe,[switch]$Resume,
 [Parameter(Mandatory)][string]$OldScript,[Parameter(Mandatory)][string]$NewScript,
 [string]$DiagnosticDirectory=(Join-Path $PSScriptRoot '../../out/perf/c41/bstr-pilot-fresh-build/diagnostic-bin'),
 [string]$OutputDirectory=(Join-Path $PSScriptRoot '../../out/perf/c41/fix-benchmark'))
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$out=[IO.Path]::GetFullPath($OutputDirectory)
if((Test-Path (Join-Path $out 'manifest.json')) -and -not $Resume){throw 'Use a new benchmark output directory or explicit -Resume'}
if($Resume -and -not(Test-Path (Join-Path $out 'manifest.json'))){throw 'Resume manifest missing'}
if(@(Get-CimInstance Win32_Process -Filter "Name='MSBuild.exe'"|Where-Object {$_.CommandLine -like ('*'+$root+'*')}).Count){throw 'Finish repository builds before benchmarking'}
New-Item -ItemType Directory -Force $out|Out-Null
function Get-ProtectedHashes {
 $hashes=@{}
 $files=@(Get-ChildItem $DiagnosticDirectory -File|Where-Object {$_.Extension -in '.exe','.dll','.js'})
 $files+=@(Get-Item $OldScript,$NewScript,(Join-Path $root 'runtime/main.js'))
 $files+=@(Get-ChildItem (Join-Path $root 'src/fbe') -Recurse -File|Where-Object {$_.Extension -in '.cpp','.h','.inl'})
 $files+=@(Get-Item (Join-Path $root 'out/perf/results-2f0113b2-final64-uiwait/many.fb2'),(Join-Path $root 'out/perf/results-2f0113b2-final64-uiwait/large.fb2'))
 foreach($file in $files){$hashes[$file.FullName]=(Get-FileHash $file.FullName).Hash}
 return $hashes
}
$current=Get-ProtectedHashes
if($Resume){
 $manifest=Get-Content (Join-Path $out 'manifest.json') -Raw|ConvertFrom-Json -AsHashtable
 if($manifest.repeats -ne 7 -or $manifest.expected -ne 28){throw 'Resume protocol mismatch'}
 $before=$manifest.hashesBefore
 if($current.Count -ne $before.Count){throw 'Resume protected file set changed'}
 foreach($name in $before.Keys){if($current[$name] -cne $before[$name]){throw "Resume source/fixture/runtime mismatch: $name"}}
 $manifest.resumptions=@($manifest.resumptions)+@{utc=[DateTime]::UtcNow.ToString('o');completedBeforeResume=$manifest.results.Count;previousError=$manifest.error;hashes='UNCHANGED'}
 $manifest.Remove('error');$manifest.status='RUNNING'
}else{
 $before=$current
 $manifest=@{status='RUNNING';kind='C4.1 targeted Open/Source/Body old-new';repeats=7;expected=28;results=@();hashesBefore=$before}
}
$manifest|ConvertTo-Json -Depth 6|Set-Content (Join-Path $out 'manifest.json') -Encoding utf8
try {
 for($repeat=1;$repeat -le 7;++$repeat){
  $variants=if($repeat%2){@('old','new')}else{@('new','old')}
  $fixtures=if($repeat%2){@('many','large')}else{@('large','many')}
  foreach($fixture in $fixtures){foreach($variant in $variants){
   $scriptPath=if($variant -eq 'old'){$OldScript}else{$NewScript}
   $directory=Join-Path $out "r$repeat-$variant-$fixture"
   $completed=@($manifest.results|Where-Object {$_.repeat -eq $repeat -and $_.variant -eq $variant -and $_.fixture -eq $fixture})
   if($completed.Count -gt 1){throw 'Duplicate completed run in resume manifest'}
   if($completed.Count){
    if(-not$Resume){throw 'Unexpected preexisting run'}
    $saved=$completed[0]
    if($saved.status -ne 'PASS' -or $saved.equivalence -ne 'PASS' -or $saved.directory -ine $directory){throw 'Invalid completed run metadata'}
    $child=Get-Content (Join-Path $directory 'manifest.json') -Raw|ConvertFrom-Json
    $eq=Get-Content (Join-Path $directory 'equivalence.json') -Raw|ConvertFrom-Json
    if($child.status -ne 'PASS' -or $eq.status -ne 'PASS'){throw 'Completed run evidence not PASS'}
    if((Get-FileHash (Join-Path $directory "$fixture-1-report.txt")).Hash -cne $child.results[0].reportSha256){throw 'Completed report changed'}
    if((Get-FileHash $scriptPath).Hash -cne $child.results[0].effectiveMainJsSha256){throw 'Completed script mismatch'}
    if((Get-FileHash (Join-Path $directory "$fixture-1-roundtrip.fb2")).Hash.ToLowerInvariant() -cne $eq.actual_file_sha256){throw 'Completed roundtrip changed'}
    Write-Host "Verified existing PASS repeat=$repeat implementation=$variant fixture=$fixture"
    continue
   }
   Write-Host "C4.1 fix benchmark repeat=$repeat implementation=$variant fixture=$fixture"
   & (Join-Path $PSScriptRoot 'run-binary-store-c41-probe.ps1') -Experiment source -Fixtures $fixture -Repeats 1 -DiagnosticDirectory $DiagnosticDirectory -MainScriptPath $scriptPath -OutputDirectory $directory *> (Join-Path $out "r$repeat-$variant-$fixture.log")
   $run=Get-Content (Join-Path $directory 'manifest.json') -Raw|ConvertFrom-Json
   if($run.status -ne 'PASS'){throw 'Targeted memory timeline failed'}
   $eq=Join-Path $directory 'equivalence.json'
   & $PythonExe (Join-Path $PSScriptRoot 'check-binary-store-c41-roundtrip.py') (Join-Path $root "out/perf/results-2f0113b2-final64-uiwait/$fixture.fb2") (Join-Path $directory "$fixture-1-roundtrip.fb2") --output $eq
   if($LASTEXITCODE){throw 'Independent roundtrip equivalence failed'}
   $manifest.results+=@{repeat=$repeat;variant=$variant;fixture=$fixture;directory=$directory;status='PASS';equivalence='PASS';mainJsSha256=$run.results[0].effectiveMainJsSha256}
   $manifest|ConvertTo-Json -Depth 6|Set-Content (Join-Path $out 'manifest.json') -Encoding utf8
  }}
 }
 $after=Get-ProtectedHashes;$manifest.hashesAfter=$after
 foreach($name in $before.Keys){if($after[$name] -cne $before[$name]){throw "Protected source/fixture/runtime changed: $name"}}
 if($manifest.results.Count -ne 28){throw 'Incomplete targeted benchmark'}
 $manifest.status='PASS';$manifest.equivalence='28/28 PASS';$manifest.hashes='UNCHANGED'
}catch{$manifest.status='FAIL';$manifest.error=$_.Exception.Message;throw}
finally{$manifest|ConvertTo-Json -Depth 6|Set-Content (Join-Path $out 'manifest.json') -Encoding utf8}
Write-Host 'C4.1 targeted fix benchmark 28/28 PASS; equivalence 28/28 PASS; protected hashes unchanged'
