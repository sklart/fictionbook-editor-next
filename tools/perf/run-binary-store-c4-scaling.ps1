[CmdletBinding()]
param([string]$OutputDirectory=(Join-Path $PSScriptRoot '../../out/perf/c4/scaling'))
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$out=[IO.Path]::GetFullPath($OutputDirectory)
if(Test-Path (Join-Path $out 'manifest.json')){throw 'Use a new output directory'}
New-Item -ItemType Directory -Force $out | Out-Null
. (Join-Path $root 'tools/tests/RuntimeTestIsolation.ps1')
. (Join-Path $root 'tools/build/Import-VsDevEnvironment.ps1') -Arch x86 -HostArch x64 -PlatformToolset v143 -VcVarsVersion 14.44
Push-Location $out
try { & cl.exe /nologo /O2 /EHsc /std:c++17 /MT /W4 /WX /DUNICODE /D_UNICODE (Join-Path $PSScriptRoot 'binary-store-c4-driver.cpp') /Fe:open-driver.exe; if($LASTEXITCODE){throw 'Controller compile failed'} }finally{Pop-Location}
$source=Join-Path $root 'out/perf/results-2f0113b2-final64-uiwait'
$control=[IO.File]::ReadAllText((Join-Path $source 'control.fb2'))
$template=$control.Substring(0,$control.IndexOf('<binary '))
$empty=$template.Replace('<coverpage><image l:href="#image1.png"/></coverpage>','').Replace('<image l:href="#image1.png"/>','').Replace('<image l:href="#photo.png"/>','')
[xml]$xml=$control
$ns=[Xml.XmlNamespaceManager]::new($xml.NameTable);$ns.AddNamespace('f','http://www.gribuser.ru/xml/fictionbook/2.0')
$seed=[Convert]::FromBase64String($xml.SelectSingleNode('/f:FictionBook/f:binary',$ns).InnerText)
$cases=@([pscustomobject]@{name='n002-8k';count=2;bytes=8192})
foreach($n in 50,100,250,500){$cases += [pscustomobject]@{name=('n{0:d3}-32m' -f $n);count=$n;bytes=32768000}}
$cases += [pscustomobject]@{name='n500-4m';count=500;bytes=4194304}
foreach($case in $cases){
 $path=Join-Path $out ($case.name+'.fb2');$writer=[IO.StreamWriter]::new($path,$false,[Text.UTF8Encoding]::new($false))
 try {$writer.Write($(if($case.count){$template}else{$empty}));for($i=0;$i -lt $case.count;$i++){
  $size=[int][Math]::Floor($case.bytes/$case.count)+$(if($i -lt ($case.bytes%$case.count)){1}else{0})
  $bytes=[byte[]]::new($size);[Array]::Copy($seed,$bytes,[Math]::Min($size,$seed.Length)); [BitConverter]::GetBytes([int]$i).CopyTo($bytes,$bytes.Length-4)
  $id=if($i -eq 0){'image1.png'}elseif($i -eq 1){'photo.png'}else{"payload-$i.bin"};$mime=if($i -lt 2){'image/png'}else{'application/octet-stream'}
  $writer.Write('<binary id="'+$id+'" content-type="'+$mime+'">'+[Convert]::ToBase64String($bytes)+'</binary>')
 };$writer.Write('</FictionBook>')}finally{$writer.Dispose()}
 $case|Add-Member NoteProperty path $path;$case|Add-Member NoteProperty sha256 (Get-FileHash $path).Hash
 $dom=New-Object -ComObject Msxml2.DOMDocument.6.0;$dom.async=$false;$cache=New-Object -ComObject Msxml2.XMLSchemaCache.6.0;$cache.add('http://www.gribuser.ru/xml/fictionbook/2.0',(Join-Path $root 'runtime/FictionBook.xsd'));if(-not $dom.load($path)){throw 'Fixture parse failed'};$dom.schemas=$cache;if($dom.validate().errorCode){throw 'Fixture XSD failed'}
}
$variants=[ordered]@{baseline=(Join-Path $root 'out/perf/baseline/FBE.exe');c2=(Join-Path $root 'out/perf/c2/FBE.exe');c3=(Join-Path $root 'out/perf/c3-2f0113b2/FBE.exe')}
$manifest=@{status='RUNNING';kind='diagnostic Open-only scaling';payloadContent='unique per-entry index in PNG trailing padding';fixedTotalDecodedBytes=32768000;nearZeroFixedCostControlCount=2;measuredRepeats=3;warmupsPerCaseVariant=1;cases=$cases;variants=$variants;driverSha256=(Get-FileHash (Join-Path $out 'open-driver.exe')).Hash;runtime='immutable C3 benchmark snapshots';order='alternating reverse/forward round, same order for all cases in round'}
$manifest|ConvertTo-Json -Depth 7|Set-Content (Join-Path $out 'manifest.json') -Encoding utf8
$oldTemp=$env:TEMP;$oldTmp=$env:TMP;$temp=Join-Path $root 'out/perf/c4/tmp';New-Item -ItemType Directory -Force $temp|Out-Null;$env:TEMP=$temp;$env:TMP=$temp
$isolations=@{};$passed=$false;$rows=[Collections.Generic.List[object]]::new()
try {
 foreach($v in $variants.Keys){$iso=New-IsolatedFbeRuntime -FbeExe $variants[$v] -Name "c4-$v";$isolations[$v]=$iso}
 foreach($round in 0..3){$order=if($round%2){@('c3','c2','baseline')}else{@('baseline','c2','c3')};foreach($case in $cases){foreach($v in $order){
  $iso=$isolations[$v];$null=New-IsolatedFbeSettingsSeed $iso -StartupScriptsDirectory (Join-Path $iso.Root 'perf-scripts')
  $report=Join-Path $out ($case.name+"-$round-$v.tsv");Write-Host "C4 Open $($case.name) round=$round variant=$v"
  & (Join-Path $out 'open-driver.exe') $iso.Exe $case.path ([string]$case.count) $report
  if($LASTEXITCODE){throw "Open driver failed: $($case.name)/$round/$v"}
  foreach($line in [IO.File]::ReadAllLines($report)){$f=$line.Split("`t");if($f[0] -notin @('time','memory')){continue};$rows.Add([pscustomobject]@{case=$case.name;count=$case.count;bytes=$case.bytes;round=$round;warmup=($round -eq 0);variant=$v;kind=$f[0];metric=$f[1];value=[double]::Parse($f[2],[Globalization.CultureInfo]::InvariantCulture)})}
  $rows|ConvertTo-Json -Depth 4|Set-Content (Join-Path $out 'raw.json') -Encoding utf8
 }}}
 foreach($case in $cases){if((Get-FileHash $case.path).Hash -cne $case.sha256){throw 'Fixture changed'}}
 $manifest.status='PASS';$manifest.completedWarmups=18;$manifest.completedMeasured=54;$manifest|ConvertTo-Json -Depth 7|Set-Content (Join-Path $out 'manifest.json') -Encoding utf8;$passed=$true
}finally{foreach($iso in $isolations.Values){if(-not [IO.Path]::GetFullPath($iso.Root).StartsWith([IO.Path]::GetFullPath($temp).TrimEnd('\')+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Unexpected isolation cleanup root'};Complete-IsolatedFbeRuntime -Isolation $iso -Passed $passed};$env:TEMP=$oldTemp;$env:TMP=$oldTmp}
Write-Host 'C4 scaling PASS'
