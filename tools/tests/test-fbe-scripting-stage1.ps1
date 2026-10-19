[CmdletBinding()]
param([string]$FbeExe = (Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe'), [int]$TimeoutSeconds = 90, [ValidateSet("portable","installed")][string]$Mode = "portable", [switch]$Dialogs, [switch]$Environment)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
if ((Get-FileHash (Join-Path $root 'src\contracts\fbe.idl') -Algorithm SHA256).Hash -ne 'FB69F8A6C18DACED5C5E435C5E23286A30F89274586F569C9A730322CE3AAEEE') { throw 'Legacy COM ABI source changed.' }
$isolation = New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'scripting-api'
$passed = $false
try {
    $null = New-IsolatedFbeSettingsSeed -Isolation $isolation
    $scripts = Join-Path $isolation.Root 'api-scripts-Юникод'
    New-Item -ItemType Directory -Path $scripts | Out-Null
    $settings = Join-Path $isolation.DataPath 'Settings'
    '<Scripts version="1"><Script uid="11111111-1111-4111-8111-111111111111" path="outer.js" fingerprint="test"/><Script uid="22222222-2222-4222-8222-222222222222" path="child.js" fingerprint="test2"/></Scripts>' | Set-Content (Join-Path $settings 'ScriptRegistry.xml') -Encoding Unicode
    $outer = @'
var stage1Failure = "not executed";
function stage1Assert(value, label) { if(!value) throw new Error(label); }
function Run() {
 var e=window.external, self=e.GetCurrentScriptPath();
 if(e.GetCurrentScriptName()=="renamed.js"){stage1Failure=e.GetScriptSetting("rename","")==="survives"?"PASS":"rename settings";return;}
 try {
  stage1Assert(/^1\./.test(e.GetScriptingApiVersion()),"version");
  stage1Assert(!e.HasScriptingMethod("GetCurrentScriptPath\u0000other"),"embedded nul method");
  stage1Assert(e.HasScriptingMethod("BeginUndoUnit") && e.HasScriptingMethod("ApplySourceText"),"legacy lookup");
  stage1Assert(!e.HasScriptingMethod("Nonexistent45") && !e.HasScriptingMethod("SaveDocumentCopy"),"unknown/deferred lookup");
  stage1Assert(e.GetScriptingCapabilities().indexOf('"scriptContext":true')>=0,"capabilities");
  stage1Assert(self.toLowerCase()==@@OUTER@@.toLowerCase(),"outer path");
  stage1Assert(e.GetCurrentScriptName()=="outer.js","name");
  stage1Assert(e.GetCurrentScriptDirectory().toLowerCase()==(@@ROOT@@+"\\").toLowerCase(),"directory");
  stage1Assert(e.GetCurrentScriptId()=="11111111-1111-4111-8111-111111111111","registered uid");
  stage1Assert(e.GetScriptsRootDirectory().toLowerCase().replace(/[\\\/]$/, "")==@@ROOT@@.toLowerCase(),"scripts root");
  stage1Assert(e.GetInterfaceLanguage().indexOf("-")>0,"locale");
  stage1Assert(e.GetCurrentViewMode()=="design" && typeof(e.IsDocumentModified())=="boolean","environment");
  stage1Assert(e.GetUUID().length>0 && e.GetNBSP().length>0 && e.GetProgramVersion().length>0,"legacy invocation");
  stage1Assert(e.SetScriptSetting("shared","outer"),"write string");
  stage1Assert(e.SetScriptSetting("empty","") && e.GetScriptSetting("empty","default")==="","empty value");
  var tooLargeRejected=false;
  try {e.SetScriptSetting("too-large",new Array(32770).join("x"));}catch(error){tooLargeRejected=true;}
  stage1Assert(tooLargeRejected,"value size limit");
  stage1Assert(e.SetScriptSetting("number",-1.25) && e.GetScriptSetting("number",0)===-1.25,"number");
  stage1Assert(e.SetScriptSetting("boolean",false) && e.GetScriptSetting("boolean",true)===false,"boolean");
  stage1Assert(e.SetScriptSetting("unicode","Привет\u0000世界") && e.GetScriptSetting("unicode","")==="Привет\u0000世界","unicode/nul");
  stage1Assert(e.GetScriptSetting("shared",42)===42 && e.GetScriptSetting("missing",true)===true,"defaults/types");
  var coerced=false, rejected=false;
  try { e.SetScriptSetting("object",{toString:function(){coerced=true;return "bad";}}); } catch(error) { rejected=true; }
  stage1Assert(rejected && !coerced,"object rejection");
  apiRunCmd(@@CHILD@@);
  stage1Assert(e.GetScriptSetting("shared","")==="outer","script isolation");
  stage1Assert(e.RemoveScriptSetting("number") && e.GetScriptSetting("number",23)===23,"remove");
  stage1Assert(e.SetScriptSetting("rename","survives"),"rename seed");
  apiRunCmd(@@FALLBACK@@);
  stage1Assert(window.stage1Child===true && e.GetCurrentScriptPath()==self,"nested restore");
  try { apiRunCmd(@@ERROR@@); stage1Assert(false,"missing nested exception"); } catch(error) { stage1Assert(e.GetCurrentScriptPath()==self,"error restore"); }
  stage1Failure="PASS";
 } catch(error) { stage1Failure=String(error.message || error); }
}
function apiStage1EnvironmentValue() {
 return window.external.GetCurrentViewMode()+":"+window.external.IsDocumentModified();
}
function apiStage1StorageCheck(mode) {
 var e=window.external;
 if(mode===0)return e.GetScriptSetting("shared","default")==="default" && e.SetScriptSetting("shared","bad")===false;
 if(mode===1)return e.SetScriptSetting("shared","lost")===false && e.GetScriptSetting("shared","")==="outer";
 return e.SetScriptSetting("after-failure",true) && e.GetScriptSetting("after-failure",false)===true;
}
function stage1Json(value) {
 if(value===null)return "null";
 if(typeof(value)==="string")return '"'+value.replace(/\\/g,"\\\\").replace(/"/g,'\\"').replace(/\r/g,"\\r").replace(/\n/g,"\\n")+'"';
 if(typeof(value)!=="object")return String(value);
 var parts=[],key;
 if(value instanceof Array){for(var i=0;i<value.length;i++)parts.push(stage1Json(value[i]));return "["+parts.join(",")+"]";}
 for(key in value)if(value.hasOwnProperty(key))parts.push(stage1Json(key)+":"+stage1Json(value[key]));
 return "{"+parts.join(",")+"}";
}
function apiStage1DialogCase(index, title) {
 var e=window.external, root=@@ROOT@@, first=root+"\\файл один.txt", second=root+"\\второй.txt";
 function parse(text){return eval("("+text+")");}
 function equal(left,right){return String(left).replace(/[\\\/]$/,"").toLowerCase()==String(right).replace(/[\\\/]$/,"").toLowerCase();}
 var options={title:title,initialDirectory:root}, result;
 try {
  if(index===0){
   stage1Assert(parse(e.SelectOpenFile('{"unknown":1}')).status=="error","unknown options");
   stage1Assert(parse(e.SelectSaveFile('{"multiSelect":true}')).code=="invalid-options","invalid save");
   stage1Assert(parse(e.SelectOpenFile('{"title":"a","title":"b"}')).status=="error","duplicate options");
   stage1Assert(parse(e.SelectOpenFile('{"initialDirectory":'+stage1Json(root+"\\missing45")+'}')).status=="error","invalid directory");
   options.initialFileName=first;options.filters=[{name:"Текст",pattern:"*.txt"}];
   result=parse(e.SelectOpenFile(stage1Json(options)));
   stage1Assert(result.status=="ok" && equal(result.path,first),"open: "+stage1Json(result));
  }else if(index===1){
   options.multiSelect=true;options.initialFileName='"'+first+'" "'+second+'"';
   result=parse(e.SelectOpenFile(stage1Json(options)));
   stage1Assert(result.status=="ok" && result.paths.length===2 && ((equal(result.paths[0],first)&&equal(result.paths[1],second))||(equal(result.paths[1],first)&&equal(result.paths[0],second))),"multi: "+stage1Json(result));
  }else if(index===2){
   options.initialFileName="новый результат";options.defaultExtension="txt";
   result=parse(e.SelectSaveFile(stage1Json(options)));
   stage1Assert(result.status=="ok" && equal(result.path,root+"\\новый результат.txt"),"save: "+stage1Json(result));

  }else if(index===3){
   result=parse(e.SelectFolder(stage1Json(options)));
   stage1Assert(result.status=="ok" && equal(result.path,root),"folder: "+stage1Json(result));
  }else if(index===4){
   result=parse(e.SelectOpenFile(stage1Json(options)));stage1Assert(result.status=="cancelled","file cancellation");
  }else{
   var oldModal=e.GetModalResult();
   result=parse(e.InputBoxEx(stage1Json({title:title,prompt:"Проверка пустой строки",value:""})));
   stage1Assert(result.accepted===(index===5) && result.value==="" && e.GetModalResult()===oldModal,"input/cancel/legacy modal result");
  }
  return "PASS";
 }catch(error){return String(error.message||error);}
}
function apiStage1Results() {
 var e=window.external;
 if(e.GetCurrentScriptPath()!="" || e.GetCurrentScriptDirectory()!="" || e.GetCurrentScriptName()!="" || e.GetCurrentScriptId()!="") return "leaked context";
 return stage1Failure;
}
'@
    $outer = $outer.Replace('@@STORE@@', ((Join-Path $settings 'ScriptSettings.xml') | ConvertTo-Json -Compress)).Replace('@@FALLBACK@@', ((Join-Path $scripts 'fallback.js') | ConvertTo-Json -Compress)).Replace('@@OUTER@@', ((Join-Path $scripts 'outer.js') | ConvertTo-Json -Compress)).Replace('@@ROOT@@', ($scripts | ConvertTo-Json -Compress)).Replace('@@CHILD@@', ((Join-Path $scripts 'child.js') | ConvertTo-Json -Compress)).Replace('@@ERROR@@', ((Join-Path $scripts 'error.js') | ConvertTo-Json -Compress))
    $outer | Set-Content (Join-Path $scripts 'outer.js') -Encoding utf8
    'function Run(){var e=window.external;e.SetScriptSetting("shared","child");e.SetScriptSetting("clear",true);if(!e.ClearScriptSettings() || e.GetScriptSetting("clear",false)!==false)throw new Error("clear");window.stage1Child=e.GetCurrentScriptName()=="child.js" && e.GetCurrentScriptId()=="22222222-2222-4222-8222-222222222222";}' | Set-Content (Join-Path $scripts 'child.js') -Encoding utf8
    'function Run(){if(window.external.GetCurrentScriptId()!="")throw new Error("unexpected UID");throw new Error("intentional");}' | Set-Content (Join-Path $scripts 'error.js') -Encoding utf8
    'function Run(){var e=window.external;if(e.GetCurrentScriptId()!=="" || e.GetScriptSetting("shared","missing")!=="missing" || !e.SetScriptSetting("shared","path-scope"))throw new Error("path fallback");}' | Set-Content (Join-Path $scripts 'fallback.js') -Encoding utf8
    'one' | Set-Content (Join-Path $scripts 'файл один.txt') -Encoding utf8
    'two' | Set-Content (Join-Path $scripts 'второй.txt') -Encoding utf8
    $fixture = Join-Path $isolation.Root 'api.fb2'; $report = Join-Path $isolation.Root 'api.tsv'
    '<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>API</book-title><lang>en</lang></title-info><document-info><id>scripting-api</id><version>1</version></document-info></description><body><section><p>ordinary paragraph</p></section></body></FictionBook>' | Set-Content $fixture -Encoding utf8
    $oldMode,$oldScenario,$oldRoot,$oldSettings=$env:FBE_NEXT_TEST_MODE,$env:FBE_NEXT_TEST_SCENARIO,$env:FBE_NEXT_SCRIPT_API_ROOT,$env:FBE_NEXT_TEST_SETTINGS_DIRECTORY
    try {
        $env:FBE_NEXT_TEST_SETTINGS_DIRECTORY=$settings; $env:FBE_NEXT_TEST_MODE='1'; $env:FBE_NEXT_TEST_SCENARIO=if($Dialogs){'scripting-dialogs'}elseif($Environment){'scripting-environment'}else{'scripting-stage1'}; $env:FBE_NEXT_SCRIPT_API_ROOT=$scripts
        $process=Start-Process -FilePath $isolation.Exe -ArgumentList @((('--' + $Mode)),'-b',$report,$fixture) -WorkingDirectory $isolation.Root -WindowStyle Hidden -PassThru
        $code=Wait-IsolatedFbeProcess -Process $process -Isolation $isolation -Scenario 'scripting-stage1' -Report $report -TimeoutSeconds $TimeoutSeconds
        $details=if(Test-Path $report){Get-Content $report -Raw}else{'missing report'}
        if($code -ne 0 -or $details -notmatch 'result=PASS'){throw "Scripting API runtime failed: $code; $details"}
    } finally {$env:FBE_NEXT_TEST_MODE,$env:FBE_NEXT_TEST_SCENARIO,$env:FBE_NEXT_SCRIPT_API_ROOT,$env:FBE_NEXT_TEST_SETTINGS_DIRECTORY=$oldMode,$oldScenario,$oldRoot,$oldSettings}
    $passed=$true
    Write-Host "Scripting stage 1 COM/JScript runtime ($Mode; dialogs=$Dialogs; environment=$Environment): PASS."
} finally {
    if(-not ([IO.Path]::GetFullPath($isolation.Root).StartsWith([IO.Path]::GetFullPath([IO.Path]::GetTempPath()),[StringComparison]::OrdinalIgnoreCase))){throw 'Unexpected runtime directory'}
    Complete-IsolatedFbeRuntime -Isolation $isolation -Passed $passed
}
