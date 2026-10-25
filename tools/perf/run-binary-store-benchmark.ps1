[CmdletBinding()]
param(
    [string]$Baseline=(Join-Path $PSScriptRoot '../../out/perf/baseline/FBE.exe'),
    [string]$C2=(Join-Path $PSScriptRoot '../../out/perf/c2/FBE.exe'),
    [string]$C3=(Join-Path $PSScriptRoot '../../out/perf/c3/FBE.exe'),
    [Parameter(Mandatory)][string]$C3Revision,
    [ValidateRange(7,100)][int]$MeasuredRuns=7,
    [ValidateRange(1,2)][int]$WarmupRuns=1,
    [switch]$Smoke,
    [ValidateSet('control','many','large')][string[]]$Fixtures=@('control','many','large'),
    [string]$OutputDirectory=(Join-Path $PSScriptRoot '../../out/perf/results')
)
$ErrorActionPreference='Stop'
if(-not $Smoke -and $C3Revision -notmatch '^[0-9a-fA-F]{40}$'){throw 'Measured C3 requires an exact committed 40-character Git SHA; use -Smoke for development.'}
$root=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$output=[IO.Path]::GetFullPath($OutputDirectory)
if(Test-Path (Join-Path $output 'manifest.json')){throw 'Choose a new output directory; completed benchmark evidence is immutable.'}
New-Item -ItemType Directory -Force $output | Out-Null
. (Join-Path $root 'tools/tests/RuntimeTestIsolation.ps1')
. (Join-Path $root 'tools/build/Import-VsDevEnvironment.ps1') -Arch x86 -HostArch x64 -PlatformToolset v143 -VcVarsVersion 14.44
Push-Location $output
try {
    & cl.exe /nologo /O2 /EHsc /std:c++17 /MT /W4 /WX /DUNICODE /D_UNICODE (Join-Path $PSScriptRoot 'binary-store-driver.cpp') /Fe:binary-store-driver.exe
    if($LASTEXITCODE){throw 'Benchmark controller build failed'}
} finally {Pop-Location}
$driver=Join-Path $output 'binary-store-driver.exe'
function Hash([string]$Path){(Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash}
function Runtime-Hashes([string]$Exe) {
    $dir=Split-Path $Exe
    $items=Get-ChildItem $dir -Recurse -File | Where-Object {$_.Extension -in @('.exe','.dll','.js','.xsd','.html','.xsl','.xslt')}
    $rows=[ordered]@{};foreach($item in $items){$rows[$item.FullName.Substring($dir.Length+1)]=Hash $item.FullName};return $rows
}
function Assert-Equivalent([string]$Path,[hashtable]$Expected,[string[]]$References) {
    $cache=New-Object -ComObject Msxml2.XMLSchemaCache.6.0
    $cache.add('http://www.gribuser.ru/xml/fictionbook/2.0',(Join-Path $root 'runtime/FictionBook.xsd'))
    $dom=New-Object -ComObject Msxml2.DOMDocument.6.0;$dom.async=$false
    if(-not $dom.load($Path)){throw "Invalid XML: $Path"};$dom.schemas=$cache
    if($dom.validate().errorCode){throw "XSD failed: $Path : $($dom.validate().reason)"}
    [xml]$xml=[IO.File]::ReadAllText($Path)
    $ns=[Xml.XmlNamespaceManager]::new($xml.NameTable);$ns.AddNamespace('f','http://www.gribuser.ru/xml/fictionbook/2.0');$ns.AddNamespace('l','http://www.w3.org/1999/xlink')
    $nodes=$xml.SelectNodes('/f:FictionBook/f:binary',$ns)
    if($nodes.Count -ne $Expected.Count){throw "Binary count differs: $Path"}
    foreach($node in $nodes){
        $bytes=[Convert]::FromBase64String($node.InnerText)
        $hash=[Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($bytes))
        if(-not $Expected.ContainsKey($node.id) -or $hash -cne $Expected[$node.id].hash -or $node.GetAttribute('content-type') -cne $Expected[$node.id].mime){throw "Binary identity differs: $Path / $($node.id)"}
    }
    $refs=@($xml.SelectNodes('//@l:href',$ns) | ForEach-Object {$_.Value})
    if(($refs -join "`n") -cne ($References -join "`n")){throw "References differ: $Path"}
}
Add-Type -AssemblyName System.Drawing
function Image-Bytes([int]$Seed,[bool]$Large=$false) {
    $width=if($Large){1024}else{3};$height=if($Large){1024}else{2}
    $bitmap=[Drawing.Bitmap]::new($width,$height,[Drawing.Imaging.PixelFormat]::Format24bppRgb)
    $stream=[IO.MemoryStream]::new()
    try {
        $rect=[Drawing.Rectangle]::new(0,0,$width,$height)
        $locked=$bitmap.LockBits($rect,[Drawing.Imaging.ImageLockMode]::WriteOnly,[Drawing.Imaging.PixelFormat]::Format24bppRgb)
        try {$pixels=[byte[]]::new($locked.Stride*$height);[Random]::new($Seed).NextBytes($pixels);[Runtime.InteropServices.Marshal]::Copy($pixels,0,$locked.Scan0,$pixels.Length)}finally{$bitmap.UnlockBits($locked)}
        $bitmap.Save($stream,[Drawing.Imaging.ImageFormat]::Png);return ,$stream.ToArray()
    } finally {$stream.Dispose();$bitmap.Dispose()}
}
$replacement=Join-Path $output 'replacement.png';[IO.File]::WriteAllBytes($replacement,(Image-Bytes 999))
$replacementHash=Hash $replacement
$fixtureRecords=@{}
foreach($name in $Fixtures) {
    $count=switch($name){control{4}many{500}large{10}}
    $expected=@{};$path=Join-Path $output "$name.fb2"
    $writer=[IO.StreamWriter]::new($path,$false,[Text.UTF8Encoding]::new($false))
    try {
        $writer.Write('<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0" xmlns:l="http://www.w3.org/1999/xlink"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>Binary benchmark</book-title><coverpage><image l:href="#image1.png"/></coverpage><lang>en</lang></title-info><document-info><author><first-name>T</first-name><last-name>T</last-name></author><date value="2026-10-25">2026-10-25</date><id>binary-perf</id><version>1</version></document-info></description><body><section>')
        for($p=0;$p -lt 100;$p++){$writer.Write('<p>Reproducible binary benchmark ordinary text paragraph.</p>')}
        $writer.Write('<p><image l:href="#image1.png"/><image l:href="#image1.png"/><image l:href="#photo.png"/></p></section></body>')
        $payloadBytes=0L
        for($i=0;$i -lt $count;$i++) {
            $bytes=Image-Bytes (100+$i) ($name -eq 'large')
            if($name -ne 'large'){$padded=[byte[]]::new($(if($name -eq 'many'){65536}else{4096}));$bytes.CopyTo($padded,0);$bytes=$padded}
            $mime=if($name -eq 'many' -and $i -ge 2){'application/octet-stream'}else{'image/png'}
            $extension=if($mime -eq 'image/png'){'png'}else{'bin'}
            $id=if($i -eq 0){'image1.png'}elseif($i -eq 1){'photo.png'}else{"payload-$i.$extension"}
            $expected[$id]=@{hash=[Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($bytes));mime=$mime}
            $payloadBytes+=$bytes.Length
            $writer.Write('<binary id="'+$id+'" content-type="'+$mime+'">'+[Convert]::ToBase64String($bytes)+'</binary>')
        }
        $writer.Write('</FictionBook>')
    }finally{$writer.Dispose()}
    $refs=@('#image1.png','#image1.png','#image1.png','#photo.png')
    Assert-Equivalent $path $expected $refs
    $fixtureRecords[$name]=@{path=$path;sha256=(Hash $path);payloadBytes=$payloadBytes;binaryCount=$count;expected=$expected;references=$refs}
}
$variants=[ordered]@{baseline=@{exe=(Resolve-Path $Baseline).Path;revision='a4145d832b9be02930400969328b2f5a0ec54182'};c2=@{exe=(Resolve-Path $C2).Path;revision='3e44dc5f621819c5a4d50009ad367e24c8ac4fea'};c3=@{exe=(Resolve-Path $C3).Path;revision=$C3Revision}}
$runs=[Collections.Generic.List[object]]::new();$isolations=@{}
$environmentNames=@('FBE_NEXT_TEST_MODE','FBE_NEXT_TEST_SCENARIO','FBE_NEXT_SCRIPT_API_ROOT','FBE_NEXT_TRACE','FBE_NEXT_TEST_SETTINGS_DIRECTORY')
$old=@{};foreach($key in $environmentNames){$old[$key]=[Environment]::GetEnvironmentVariable($key,'Process');[Environment]::SetEnvironmentVariable($key,$null,'Process')}
$passed=$false
try {
    foreach($key in $variants.Keys){
        $variant=$variants[$key];$variant.hashes=Runtime-Hashes $variant.exe
        $iso=New-IsolatedFbeRuntime -FbeExe $variant.exe -Name "perf-$key";$null=New-IsolatedFbeSettingsSeed $iso -StartupScriptsDirectory (Join-Path $iso.Root 'perf-scripts');$isolations[$key]=$iso
    }
    $dependencies=@($variants.baseline.hashes.Keys | Where-Object {$_ -like '*.dll'})
    foreach($key in @('c2','c3')) {
        $current=@($variants[$key].hashes.Keys | Where-Object {$_ -like '*.dll'})
        if((($current | Sort-Object) -join "`n") -cne (($dependencies | Sort-Object) -join "`n")){throw "Dependency inventory differs: $key"}
        foreach($dep in $dependencies){if($variants[$key].hashes[$dep] -cne $variants.baseline.hashes[$dep]){throw "Dependency bytes differ: $key / $dep"}}
    }
    $manifest=@{status='RUNNING';configuration='Release Win32';compiler=$env:VCToolsVersion;machine=$env:COMPUTERNAME;os=[Environment]::OSVersion.VersionString;powerProfile=(& powercfg.exe /getactivescheme | Out-String).Trim();warmups=$WarmupRuns;measured=$MeasuredRuns;variants=$variants;fixtures=$fixtureRecords;driverSha256=(Hash $driver);roundtrip='Save -> document close -> process close -> process start -> document ready';privatePeak='sampled every 10 ms';steadyMemory='mean of 10 samples 50 ms apart';order='alternating forward/reverse per fixture and repetition'}
    if($Smoke){$manifest.measured=0;$manifest.warmups=1}
    $manifest | ConvertTo-Json -Depth 10 | Set-Content (Join-Path $output 'manifest.json') -Encoding utf8
    $roundCount=if($Smoke){1}else{$WarmupRuns+$MeasuredRuns}
    for($round=0;$round -lt $roundCount;$round++) {
        foreach($name in $Fixtures) {
            $order=if($round%2 -eq 0){@('baseline','c2','c3')}else{@('c3','c2','baseline')}
            foreach($key in $order) {
                $fixture=$fixtureRecords[$name];$iso=$isolations[$key]
                $prefix=Join-Path $output "$name-$round-$key";$book="$prefix-input.fb2";Copy-Item $fixture.path $book
                $null=New-IsolatedFbeSettingsSeed $iso -StartupScriptsDirectory (Join-Path $iso.Root 'perf-scripts')
                Write-Host "Benchmark $name round=$round variant=$key"
                & $driver $iso.Exe $book $replacement ([string]$fixture.binaryCount) "$prefix.tsv" $prefix
                if($LASTEXITCODE){throw "Driver failed: $name / $round / $key"}
                Assert-Equivalent "$prefix-unchanged.fb2" $fixture.expected $fixture.references
                $changed=$fixture.expected.Clone();$changed['image1.png']=@{hash=$replacementHash;mime='image/png'}
                Assert-Equivalent "$prefix-replaced.fb2" $changed $fixture.references
                Assert-Equivalent "$prefix-source.fb2" $changed $fixture.references
                $mutated=$fixture.expected.Clone();$mutated['image1.png']=$fixture.expected['photo.png']
                Assert-Equivalent "$prefix-mutation.fb2" $mutated $fixture.references
                Assert-Equivalent $book $changed $fixture.references
                [pscustomobject]@{fixture=$name;round=$round;variant=$key;xsd='PASS';binarySha256='PASS';mime='PASS';references='PASS';unchangedFileSha256=(Hash "$prefix-unchanged.fb2");replacedFileSha256=(Hash "$prefix-replaced.fb2");sourceAppliedSha256=(Hash "$prefix-source.fb2");mutationSha256=(Hash "$prefix-mutation.fb2");reopenedFileSha256=(Hash $book)} | ConvertTo-Json | Set-Content "$prefix-equivalence.json" -Encoding utf8
                foreach($line in @([IO.File]::ReadAllLines("$prefix.tsv"))+@([IO.File]::ReadAllLines("$prefix.tsv.reopen"))) {
                    $fields=$line.Split("`t");if($fields[0] -notin @('time','memory')){continue}
                    $runs.Add([pscustomobject]@{fixture=$name;round=$round;warmup=($round -lt $WarmupRuns);variant=$key;kind=$fields[0];metric=$fields[1];value=[double]::Parse($fields[2],[Globalization.CultureInfo]::InvariantCulture)})
                }
                # Large serialized outputs are verified then removed; hashes and
                # raw measurements remain. Inputs are regenerated byte-identically.
                foreach($file in @($book,"$prefix-unchanged.fb2","$prefix-replaced.fb2","$prefix-source.fb2","$prefix-mutation.fb2")){Remove-Item -LiteralPath $file}
                $runs | ConvertTo-Json -Depth 4 | Set-Content (Join-Path $output 'raw.json') -Encoding utf8
            }
        }
    }
    foreach($key in $variants.Keys){$after=Runtime-Hashes $variants[$key].exe;if(($after | ConvertTo-Json -Compress) -cne ($variants[$key].hashes | ConvertTo-Json -Compress)){throw "Immutable runtime changed: $key"}}
    foreach($fixture in $fixtureRecords.Values){if((Hash $fixture.path) -cne $fixture.sha256){throw "Immutable fixture changed: $($fixture.path)"}}
    function Stats($Values){$v=@($Values | Sort-Object);$n=$v.Count;$median=if($n%2){$v[[int][Math]::Floor($n/2)]}else{($v[$n/2-1]+$v[$n/2])/2};@{median=$median;p90=$v[[int][Math]::Ceiling(.9*$n)-1];min=$v[0];max=$v[-1];n=$n}}
    function Delta([double]$Old,[double]$New){if($Old){100*($Old-$New)/$Old}else{$null}}
    $summary=@();$measured=@($runs | Where-Object {-not $_.warmup})
    foreach($group in ($measured | Group-Object fixture,kind,metric)) {
        $row=[ordered]@{fixture=$group.Group[0].fixture;kind=$group.Group[0].kind;metric=$group.Group[0].metric}
        foreach($key in $variants.Keys){
            $row[$key]=Stats @($group.Group | Where-Object variant -eq $key | ForEach-Object value)
            if($row[$key].n -ne $MeasuredRuns){throw "Incomplete measurement group: $($row.fixture) / $($row.metric) / $key"}
        }
        $row.c2VsBaseline=Delta $row.baseline.median $row.c2.median;$row.c3VsC2=Delta $row.c2.median $row.c3.median;$row.c3VsBaseline=Delta $row.baseline.median $row.c3.median
        $row.warning=($row.fixture -eq 'control' -and $row.kind -eq 'time' -and $row.c3VsC2 -lt -5)
        $summary += [pscustomobject]$row
    }
    $summary | ConvertTo-Json -Depth 7 | Set-Content (Join-Path $output 'summary.json') -Encoding utf8
    $lines=@('| Kind | Fixture | Metric | Baseline median | C2 median | C3 median | C2 vs baseline % | C3 vs C2 % | C3 vs baseline % |','|---|---|---|---:|---:|---:|---:|---:|---:|')
    foreach($row in $summary){$lines+='| {0} | {1} | {2} | {3:F2} | {4:F2} | {5:F2} | {6:F2} | {7:F2} | {8:F2} |' -f $row.kind,$row.fixture,$row.metric,$row.baseline.median,$row.c2.median,$row.c3.median,$row.c2VsBaseline,$row.c3VsC2,$row.c3VsBaseline}
    $lines | Set-Content (Join-Path $output 'summary.md') -Encoding utf8
    $manifest.status=if($Smoke){'SMOKE_PASS'}else{'PASS'};$manifest.startupScriptCatalog='empty isolated fixture directory';$manifest.equivalence='all saved outputs: binary SHA-256, MIME, references and XSD PASS';$manifest | ConvertTo-Json -Depth 10 | Set-Content (Join-Path $output 'manifest.json') -Encoding utf8
    $passed=$true
} finally {
    foreach($key in $environmentNames){[Environment]::SetEnvironmentVariable($key,$old[$key],'Process')}
    foreach($iso in $isolations.Values){Complete-IsolatedFbeRuntime -Isolation $iso -Passed $passed}
}
Write-Host "Three-way benchmark $($manifest.status): $output"
