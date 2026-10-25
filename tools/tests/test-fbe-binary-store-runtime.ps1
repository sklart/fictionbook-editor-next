[CmdletBinding()]
param([string]$FbeExe=(Join-Path $PSScriptRoot '../../out/Release/FBE.exe'),[int]$TimeoutSeconds=180)
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$iso=New-IsolatedFbeRuntime -FbeExe $FbeExe -Name binary-store
$passed=$false
$names=@('FBE_NEXT_TEST_MODE','FBE_NEXT_TEST_SCENARIO','FBE_NEXT_SCRIPT_API_ROOT','FBE_NEXT_TEST_SETTINGS_DIRECTORY','FBE_NEXT_TRACE')
$old=@{};foreach($name in $names){$old[$name]=[Environment]::GetEnvironmentVariable($name,'Process')}
$evidence=Join-Path $root 'out/tests/binary-store'
New-Item -ItemType Directory -Force $evidence | Out-Null
try {
    $null=New-IsolatedFbeSettingsSeed $iso
    Add-Type -AssemblyName System.Drawing
    $binaries=[Text.StringBuilder]::new()
    $expected=@{}
    $bitmap=[Drawing.Bitmap]::new(3,2)
    try {
        foreach($kind in @('png','jpeg')) {
            $stream=[IO.MemoryStream]::new()
            try {
                $bitmap.Save($stream,[Drawing.Imaging.ImageFormat]::$kind)
                $bytes=[byte[]]::new(65536);$stream.ToArray().CopyTo($bytes,0)
                $expected[$kind]=$bytes
            } finally {$stream.Dispose()}
        }
    } finally {$bitmap.Dispose()}
    $bitmap=[Drawing.Bitmap]::new(5,4)
    try {$bitmap.Save((Join-Path $iso.Root 'replace.png'),[Drawing.Imaging.ImageFormat]::Png)} finally {$bitmap.Dispose()}
    for($index=0;$index -lt 100;$index++) {
        $id=if($index -eq 0){'png'}elseif($index -eq 1){'jpeg'}else{"payload$index"}
        $mime=if($index -lt 2){"image/$id"}else{'application/octet-stream'}
        if($index -ge 2){$bytes=[byte[]]::new(65536);for($offset=0;$offset -lt $bytes.Length;$offset++){$bytes[$offset]=[byte](($index+$offset)%256)};$expected[$id]=$bytes}
        [void]$binaries.Append('<binary id="'+$id+'" content-type="'+$mime+'">'+[Convert]::ToBase64String($expected[$id])+'</binary>')
    }
    $fixture=Join-Path $iso.Root 'fixture.fb2'
    $xml='<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0" xmlns:l="http://www.w3.org/1999/xlink"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>C2</book-title><lang>en</lang></title-info><document-info><id>c2</id><version>1</version></document-info></description><body><section><p>C2 binary store</p></section></body>'+$binaries.ToString()+'</FictionBook>'
    [IO.File]::WriteAllText($fixture,$xml,[Text.UTF8Encoding]::new($false))
    $legacy=[byte[]](0..16)
    $js=@'
function Run(){
 var binary=document.all.binobj.getElementsByTagName("DIV")[2];
 var dom=new ActiveXObject("Msxml2.DOMDocument.6.0");dom.loadXML("<data/>");
 var data=dom.documentElement;data.dataType="bin.base64";data.text="@@DATA@@";
 binary.base64data=data.nodeTypedValue;
 binary.all.id.value="legacy-renamed";
 binary.all.type.value="application/x-legacy";
 var info=eval("("+window.external.GetBinaryInfo("legacy-renamed")+")");
 if(info.size!==17||info.mimeType!=="application/x-legacy")throw new Error("legacy metadata stale inside script");
}
'@
    [IO.File]::WriteAllText((Join-Path $iso.Root 'legacy.js'),$js.Replace('@@DATA@@',[Convert]::ToBase64String($legacy)),[Text.UTF8Encoding]::new($false))
    foreach($entry in @{ 'cache-mutate.js'=[IO.File]::ReadAllBytes((Join-Path $iso.Root 'replace.png')); 'cache-restore.js'=$expected['png'] }.GetEnumerator()) {
        $cacheJs='function Run(){var n=new ActiveXObject("Msxml2.DOMDocument.6.0");n.loadXML("<data/>");n.documentElement.dataType="bin.base64";n.documentElement.text="'+[Convert]::ToBase64String($entry.Value)+'";document.all.binobj.getElementsByTagName("DIV")[0].base64data=n.documentElement.nodeTypedValue;}'
        [IO.File]::WriteAllText((Join-Path $iso.Root $entry.Key),$cacheJs,[Text.UTF8Encoding]::new($false))
    }
    [IO.File]::WriteAllText((Join-Path $iso.Root 'legacy-error.js'),'function Run(){var b=document.all.binobj.getElementsByTagName("DIV")[2];b.all.type.value="application/x-legacy-error";throw new Error("intentional C2 regression exception");}',[Text.UTF8Encoding]::new($false))
    $report=Join-Path $iso.Root 'report.txt'
    $env:FBE_NEXT_TEST_MODE='1';$env:FBE_NEXT_TEST_SCENARIO='binary-store';$env:FBE_NEXT_SCRIPT_API_ROOT=$iso.Root
    $env:FBE_NEXT_TEST_SETTINGS_DIRECTORY=Join-Path $iso.DataPath 'Settings';$env:FBE_NEXT_TRACE='1'
    $process=Start-Process $iso.Exe -WorkingDirectory $iso.Root -ArgumentList @('--portable','-b',$report,$fixture) -WindowStyle Hidden -PassThru
    $code=Wait-IsolatedFbeProcess -Process $process -Isolation $iso -Scenario binary-store -Report $report -TimeoutSeconds $TimeoutSeconds
    if(Test-Path $report){Copy-Item $report (Join-Path $evidence 'runtime-report.txt') -Force}
    $text=if(Test-Path $report){Get-Content $report -Raw}else{'missing report'}
    if($code -ne 0 -or $text -notmatch 'result=PASS'){throw "BinaryStore runtime failed: $code; $text"}
    $expected['renamed']=$expected['payload3'];$expected.Remove('payload3');$expected.Remove('payload2');$expected.Remove('payload99')
    $expected['legacy-renamed']=$legacy
    $expected['png']=[IO.File]::ReadAllBytes((Join-Path $iso.Root 'replace.png'));$expected['added']=$expected['png']
    [xml]$saved=Get-Content (Join-Path $iso.Root 'saved.fb2') -Raw
    $ns=[Xml.XmlNamespaceManager]::new($saved.NameTable);$ns.AddNamespace('fb','http://www.gribuser.ru/xml/fictionbook/2.0')
    $nodes=$saved.SelectNodes('/fb:FictionBook/fb:binary',$ns)
    if($nodes.Count -ne $expected.Count){throw 'Saved binary count mismatch'}
    foreach($node in $nodes){
        if(-not $expected.ContainsKey($node.id)){throw "Unexpected saved ID: $($node.id)"}
        $actual=[Convert]::FromBase64String($node.InnerText)
        if([Convert]::ToBase64String($actual) -cne [Convert]::ToBase64String($expected[$node.id])){throw "Saved bytes mismatch: $($node.id)"}
    }
    if($saved.SelectSingleNode('/fb:FictionBook/fb:binary[@id="legacy-renamed"]',$ns).GetAttribute('content-type') -cne 'application/x-legacy-error'){throw 'Legacy MIME not preserved'}
    $trace=Get-ChildItem (Join-Path $iso.DataPath 'Diagnostics') -Filter 'fbe-trace-*.log' | Sort-Object LastWriteTime -Descending | Select-Object -First 1
    if(-not $trace){throw 'Missing diagnostic trace'}
    Copy-Item $trace.FullName (Join-Path $evidence 'runtime-trace.log') -Force
    $traceText=Get-Content $trace.FullName -Raw
    foreach($counter in @('binary_store_entries','binary_store_rebuilds','binary_store_reconciliations','binary_lookup_count','binary_lookup_misses','binary_size_calculations','binary_size_cache_hits','binary_dimensions_calculations','binary_dimensions_cache_hits','binary_legacy_invalidations')) {
        if($traceText -notmatch ($counter+'=\d+')){throw "Missing aggregate counter: $counter"}
    }
    $peakPrivate=[uint64]([regex]::Match($text,'peak_private_bytes=(\d+)').Groups[1].Value)
    $peakWorkingSet=[uint64]([regex]::Match($text,'peak_working_set_bytes=(\d+)').Groups[1].Value)
    if($peakPrivate -eq 0 -or $peakWorkingSet -eq 0){throw 'Missing native peak memory metrics'}
    [pscustomobject]@{status='PASS';fixture='100 x 64 KiB';binarySha256=(Get-FileHash $FbeExe -Algorithm SHA256).Hash;mainJsSha256=(Get-FileHash (Join-Path (Split-Path $FbeExe) 'main.js') -Algorithm SHA256).Hash;peakPrivateBytes=$peakPrivate;peakWorkingSetBytes=$peakWorkingSet;metrics=$text;saveReopenBytes='PASS'} | ConvertTo-Json -Depth 4 | Set-Content (Join-Path $evidence 'runtime-summary.json') -Encoding utf8
    $passed=$true;Write-Host 'BinaryStore load/cache/rename/mutations/Undo/Redo/legacy/save-reopen: PASS.'
} finally {
    foreach($name in $names){[Environment]::SetEnvironmentVariable($name,$old[$name],'Process')}
    Complete-IsolatedFbeRuntime -Isolation $iso -Passed $passed
}
