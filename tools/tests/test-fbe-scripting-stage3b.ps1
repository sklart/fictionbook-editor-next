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
    [IO.File]::WriteAllBytes((Join-Path $scripts 'input.png'),$payloads['png'])
    [IO.File]::WriteAllBytes((Join-Path $scripts 'input.jpeg'),$payloads['jpeg'])
    [IO.File]::WriteAllBytes((Join-Path $scripts 'input.pdf'),[Text.Encoding]::ASCII.GetBytes("%PDF-1.4`n%%EOF`n"))
    [IO.File]::WriteAllBytes((Join-Path $scripts 'input.bin'),$payloads['CON'])
    [IO.File]::WriteAllBytes((Join-Path $scripts 'large.bin'),$payloads[('long'+('x'*200))])
    $bitmap=New-Object Drawing.Bitmap 5,4
    try {$bitmap.Save((Join-Path $scripts 'replace.png'),[Drawing.Imaging.ImageFormat]::Png)}finally{$bitmap.Dispose()}
    [IO.File]::WriteAllBytes((Join-Path $scripts 'broken.png'),[byte[]]@(137,80,78,71,13,10,26,10))
    [IO.File]::WriteAllBytes((Join-Path $scripts 'empty.bin'),[byte[]]@())
    $oversize=[IO.File]::Create((Join-Path $scripts 'oversize.bin'));try{$oversize.SetLength(65MB)}finally{$oversize.Dispose()}
    $expectedCount=$payloads.Count
    $js=@'
