[CmdletBinding()]
param(
    [string]$FbeExe=(Join-Path $PSScriptRoot '../../out/Release/FBE.exe'),
    [ValidateSet('portable','installed')][string]$Mode='portable',
    [ValidateSet('fb2','fbd')][string]$Kind='fb2',
    [int]$TimeoutSeconds=90
)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$iso=New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'scripting-stage3b'
$passed=$false
$names=@('FBE_NEXT_TEST_MODE','FBE_NEXT_TEST_SCENARIO','FBE_NEXT_SCRIPT_API_ROOT','FBE_NEXT_TEST_SETTINGS_DIRECTORY','FBE_NEXT_TRACE')
$old=@{}; foreach($name in $names){$old[$name]=[Environment]::GetEnvironmentVariable($name,'Process')}
try {
    $null=New-IsolatedFbeSettingsSeed $iso
    $scripts=Join-Path $iso.Root 'api3b'; New-Item -ItemType Directory $scripts | Out-Null
    $export=Join-Path $scripts 'export'; New-Item -ItemType Directory $export | Out-Null
    Add-Type -AssemblyName System.Drawing
    $payloads=New-Object 'Collections.Generic.Dictionary[string,byte[]]' ([StringComparer]::Ordinal)
    $bitmap=New-Object Drawing.Bitmap 3,2
    try {
        foreach($format in @('png','jpeg')) {
            $stream=New-Object IO.MemoryStream
            try {$bitmap.Save($stream, [Drawing.Imaging.ImageFormat]::$format);$payloads[$format]=$stream.ToArray()}finally{$stream.Dispose()}
        }
    }finally{$bitmap.Dispose()}
    $payloads['CON']=[byte[]]@(0,255,34,92,13,10)
    $payloads['Case']=[byte[]]@(4,5)
    $payloads['case']=[byte[]]@(6,7)
    $payloads['кириллица']=[byte[]]@(8,9)
    $payloads[('long'+('x'*200))]=[byte[]]::new(1024*1024)
    $expectedCount=$payloads.Count
    $js=@'
var binaryResult="not run", e=window.external;
function jB(text){return eval("("+text+")");} // Trusted native API results only.
function aB(ok,label){if(!ok)throw new Error(label);}
function rejectB(callback){var rejected=false;try{callback();}catch(error){rejected=true;}aB(rejected,"missing rejection");}
function Run(){binaryResult="PASS";}
function resultBinary(){return binaryResult;}
function readBinary(){try{
 var selected=e.GetSelectionText(), html=e.GetSelectionHtml(), bookmark=e.CreateSelectionBookmark();
 aB(e.HasScriptingMethod("GetBinaryInfo")&&e.HasScriptingMethod("GenerateUniqueBinaryId")&&e.HasScriptingMethod("SaveAllBinaries"),"methods");
 aB(!e.HasScriptingMethod("AddBinaryFromFile")&&!e.HasScriptingMethod("ReplaceBinaryFromFile")&&!e.HasScriptingMethod("RemoveBinary"),"unpublished mutation methods");
 var p=jB(e.GetBinaryInfo("png"));aB(p.id==="png"&&p.mimeType==="image/png"&&p.width===3&&p.height===2&&p.size>20,"PNG info");
 p=jB(e.GetBinaryInfo("jpeg"));aB(p.mimeType==="image/jpeg"&&p.width===3&&p.height===2&&p.size>20,"JPEG info");
 p=jB(e.GetBinaryInfo("CON"));aB(p.size===6&&p.width===null&&p.height===null&&p.referenceCount===null,"nonimage info");
 rejectB(function(){e.GetBinaryInfo("missing");});aB(jB(e.GetLastScriptingError()).method==="GetBinaryInfo","error context");
 rejectB(function(){e.GetBinaryInfo(1);});rejectB(function(){e.GetBinaryInfo();});
 aB(e.GenerateUniqueBinaryId("кириллица")==="кириллица_0","Unicode conflict");
 var unicode="😀.png";aB(e.GenerateUniqueBinaryId(unicode)===unicode&&IsBinaryXmlId(unicode),"supplementary ID");
 var names=["", "../a b", "1.png", "\ud800x", "\udc00"];
 for(var i=0;i<names.length;i++)aB(IsBinaryXmlId(e.GenerateUniqueBinaryId(names[i])),"normalized XML ID");
 var dir=@@EXPORT@@;
 var first=jB(e.SaveAllBinaries(dir));aB(first.errors===0&&first.saved+first.skipped===@@COUNT@@,"default export");
 var skip=jB(e.SaveAllBinaries(dir,"skip"));aB(skip.saved===0&&skip.skipped===@@COUNT@@&&skip.errors===0,"skip");
 var fail=jB(e.SaveAllBinaries(dir,"fail"));aB(fail.status==="error"&&fail.errors===@@COUNT@@&&fail.saved===0,"conflicts");
 var replace=jB(e.SaveAllBinaries(dir,"replace"));aB(replace.status==="ok"&&replace.saved===@@COUNT@@&&replace.errors===0,"replace");
 e.WriteTextFileEx(@@MANIFEST@@,e.SaveAllBinaries(dir,"skip"),true);
 rejectB(function(){e.SaveAllBinaries(dir,"bad");});rejectB(function(){e.SaveAllBinaries(dir+"\\absent");});
 aB(selected===e.GetSelectionText()&&html===e.GetSelectionHtml()&&e.RestoreSelectionBookmark(bookmark),"selection preserved");e.ReleaseSelectionBookmark(bookmark);
 return "PASS";
}catch(error){return String(error.message||error);}}
function sourceBinary(){try{rejectB(function(){e.GetBinaryInfo("png");});rejectB(function(){e.GenerateUniqueBinaryId("x");});rejectB(function(){e.SaveAllBinaries(@@EXPORT@@);});return "PASS";}catch(error){return String(error.message||error);}}
function lockedBinary(){try{var r=jB(e.SaveAllBinaries(@@EXPORT@@,"replace"));aB(r.status==="error"&&r.errors===1&&r.saved===@@COUNT@@-1,"partial locked export");return "PASS";}catch(error){return String(error.message||error);}}
'@
    $manifest=Join-Path $scripts 'export.json'
    $js=$js.Replace('@@EXPORT@@',($export|ConvertTo-Json -Compress)).Replace('@@COUNT@@',[string]$expectedCount).Replace('@@MANIFEST@@',($manifest|ConvertTo-Json -Compress))
    $js | Set-Content (Join-Path $scripts 'stage3b.js') -Encoding utf8
    $fixture=Join-Path $iso.Root "api3.$Kind"; $report=Join-Path $iso.Root 'api3.txt'
    $body=if($Kind -eq 'fb2'){'<body><section><p>ordinary paragraph</p></section></body>'}else{''}
    $binaries=''
    foreach($id in $payloads.Keys){$mime=if($id -eq 'png'){'image/png'}elseif($id -eq 'jpeg'){'image/jpeg'}else{'application/octet-stream'};$binaries+='<binary id="'+$id+'" content-type="'+$mime+'">'+[Convert]::ToBase64String($payloads[$id])+'</binary>'}
    ('<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>API3B</book-title><lang>en</lang></title-info><document-info><id>api3b</id><version>1</version></document-info></description>'+$body+$binaries+'</FictionBook>') | Set-Content $fixture -Encoding utf8
    $env:FBE_NEXT_TEST_MODE='1'; $env:FBE_NEXT_TEST_SCENARIO='scripting-stage3b'; $env:FBE_NEXT_SCRIPT_API_ROOT=$scripts
    $env:FBE_NEXT_TEST_SETTINGS_DIRECTORY=Join-Path $iso.DataPath 'Settings'; $env:FBE_NEXT_TRACE='0'
    $process=Start-Process $iso.Exe -WorkingDirectory $iso.Root -ArgumentList @("--$Mode",'-b',$report,$fixture) -WindowStyle Hidden -PassThru
    $code=Wait-IsolatedFbeProcess -Process $process -Isolation $iso -Scenario 'scripting-stage3b' -Report $report -TimeoutSeconds $TimeoutSeconds
    $text=if(Test-Path $report){Get-Content $report -Raw}else{'missing report'}
    if($code -ne 0 -or $text -notmatch 'result=PASS'){throw "Stage 3B failed: $code; $text"}
    $results=Get-Content $manifest -Raw | ConvertFrom-Json
    if($results.objects.Count -ne $expectedCount){throw 'Export manifest count'}
    $seen=@{}
    foreach($object in $results.objects){
        if($seen.ContainsKey($object.fileName.ToUpperInvariant())){throw 'Case-insensitive export collision'}
        $seen[$object.fileName.ToUpperInvariant()]=$true
        $actual=[IO.File]::ReadAllBytes((Join-Path $export $object.fileName))
        if([Convert]::ToBase64String($actual) -cne [Convert]::ToBase64String($payloads[$object.id])){throw "Binary bytes changed: $($object.id)"}
    }
    if(@(Get-ChildItem $export -Filter '*.tmp').Count){throw 'Leaked export temporary files'}
    $passed=$true; Write-Host "Scripting stage 3B reading/export ($Mode/$Kind): PASS."
} finally {
    foreach($name in $names){[Environment]::SetEnvironmentVariable($name,$old[$name],'Process')}
    Complete-IsolatedFbeRuntime -Isolation $iso -Passed $passed
}
