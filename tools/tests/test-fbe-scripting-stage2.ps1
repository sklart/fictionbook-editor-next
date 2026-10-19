[CmdletBinding()]
param([string]$FbeExe = (Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe'), [ValidateSet('portable','installed')][string]$Mode='portable', [int]$TimeoutSeconds=90)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$root=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
if ((Get-FileHash (Join-Path $root 'src/contracts/fbe.idl')).Hash -ne 'FB69F8A6C18DACED5C5E435C5E23286A30F89274586F569C9A730322CE3AAEEE') {throw 'Legacy ABI changed'}
$iso=New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'scripting-stage2'; $passed=$false
$oldMode,$oldScenario,$oldRoot,$oldSettings=$env:FBE_NEXT_TEST_MODE,$env:FBE_NEXT_TEST_SCENARIO,$env:FBE_NEXT_SCRIPT_API_ROOT,$env:FBE_NEXT_TEST_SETTINGS_DIRECTORY
try {
    $null=New-IsolatedFbeSettingsSeed -Isolation $iso
    $scripts=Join-Path $iso.Root 'api2-Юникод'; New-Item -ItemType Directory $scripts | Out-Null
    $script=@'
var stage2Phase="start";
var stage2Failure="not run", stage2Progress="", stage2SourceBefore="", stage2SourceAfter="";
function assert2(value,label){if(!value)throw new Error(label);}
function rejects2(callback,label){var rejected=false;try{callback();}catch(error){rejected=true;}assert2(rejected,label);}
function paragraph2(){return document.getElementById("fbw_body").getElementsByTagName("P")[0];}
function select2(){var r=document.body.createTextRange();r.moveToElementText(paragraph2());r.select();}
function Run(){try{
 var e=window.external,root=@@ROOT@@;
 var methods="CreateSelectionBookmark RestoreSelectionBookmark ReleaseSelectionBookmark GetSelectionText GetSelectionHtml GetSelectionInfo BeginDocumentUpdate EndDocumentUpdate BeginProgress SetProgress IsProgressCancelled EndProgress ReadTextFile WriteTextFile FileExists DirectoryExists CreateDirectory CombinePath GetFileName GetDirectoryName GetExtension GetTempDirectory GetTempFileName".split(" ");
 assert2(e.GetScriptingApiVersion()==="1.1.0","version");for(var i=0;i<methods.length;i++)assert2(e.HasScriptingMethod(methods[i]),methods[i]);
 assert2(!e.HasScriptingMethod("ProcessPendingUiEvents")&&!e.HasScriptingMethod("SaveDocumentCopy"),"deferred methods");
 var path=e.CombinePath(root,"каталог\\данные.txt");assert2(e.CreateDirectory(e.GetDirectoryName(path)),"mkdir");
 assert2(e.DirectoryExists(e.GetDirectoryName(path))&&!e.FileExists(e.GetDirectoryName(path)),"directory kind");
 var text="Привет 😀\r\nsecond\u0000end";
 assert2(e.WriteTextFile(path,text)&&e.FileExists(path)&&!e.DirectoryExists(path)&&e.ReadTextFile(path)===text,"utf8 roundtrip");
 assert2(e.WriteTextFile(path,"замена","utf-8-bom")&&e.ReadTextFile(path)==="замена","replace bom");
 assert2(e.WriteTextFile(path,text,"utf-16le")&&e.ReadTextFile(path)===text,"utf16 roundtrip");
 assert2(e.WriteTextFile(path,"Кириллица","windows-1251")&&e.ReadTextFile(path,"windows-1251")==="Кириллица","cp1251");
 rejects2(function(){e.WriteTextFile(path,"😀","windows-1251");},"unmappable");assert2(e.ReadTextFile(path,"windows-1251")==="Кириллица","preserved on conversion failure");
 rejects2(function(){e.WriteTextFile(root+"\\readonly.txt","new");},"readonly");assert2(e.ReadTextFile(root+"\\readonly.txt")==="original","readonly preserved");
 rejects2(function(){e.ReadTextFile(root+"\\invalid.txt");},"invalid utf8");rejects2(function(){e.ReadTextFile(root+"\\invalid16.txt");},"invalid utf16");rejects2(function(){e.ReadTextFile(root+"\\large.txt");},"file limit");rejects2(function(){e.ReadTextFile(root+"\\missing.txt");},"missing");
 rejects2(function(){e.WriteTextFile(path,"\ud800","utf-16le");},"invalid surrogate");rejects2(function(){e.WriteTextFile(path,"x","unsupported");},"encoding");rejects2(function(){e.FileExists(path+"\u0000bad");},"nul path");
 assert2(e.GetFileName(path)==="данные.txt"&&e.GetExtension(path)===".txt"&&e.GetExtension("x")==="","path parts");
 assert2(e.GetDirectoryName("C:\\x")==="C:\\"&&e.GetFileName("relative.txt")==="relative.txt","path roots");
 assert2(e.DirectoryExists(e.GetTempDirectory()),"temp directory");var tmp=e.GetTempFileName();assert2(e.FileExists(tmp),"temp file");window.stage2TempFile=tmp;
 assert2(e.GetUUID().length>0&&typeof(e.GetNBSP())==="string","legacy methods");
 stage2Phase="selection getters";paragraph2().innerText="selected words";select2();assert2(e.GetSelectionText().replace(/\r\n$/,"")==="selected words"&&e.GetSelectionHtml().indexOf("selected words")>=0,"selection text/html: "+e.GetSelectionText()+" / "+e.GetSelectionHtml());
 assert2(e.GetSelectionInfo().indexOf('"view":"design"')>=0,"selection info");
 stage2Phase="selection bookmark";var mark=e.CreateSelectionBookmark(),r=document.body.createTextRange();r.moveToElementText(paragraph2());r.collapse(true);r.select();
 assert2(e.RestoreSelectionBookmark(mark)&&e.GetSelectionText().replace(/\r\n$/,"")==="selected words","restore");assert2(e.ReleaseSelectionBookmark(mark)&&!e.RestoreSelectionBookmark(mark)&&!e.ReleaseSelectionBookmark(mark),"release");
 stage2Phase="bookmark invalidation";mark=e.CreateSelectionBookmark();paragraph2().innerText="changed";assert2(!e.RestoreSelectionBookmark(mark),"DOM edit invalidates");paragraph2().innerText="selected words";
 stage2Phase="control selection";var image=document.createElement("IMG");image.width=16;image.height=16;paragraph2().appendChild(image);
 var control=document.body.createControlRange();control.add(image);control.select();
 assert2(e.GetSelectionText()===""&&e.GetSelectionHtml().toLowerCase().indexOf("<img")>=0&&e.GetSelectionInfo().indexOf('"controlCount":1')>=0,"control selection");
 mark=e.CreateSelectionBookmark();select2();assert2(e.RestoreSelectionBookmark(mark)&&e.ReleaseSelectionBookmark(mark),"control bookmark");image.parentNode.removeChild(image);select2();
 stage2Phase="batch";rejects2(function(){e.BeginDocumentUpdate("bad",'{"unknown":true}');},"batch options");
 var beforeBatch=e.GetSourceText();var outer=e.BeginDocumentUpdate("Stage2 grouped change",'{"suspendRedraw":true}');
 assert2(!e.ApplySourceText(beforeBatch,"blocked replacement"),"replacement blocked during batch");paragraph2().innerText="batch one";var inner=e.BeginDocumentUpdate("inner","{}");
 rejects2(function(){e.EndDocumentUpdate(outer);},"LIFO");paragraph2().innerText="batch two";assert2(e.EndDocumentUpdate(inner),"inner end");
 window.stage2OuterToken=outer;
 stage2Phase="nested error";try{apiRunCmd(@@CHILD@@);assert2(false,"nested exception");}catch(error){assert2(e.GetCurrentScriptName()==="stage2.js","context restored");}
 paragraph2().innerText="batch two";assert2(e.EndDocumentUpdate(outer),"outer end");rejects2(function(){e.EndDocumentUpdate(outer);},"stale update");
 stage2Failure="PASS";
}catch(error){stage2Failure=stage2Phase+": "+String(error.message||error);}}
function stage2TemporaryFile(){return window.stage2TempFile||"";}
function stage2Result(){return stage2Failure;}
function stage2Paragraph(){return paragraph2().innerText;}
function stage2Source(){try{var e=window.external;stage2SourceBefore=e.GetSourceText();var mark=e.CreateSelectionBookmark();assert2(e.RestoreSelectionBookmark(mark)&&e.ReleaseSelectionBookmark(mark),"source bookmark");assert2(e.GetSelectionInfo().indexOf('"view":"source"')>=0,"source info");e.GetSelectionText();e.GetSelectionHtml();window.stage2SourceToken=e.BeginDocumentUpdate("Source update","{}");return "PASS";}catch(error){return "stage2Source: "+String(error.message||error);}}
function stage2SourceEnd(){try{var e=window.external;assert2(e.EndDocumentUpdate(window.stage2SourceToken),"source end");stage2SourceAfter=e.GetSourceText();assert2(stage2SourceAfter===stage2SourceBefore+"  ","source edits");return "PASS";}catch(error){return "stage2SourceEnd: "+String(error.message||error);}}
function stage2SourceUndone(){return window.external.GetSourceText()===stage2SourceBefore?"PASS":"source undo";}
function stage2SourceRedone(){return window.external.GetSourceText()===stage2SourceAfter?"PASS":"source redo";}
function stage2Replaced(){try{var e=window.external;select2();var mark=e.CreateSelectionBookmark(),xml=e.GetSourceText();xml=xml.replace("batch two","replacement two");assert2(e.ApplySourceText(xml,"replace full DOM"),"apply");assert2(!e.RestoreSelectionBookmark(mark),"replacement invalidation");return "PASS";}catch(error){return "stage2Replaced: "+String(error.message||error);}}
function stage2ProgressBegin(){try{var e=window.external;rejects2(function(){e.BeginProgress("invalid",0);},"zero maximum");stage2Progress=e.BeginProgress("FBE stage2 cancellation",10);assert2(e.SetProgress(stage2Progress,2,"Работа"),"set progress");rejects2(function(){e.BeginProgress("second",1);},"single progress");rejects2(function(){e.SetProgress(stage2Progress,11,"bad");},"range");return "PASS";}catch(error){return "stage2ProgressBegin: "+String(error.message||error);}}
function stage2ProgressCancelled(){return window.external.IsProgressCancelled(stage2Progress)?"PASS":"waiting";}
function stage2ProgressEnd(){try{var e=window.external;assert2(e.EndProgress(stage2Progress),"end progress");rejects2(function(){e.IsProgressCancelled(stage2Progress);},"stale progress");return "PASS";}catch(error){return "stage2ProgressEnd: "+String(error.message||error);}}
'@
    $script=$script.Replace('@@ROOT@@',($scripts|ConvertTo-Json -Compress)).Replace('@@CHILD@@',((Join-Path $scripts 'child.js')|ConvertTo-Json -Compress))
    $script | Set-Content (Join-Path $scripts 'stage2.js') -Encoding utf8
    'function Run(){var e=window.external;var rejected=false;try{e.EndDocumentUpdate(window.stage2OuterToken);}catch(error){rejected=true;}if(!rejected)throw new Error("foreign token accepted");e.BeginDocumentUpdate("unfinished",''{"suspendRedraw":true}'');paragraph2().innerText="child mutation";e.BeginProgress("Unfinished child progress",2);throw new Error("intentional");}' | Set-Content (Join-Path $scripts 'child.js') -Encoding utf8
    [IO.File]::WriteAllText((Join-Path $scripts 'readonly.txt'),'original',[Text.UTF8Encoding]::new($false)); (Get-Item (Join-Path $scripts 'readonly.txt')).IsReadOnly=$true
    [IO.File]::WriteAllBytes((Join-Path $scripts 'invalid.txt'),[byte[]]@(0xff,0xff))
    [IO.File]::WriteAllBytes((Join-Path $scripts 'invalid16.txt'),[byte[]]@(0xff,0xfe,0x00,0xd8))
    $large=[IO.File]::Create((Join-Path $scripts 'large.txt'));try{$large.SetLength(8*1024*1024+1)}finally{$large.Dispose()}
    $fixture=Join-Path $iso.Root 'api2.fb2'; $report=Join-Path $iso.Root 'api2.txt'
    '<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>API2</book-title><lang>en</lang></title-info><document-info><id>api2</id><version>1</version></document-info></description><body><section><p>selected words</p></section></body></FictionBook>' | Set-Content $fixture -Encoding utf8
    $env:FBE_NEXT_TEST_MODE='1';$env:FBE_NEXT_TEST_SCENARIO='scripting-stage2';$env:FBE_NEXT_SCRIPT_API_ROOT=$scripts;$env:FBE_NEXT_TEST_SETTINGS_DIRECTORY=Join-Path $iso.DataPath 'Settings'
    $process=Start-Process $iso.Exe -WorkingDirectory $iso.Root -ArgumentList @("--$Mode",'-b',$report,$fixture) -WindowStyle Hidden -PassThru
    $code=Wait-IsolatedFbeProcess -Process $process -Isolation $iso -Scenario 'scripting-stage2' -Report $report -TimeoutSeconds $TimeoutSeconds
    $text=if(Test-Path $report){Get-Content $report -Raw}else{'missing report'}
    if($code -ne 0 -or $text -notmatch 'result=PASS'){throw "Stage 2 failed: $code; $text"}
    $passed=$true; Write-Host "Scripting stage 2 real COM/JScript ($Mode): PASS."
} finally {
    $env:FBE_NEXT_TEST_MODE,$env:FBE_NEXT_TEST_SCENARIO,$env:FBE_NEXT_SCRIPT_API_ROOT,$env:FBE_NEXT_TEST_SETTINGS_DIRECTORY=$oldMode,$oldScenario,$oldRoot,$oldSettings
    if(-not ([IO.Path]::GetFullPath($iso.Root).StartsWith([IO.Path]::GetFullPath([IO.Path]::GetTempPath()),[StringComparison]::OrdinalIgnoreCase))){throw 'Unsafe isolation root'}
    Complete-IsolatedFbeRuntime -Isolation $iso -Passed $passed
}