var binaryResult="not run", e=window.external;
function jB(text){return eval("("+text+")");} // Trusted native API results only.
function aB(ok,label){if(!ok)throw new Error(label);}
function rejectB(callback){var rejected=false;try{callback();}catch(error){rejected=true;}aB(rejected,"missing rejection");}
function Run(){binaryResult="PASS";}
function resultBinary(){return binaryResult;}
function readBinary(){try{
 aB(e.GetScriptingApiVersion()==="1.3.0"&&jB(e.GetScriptingCapabilities()).capabilities.binaryObjects,"binary API version/capability");
 var selected=e.GetSelectionText(), html=e.GetSelectionHtml(), bookmark=e.CreateSelectionBookmark();
 aB(e.HasScriptingMethod("GetBinaryInfo")&&e.HasScriptingMethod("GenerateUniqueBinaryId")&&e.HasScriptingMethod("SaveAllBinaries"),"methods");
 aB(e.HasScriptingMethod("AddBinaryFromFile")&&e.HasScriptingMethod("ReplaceBinaryFromFile")&&e.HasScriptingMethod("RemoveBinary"),"mutation methods");
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
function sourceBinary(){try{rejectB(function(){e.GetBinaryInfo("png");});rejectB(function(){e.GenerateUniqueBinaryId("x");});rejectB(function(){e.SaveAllBinaries(@@EXPORT@@);});rejectB(function(){e.AddBinaryFromFile(@@INPUT@@,"added");});rejectB(function(){e.ReplaceBinaryFromFile("png",@@REPLACE@@);});rejectB(function(){e.RemoveBinary("png",true);});return "PASS";}catch(error){return String(error.message||error);}}
function lockedBinary(){try{var r=jB(e.SaveAllBinaries(@@EXPORT@@,"replace"));aB(r.status==="error"&&r.errors===1&&r.saved===@@COUNT@@-1,"partial locked export");return "PASS";}catch(error){return String(error.message||error);}}
function directoryBinary(){try{var r=jB(e.SaveAllBinaries(@@EXPORT@@));aB(r.status==="error"&&r.errors===1&&r.skipped===@@COUNT@@-1&&r.saved===0,"directory conflict is an error");return "PASS";}catch(error){return String(error.message||error);}}
function addBinary(){try{aB(e.AddBinaryFromFile(@@INPUT@@,"png")==="png_0","add conflict ID");aB(jB(e.GetBinaryInfo("png_0")).width===3,"added info");return "PASS";}catch(error){return String(error.message||error);}}
function replaceBinary(){try{aB(e.ReplaceBinaryFromFile("png",@@REPLACE@@),"replace");aB(jB(e.GetBinaryInfo("png")).width===5,"new dimensions");for(var k=0;k<ImagesInfo.length;k++)if(ImagesInfo[k].id==="png")aB(Number(ImagesInfo[k].width)===5&&Number(ImagesInfo[k].height)===4,"ImagesInfo refreshed");var bo=document.all.binobj.getElementsByTagName("DIV");for(var i=0;i<bo.length;i++)if(bo[i].all.id.value==="png")aB(bo[i].all.dims.value==="5x4","attachment dimensions UI");return "PASS";}catch(error){return String(error.message||error);}}
function sameBinary(){try{aB(e.ReplaceBinaryFromFile("png",@@INPUT@@),"identical");return "PASS";}catch(error){return String(error.message||error);}}
function removeBinary(){try{aB(e.RemoveBinary("png",true),"remove references");rejectB(function(){e.GetBinaryInfo("png");});aB(!BinaryIsReferenced("png"),"no dangling references");return "PASS";}catch(error){return String(error.message||error);}}
function removeFreeBinary(){try{aB(e.RemoveBinary("CON"),"remove unreferenced");return "PASS";}catch(error){return String(error.message||error);}}
function largeBinary(){try{aB(e.AddBinaryFromFile(@@LARGE@@,"large","application/octet-stream")==="large","large add");aB(jB(e.GetBinaryInfo("large")).size===1048576,"large size");return "PASS";}catch(error){return String(error.message||error);}}
function unicodeBinary(){try{aB(e.AddBinaryFromFile(@@INPUT@@,"книга😀.png")==="книга😀.png","Unicode add");return "PASS";}catch(error){return String(error.message||error);}}
function namedBinary(){try{aB(e.AddBinaryFromFile(@@BINARY@@,"","application/octet-stream")==="input.bin","filename-derived ID");return "PASS";}catch(error){return String(error.message||error);}}
function jpegBinary(){try{aB(e.AddBinaryFromFile(@@JPEG@@,"new.jpeg")==="new.jpeg","JPEG add");aB(jB(e.GetBinaryInfo("new.jpeg")).mimeType==="image/jpeg","JPEG MIME");return "PASS";}catch(error){return String(error.message||error);}}
function pdfBinary(){try{aB(e.AddBinaryFromFile(@@PDF@@,"new.pdf")==="new.pdf","PDF add");var r=jB(e.GetBinaryInfo("new.pdf"));aB(r.mimeType==="application/pdf"&&r.width===null,"PDF MIME");return "PASS";}catch(error){return String(error.message||error);}}
function replaceNonimageBinary(){try{aB(e.ReplaceBinaryFromFile("CON",@@LARGE@@,"application/octet-stream"),"nonimage replace");aB(jB(e.GetBinaryInfo("CON")).size===1048576,"nonimage size");return "PASS";}catch(error){return String(error.message||error);}}
function prepareBinarySelection(){try{if(e.GetCurrentViewMode()==="description"){var input=document.getElementById("tiTitle");input.focus();var range=input.createTextRange();range.select();}else{var paragraphs=document.getElementById("fbw_body").getElementsByTagName("P");if(paragraphs.length){var range=document.body.createTextRange();range.moveToElementText(paragraphs[0]);range.collapse(true);range.moveEnd("character",4);range.select();}}window.beforeBinarySelection=e.GetSelectionText()+"|"+e.GetSelectionHtml();return "PASS";}catch(error){return String(error.message||error);}}
function afterBinarySelection(){window.savedAfterBinarySelection=e.GetSelectionText()+"|"+e.GetSelectionHtml();return "PASS";}
function undoBinarySelection(){return window.beforeBinarySelection===e.GetSelectionText()+"|"+e.GetSelectionHtml()?"PASS":"selection not restored by Undo: expected ["+window.beforeBinarySelection+"] actual ["+e.GetSelectionText()+"|"+e.GetSelectionHtml()+"]";}
function redoBinarySelection(){return window.savedAfterBinarySelection===e.GetSelectionText()+"|"+e.GetSelectionHtml()?"PASS":"selection not restored by Redo";}
function rejectedBinaries(){try{
 rejectB(function(){e.AddBinaryFromFile(@@INPUT@@+".absent","x");});
 rejectB(function(){e.AddBinaryFromFile(@@BROKEN@@,"x");});rejectB(function(){e.AddBinaryFromFile(@@EMPTY@@,"x","application/octet-stream");});
 rejectB(function(){e.AddBinaryFromFile(@@BINARY@@,"x");});rejectB(function(){e.AddBinaryFromFile(@@INPUT@@,"x","image/jpeg");});
 rejectB(function(){e.AddBinaryFromFile(@@OVERSIZE@@,"x","application/octet-stream");});
 rejectB(function(){e.AddBinaryFromFile(@@BINARY@@,"x","application/pdf");});
 rejectB(function(){e.ReplaceBinaryFromFile("missing",@@INPUT@@);});rejectB(function(){e.RemoveBinary("missing");});
 rejectB(function(){e.RemoveBinary("png");});rejectB(function(){e.ReplaceBinaryFromFile("png",@@BINARY@@,"application/octet-stream");});
 var token=e.BeginDocumentUpdate("binary rejected batch",'{}');
 try{rejectB(function(){e.AddBinaryFromFile(@@INPUT@@,"x");});}finally{e.EndDocumentUpdate(token);}
 e.BeginUndoUnit(document,"legacy rejected binary");try{rejectB(function(){e.AddBinaryFromFile(@@INPUT@@,"x");});}finally{e.EndUndoUnit(document);}
 return "PASS";
}catch(error){return String(error.message||error);}}
function unknownBinaryReferences(){try{var a=document.createElement("A"), root=document.getElementById("fbw_body");a.href="#png";a.innerText="unknown reference";root.appendChild(a);try{rejectB(function(){e.RemoveBinary("png",true);});aB(a.parentNode===root&&jB(e.GetBinaryInfo("png")).size>0,"unknown reference retained");}finally{root.removeChild(a);}return "PASS";}catch(error){return String(error.message||error);}}
function seedBinaryHistory(){try{aB(e.AddBinaryFromFile(@@INPUT@@,"seed")==="seed","seed prior history");return "PASS";}catch(error){return String(error.message||error);}}
function faultAddBinary(){try{rejectB(function(){e.AddBinaryFromFile(@@INPUT@@,"fault");});return "PASS";}catch(error){return String(error.message||error);}}
function faultReplaceBinary(){try{rejectB(function(){e.ReplaceBinaryFromFile("png",@@REPLACE@@);});return "PASS";}catch(error){return String(error.message||error);}}
function faultRemoveBinary(){try{rejectB(function(){e.RemoveBinary("png",true);});return "PASS";}catch(error){return String(error.message||error);}}
function persistBinaries(){try{e.ReplaceBinaryFromFile("png",@@REPLACE@@);e.AddBinaryFromFile(@@INPUT@@,"png");return "PASS";}catch(error){return String(error.message||error);}}
function exportedMutations(){try{var r=e.SaveAllBinaries(@@EXPORT@@,"replace");e.WriteTextFileEx(@@MANIFEST@@,r,true);aB(jB(r).errors===0,"export mutations");return "PASS";}catch(error){return String(error.message||error);}}
function lockedInputBinary(){try{rejectB(function(){e.AddBinaryFromFile(@@INPUT@@,"locked");});return "PASS";}catch(error){return String(error.message||error);}}
'@
    $manifest=Join-Path $scripts 'export.json'
    $js=$js.Replace('@@EXPORT@@',($export|ConvertTo-Json -Compress)).Replace('@@COUNT@@',[string]$expectedCount).Replace('@@MANIFEST@@',($manifest|ConvertTo-Json -Compress))
    foreach($entry in @{INPUT='input.png';REPLACE='replace.png';BINARY='input.bin';LARGE='large.bin';BROKEN='broken.png';EMPTY='empty.bin';OVERSIZE='oversize.bin';JPEG='input.jpeg';PDF='input.pdf'}.GetEnumerator()){$js=$js.Replace(('@@'+$entry.Key+'@@'),((Join-Path $scripts $entry.Value)|ConvertTo-Json -Compress))}
    $js | Set-Content (Join-Path $scripts 'stage3b.js') -Encoding utf8
    $fixture=Join-Path $iso.Root "api3.$Kind"; $report=Join-Path $iso.Root 'api3.txt'
    $body=if($Kind -eq 'fb2'){'<body><section><image l:href="#png"/><p>ordinary <image l:href="#png"/> paragraph</p></section></body>'}else{''}
    $binaries=''
    foreach($id in $payloads.Keys){$mime=if($id -eq 'png'){'image/png'}elseif($id -eq 'jpeg'){'image/jpeg'}else{'application/octet-stream'};$binaries+='<binary id="'+$id+'" content-type="'+$mime+'">'+[Convert]::ToBase64String($payloads[$id])+'</binary>'}
    ('<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0" xmlns:l="http://www.w3.org/1999/xlink"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>API3B</book-title><coverpage><image l:href="#png"/></coverpage><lang>en</lang></title-info><document-info><id>api3b</id><version>1</version></document-info></description>'+$body+$binaries+'</FictionBook>') | Set-Content $fixture -Encoding utf8
    $env:FBE_NEXT_TEST_MODE='1'; $env:FBE_NEXT_TEST_SCENARIO='scripting-stage3b'; $env:FBE_NEXT_SCRIPT_API_ROOT=$scripts
    $env:FBE_NEXT_TEST_SETTINGS_DIRECTORY=Join-Path $iso.DataPath 'Settings'; $env:FBE_NEXT_TRACE='0'
    $process=Start-Process $iso.Exe -WorkingDirectory $iso.Root -ArgumentList @("--$Mode",'-b',$report,$fixture) -WindowStyle Hidden -PassThru
    $code=Wait-IsolatedFbeProcess -Process $process -Isolation $iso -Scenario 'scripting-stage3b' -Report $report -TimeoutSeconds $TimeoutSeconds
    $text=if(Test-Path $report){Get-Content $report -Raw}else{'missing report'}
    if($code -ne 0 -or $text -notmatch 'result=PASS'){throw "Stage 3B failed: $code; $text"}
    $results=Get-Content $manifest -Raw | ConvertFrom-Json
    $payloads['png']=[IO.File]::ReadAllBytes((Join-Path $scripts 'replace.png'))
    $payloads['png_0']=[IO.File]::ReadAllBytes((Join-Path $scripts 'input.png'))
    if($results.objects.Count -ne $payloads.Count){throw 'Export manifest count'}
    $seen=@{}
    foreach($object in $results.objects){
        if($seen.ContainsKey($object.fileName.ToUpperInvariant())){throw 'Case-insensitive export collision'}
        $seen[$object.fileName.ToUpperInvariant()]=$true
        $actual=[IO.File]::ReadAllBytes((Join-Path $export $object.fileName))
        if([Convert]::ToBase64String($actual) -cne [Convert]::ToBase64String($payloads[$object.id])){throw "Binary bytes changed: $($object.id)"}
    }
    if(@(Get-ChildItem $export -Filter '*.tmp').Count){throw 'Leaked export temporary files'}
    $passed=$true; Write-Host "Scripting stage 3B binaries, Undo/Redo, save/reopen ($Mode/$Kind): PASS."
} finally {
    foreach($name in $names){[Environment]::SetEnvironmentVariable($name,$old[$name],'Process')}
    Complete-IsolatedFbeRuntime -Isolation $iso -Passed $passed
}
