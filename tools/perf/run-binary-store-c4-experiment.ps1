[CmdletBinding()]
param(
 [ValidateSet('memory-cause','open-recheck','profile-js','protocol-check')][string]$Experiment='memory-cause',
 [string]$OutputDirectory=(Join-Path $PSScriptRoot '../../out/perf/c4/experiments'),
 [string[]]$Variants=@(),[int]$MeasuredRuns=0,
 [ValidateSet('ready','pumped','legacy')][string]$Protocol='pumped'
)
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path;$out=[IO.Path]::GetFullPath($OutputDirectory)
if(Test-Path (Join-Path $out 'manifest.json')){throw 'Use new output directory'}
New-Item -ItemType Directory -Force $out|Out-Null
. (Join-Path $root 'tools/tests/RuntimeTestIsolation.ps1')
. (Join-Path $root 'tools/build/Import-VsDevEnvironment.ps1') -Arch x86 -HostArch x64 -PlatformToolset v143 -VcVarsVersion 14.44
Push-Location $out
try{& cl.exe /nologo /O2 /EHsc /std:c++17 /MT /W4 /WX /DUNICODE /D_UNICODE (Join-Path $PSScriptRoot 'binary-store-c4-profile-driver.cpp') /Fe:profile-driver.exe;if($LASTEXITCODE){throw 'C4 controller compile failed'}}finally{Pop-Location}
if(-not $Variants.Count){$Variants=if($Experiment -in @('open-recheck','profile-js')){@('c2','c3')}elseif($Experiment -eq 'protocol-check'){@('c3')}else{@('baseline','c2','c3')}}
if(-not $MeasuredRuns){$MeasuredRuns=if($Experiment -eq 'open-recheck'){7}elseif($Experiment -eq 'protocol-check'){1}else{3}}
$fixtures=if($Experiment -in @('open-recheck','profile-js')){@('control','many','large')}elseif($Experiment -eq 'protocol-check'){@('control')}else{@('many')}
$modes=if($Experiment -eq 'memory-cause'){@('default','periodic-gc','no-bstr-cache')}elseif($Experiment -eq 'profile-js'){@('profile')}else{@('default')}
$warmups=if($Experiment -eq 'protocol-check'){0}else{1}
$paths=@{baseline='baseline';c2='c2';c3='c3-2f0113b2'}
$expected=Get-Content (Join-Path $root 'out/perf/results-2f0113b2-final64-uiwait/manifest.json') -Raw|ConvertFrom-Json -AsHashtable
function Assert-Equivalent([string]$path,[string]$name){
 $cache=New-Object -ComObject Msxml2.XMLSchemaCache.6.0;$cache.add('http://www.gribuser.ru/xml/fictionbook/2.0',(Join-Path $root 'runtime/FictionBook.xsd'))
 $dom=New-Object -ComObject Msxml2.DOMDocument.6.0;$dom.async=$false;if(-not $dom.load($path)){throw 'Saved XML invalid'};$dom.schemas=$cache;if($dom.validate().errorCode){throw 'Saved XSD invalid'}
 [xml]$xml=[IO.File]::ReadAllText($path);$ns=[Xml.XmlNamespaceManager]::new($xml.NameTable);$ns.AddNamespace('f','http://www.gribuser.ru/xml/fictionbook/2.0');$ns.AddNamespace('l','http://www.w3.org/1999/xlink')
 $wanted=$expected.fixtures[$name].expected;$nodes=$xml.SelectNodes('/f:FictionBook/f:binary',$ns);if($nodes.Count -ne $wanted.Count){throw 'Saved binary count changed'}
 foreach($node in $nodes){if($wanted.Keys -cnotcontains $node.id){throw 'Saved ID changed'};$bytes=[Convert]::FromBase64String($node.InnerText);$hash=[Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($bytes));if($hash -cne $wanted[$node.id].hash -or $node.GetAttribute('content-type') -cne $wanted[$node.id].mime){throw 'Saved bytes/MIME changed'}}
 $refs=@($xml.SelectNodes('//@l:href',$ns)|ForEach-Object Value);if(($refs -join "`n") -cne ($expected.fixtures[$name].references -join "`n")){throw 'Saved references changed'}
}
$names=@('FBE_NEXT_TEST_MODE','FBE_NEXT_TEST_SCENARIO','FBE_NEXT_SCRIPT_API_ROOT','FBE_NEXT_TEST_SETTINGS_DIRECTORY','FBE_NEXT_TRACE','FBE_NEXT_TRACE_VERBOSE','OANOCACHE','TEMP','TMP');$old=@{};foreach($name in $names){$old[$name]=[Environment]::GetEnvironmentVariable($name,'Process');if($name -notin @('TEMP','TMP')){[Environment]::SetEnvironmentVariable($name,$null,'Process')}}
$manifest=@{status='RUNNING';experiment=$Experiment;protocol=$Protocol;measuredRepeats=$MeasuredRuns;warmups=$warmups;modes=$modes;variants=$Variants;fixtures=$fixtures;driverSha256=(Get-FileHash (Join-Path $out 'profile-driver.exe')).Hash;sourceSha256=(Get-FileHash (Join-Path $PSScriptRoot 'binary-store-c4-profile-driver.cpp')).Hash;overlays='only isolated runtime copies; snapshots unchanged';results=@()}
$manifest|ConvertTo-Json -Depth 5|Set-Content (Join-Path $out 'manifest.json') -Encoding utf8
$rows=[Collections.Generic.List[object]]::new();$isolations=@{};$passed=$false
try{
 $temp=Join-Path $root 'out/perf/c4/tmp';New-Item -ItemType Directory -Force $temp|Out-Null;$env:TEMP=$temp;$env:TMP=$temp
 foreach($v in $Variants){$iso=New-IsolatedFbeRuntime -FbeExe (Join-Path $root ('out/perf/'+$paths[$v]+'/FBE.exe')) -Name "c4-exp-$v";$isolations[$v]=$iso}
 for($round=0;$round -lt ($MeasuredRuns+$warmups);++$round){
  $order=if($round%2){@($Variants|Sort-Object -Descending)}else{@($Variants|Sort-Object)}
  foreach($fixture in $fixtures){foreach($mode in $modes){foreach($v in $order){
   $iso=$isolations[$v];$null=New-IsolatedFbeSettingsSeed $iso -StartupScriptsDirectory (Join-Path $iso.Root 'perf-scripts')
   $original=[IO.File]::ReadAllText((Join-Path $root ('out/perf/'+$paths[$v]+'/main.js')));$js=$original;$profile=$false
   if($mode -eq 'periodic-gc'){
    if(-not $js.Contains('  success++;')){throw 'PutBinaries GC anchor missing'}
    $js=$js.Replace('  success++;','  success++; if(((i+1)%50)==0){CollectGarbage();++_c4_periodic_gc_count;}')
    $js+="`nvar _c4_periodic_gc_count=0; function _c4_profile_json(){return '{'+String.fromCharCode(34)+'periodic_gc_calls'+String.fromCharCode(34)+':'+_c4_periodic_gc_count+'}';}`n";$profile=$true
   }
   if($mode -eq 'profile'){
    $js=$js.Replace('document.all.binobj.getElementsByTagName("DIV")','_c4_binary_enumeration()')
    $js=$js.Replace('window.external._BinaryStoreChanged("attach", div);','var _c4_attach_start=new Date().getTime();window.external._BinaryStoreChanged("attach", div);_c4_record_phase("attach_bridge",new Date().getTime()-_c4_attach_start,0);')
    $js=[regex]::Replace($js,'div\.base64data\s*=\s*data;','var _c4_put_start=new Date().getTime();$0 _c4_record_phase("facade_put_bridge",new Date().getTime()-_c4_put_start,0);')
    $js+="`n"+[IO.File]::ReadAllText((Join-Path $root 'tools/perf/binary-store-c4-open-profile.js'));$profile=$true
   }
   [IO.File]::WriteAllText((Join-Path $iso.Root 'main.js'),$js,[Text.UTF8Encoding]::new($false))
   $env:OANOCACHE=if($mode -eq 'no-bstr-cache'){'1'}else{$null}
   $book=Join-Path $iso.Root 'run-input.fb2';Copy-Item (Join-Path $root "out/perf/results-2f0113b2-final64-uiwait/$fixture.fb2") $book -Force
   $prefix="$fixture-$mode-$round-$v";$report=Join-Path $out "$prefix.tsv";$err=Join-Path $out "$prefix.stderr.txt"
   $collect=if($Experiment -eq 'memory-cause'){'1'}else{'0'};$save=$collect;$profileFlag=if($profile){'1'}else{'0'}
   Write-Host "C4 $Experiment $fixture mode=$mode round=$round variant=$v"
   & (Join-Path $out 'profile-driver.exe') $iso.Exe $book ([string]$expected.fixtures[$fixture].binaryCount) $report $Protocol $collect $profileFlag $save 2> $err
   if($LASTEXITCODE){throw "C4 driver failed $prefix"}
   if($save -eq '1'){Assert-Equivalent $book $fixture}
   if($mode -eq 'periodic-gc'){$p=Get-Content "$report.profile.json" -Raw|ConvertFrom-Json;if($p.periodic_gc_calls -ne [math]::Floor($expected.fixtures[$fixture].binaryCount/50)){throw 'GC overlay did not execute as expected'}}
   foreach($line in [IO.File]::ReadAllLines($report)){$f=$line.Split("`t");if($f[0] -notin @('time','memory')){continue};$rows.Add([pscustomobject]@{fixture=$fixture;mode=$mode;round=$round;warmup=($round -lt $warmups);variant=$v;kind=$f[0];metric=$f[1];value=[double]::Parse($f[2],[Globalization.CultureInfo]::InvariantCulture)})}
   $manifest.results+=@{fixture=$fixture;mode=$mode;round=$round;variant=$v;status='PASS';equivalence=if($save -eq '1'){'PASS XSD/binary SHA256/MIME/references'}else{'Open-only, no save requested'};overlaySha256=(Get-FileHash (Join-Path $iso.Root 'main.js')).Hash;exeSha256=(Get-FileHash $iso.Exe).Hash;outputSha256=if($save -eq '1'){(Get-FileHash $book).Hash}else{$null}}
   $rows|ConvertTo-Json -Depth 4|Set-Content (Join-Path $out 'raw.json') -Encoding utf8
  }}}
 }
 $manifest.status='PASS';$manifest.completedWarmups=@($manifest.results|Where-Object round -lt $warmups).Count;$manifest.completedMeasured=@($manifest.results|Where-Object round -ge $warmups).Count;$passed=$true
} catch {$manifest.status='FAIL';$manifest.error=$_.Exception.Message;throw}
finally{
 $manifest|ConvertTo-Json -Depth 6|Set-Content (Join-Path $out 'manifest.json') -Encoding utf8
 foreach($iso in $isolations.Values){if(-not [IO.Path]::GetFullPath($iso.Root).StartsWith([IO.Path]::GetFullPath($temp).TrimEnd('\')+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Unexpected cleanup root'};Complete-IsolatedFbeRuntime -Isolation $iso -Passed $passed}
 foreach($name in $names){[Environment]::SetEnvironmentVariable($name,$old[$name],'Process')}
}
Write-Host "C4 $Experiment PASS"
