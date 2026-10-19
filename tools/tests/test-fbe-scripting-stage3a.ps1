[CmdletBinding()]
param(
    [string]$FbeExe=(Join-Path $PSScriptRoot '../../out/Release/FBE.exe'),
    [ValidateSet('portable','installed')][string]$Mode='portable',
    [ValidateSet('fb2','fbd')][string]$Kind='fb2',
    [switch]$Trace,
    [switch]$MissingSchema,
    [int]$TimeoutSeconds=90
)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$root=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
if((Get-FileHash (Join-Path $root 'src/contracts/fbe.idl')).Hash -ne 'FB69F8A6C18DACED5C5E435C5E23286A30F89274586F569C9A730322CE3AAEEE'){throw 'Legacy COM contract changed'}
$iso=New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'scripting-stage3a'
$passed=$false
$names=@('FBE_NEXT_TEST_MODE','FBE_NEXT_TEST_SCENARIO','FBE_NEXT_SCRIPT_API_ROOT','FBE_NEXT_TEST_SETTINGS_DIRECTORY','FBE_NEXT_TRACE')
$old=@{}; foreach($name in $names){$old[$name]=[Environment]::GetEnvironmentVariable($name,'Process')}
try {
    $null=New-IsolatedFbeSettingsSeed $iso
    $scripts=Join-Path $iso.Root 'api3-Юникод'; New-Item -ItemType Directory $scripts | Out-Null
    $js=@'
var result3="not run", oldSourceDiagnostic3="";
function assert3(ok,label){if(!ok)throw new Error(label);}
function json3(text){return eval("("+text+")");} // Only trusted results of the native test API.
function rejects3(callback){var rejected=false;try{callback();}catch(error){rejected=true;}assert3(rejected,"missing COM error");}
function error3(){return json3(window.external.GetLastScriptingError());}
function Run(){try{
 var e=window.external;
 if(@@MISSING@@){for(var failureIndex=0;failureIndex<3;failureIndex++){var failed=json3(e.ValidateCurrentDocument("{}"));assert3(failed.status==="error"&&failed.valid===null&&failed.errors[0].code==="validation-unavailable","technical validation failure "+failureIndex);assert3(error3().method==="ValidateCurrentDocument","technical error context");}result3="PASS";return;}
 assert3(e.GetScriptingApiVersion()==="1.2.0","version");
 var methods=["ValidateCurrentDocument","LogMessage","GetLastScriptingError","ClearLastScriptingError"];
 for(var i=0;i<methods.length;i++)assert3(e.HasScriptingMethod(methods[i]),methods[i]);
 var capabilities=json3(e.GetScriptingCapabilities()).capabilities;
 assert3(capabilities.documentValidation&&capabilities.logging&&capabilities.scriptingErrors,"capabilities");
 assert3(!e.HasScriptingMethod("SaveDocumentCopy")&&!e.HasScriptingMethod("GetBinaryInfo"),"later stages unavailable");
 assert3(!error3().hasError,"fresh context");
 rejects3(function(){e.ValidateCurrentDocument();}); assert3(error3().code==="bad-parameter-count","argument count");
 rejects3(function(){e.ValidateCurrentDocument(0);}); assert3(error3().code==="type-mismatch","argument type");
 rejects3(function(){e.ValidateCurrentDocument('{"unknown":true}');}); assert3(error3().code==="invalid-argument","options");
 rejects3(function(){e.ValidateCurrentDocument('{');});
 rejects3(function(){e.LogMessage("fatal","bad");}); assert3(error3().method==="LogMessage"&&error3().code==="invalid-argument","level error");
 rejects3(function(){e.LogMessage("info",new Array(514).join("x"));}); assert3(error3().code==="message-too-long","length error");
 var coerced=false;rejects3(function(){e.LogMessage("info",{toString:function(){coerced=true;return "x";}});});assert3(!coerced,"no object coercion");
 rejects3(function(){e.WriteTextFileEx(@@EXISTING@@,"lost",false);});
 var parent=e.GetLastScriptingError();assert3(error3().method==="WriteTextFileEx"&&error3().code==="already-exists","old API failure capture");
 assert3(e.FileExists(@@EXISTING@@)&&e.GetLastScriptingError()===parent,"success retains error");
 rejects3(function(){e.GetLastScriptingError(1);}); assert3(e.GetLastScriptingError()===parent,"getter does not overwrite");
 assert3(e.GetLastScriptingError()===parent,"repeat read");
 try{apiRunCmd(@@CHILD@@);}catch(error){}
 assert3(window.child3===true&&e.GetLastScriptingError()===parent,"nested isolation and unwinding");
 assert3(e.ClearLastScriptingError()&&!error3().hasError,"clear current context");
 var levels=["debug","info","warning","error"], enabled=e.IsDiagnosticTraceEnabled()!==0;
 for(i=0;i<levels.length;i++)assert3(e.LogMessage(levels[i],'UNICODE_PAYLOAD_Привет 😀 " \\ \r\n\t\u0000')===enabled,"log "+levels[i]);
 if(enabled){var limited=false,accepted=0;for(i=0;i<300;i++){if(e.LogMessage("info","bounded"))accepted++;else limited=true;}assert3(limited&&accepted<=256,"rate and lifetime limits");}
 assert3(!error3().hasError,"logging errors are explicit");
 e.ValidateSourceText("<bad>"); oldSourceDiagnostic3=e.GetLastSourceDiagnostic();
 result3="PASS";
}catch(error){result3=String(error.message||error);}}
function resultStage3(){return result3;}
function checkStage3(expected){try{
 var e=window.external, body=document.getElementById("fbw_body").innerHTML, title=document.getElementById("tiTitle").value;
 var selected=e.GetSelectionText(), html=e.GetSelectionHtml(), info=e.GetSelectionInfo(), mode=e.GetCurrentViewMode(), dirty=e.IsDocumentModified();
 for(var i=0;i<12;i++){
  var result=json3(e.ValidateCurrentDocument(" { } "));
  assert3(result.schemaVersion===1&&result.status==="ok"&&result.valid===expected,"validation result "+result.status+" "+result.valid);
  assert3(result.warnings.length===0,"warnings");
  if(expected)assert3(result.errors.length===0&&result.diagnosticsComplete,"valid result");
  else assert3(result.errors.length===1&&!result.diagnosticsComplete&&result.errors[0].code==="xml-invalid"&&result.errors[0].line>=0&&result.errors[0].column>=0&&result.errors[0].message.length>0,"first diagnostic");
 }
 assert3(e.GetLastSourceDiagnostic()===oldSourceDiagnostic3,"legacy Source diagnostic preserved");
 assert3(body===document.getElementById("fbw_body").innerHTML&&title===document.getElementById("tiTitle").value,"DOM preserved");
 assert3(selected===e.GetSelectionText()&&html===e.GetSelectionHtml()&&info===e.GetSelectionInfo(),"selection preserved");
 assert3(mode===e.GetCurrentViewMode()&&dirty===e.IsDocumentModified(),"view and dirty preserved");
 return "PASS";
}catch(error){return String(error.message||error);}}
function missingStage3(){try{var e=window.external,r=json3(e.ValidateCurrentDocument("{}"));assert3(r.status==="error"&&r.valid===null&&!r.diagnosticsComplete&&r.errors[0].code==="no-document","missing document result");assert3(error3().method==="ValidateCurrentDocument","technical error stored");return "PASS";}catch(error){return String(error.message||error);}}
function cleanStage3(){return error3().hasError?"leaked error":"PASS";}
function collectStage3(){if(typeof(CollectGarbage)==="function")CollectGarbage();return "PASS";}
function positionStage3(){return json3(window.external.ValidateCurrentDocument("{}")).errors[0].line>0?"PASS":"missing parser position";}
function editDescription3(){try{var e=window.external,f=document.getElementById("tiTitle");window.titleBefore3=f.value;var token=e.BeginDocumentUpdate("3A description edit",'{}');f.focus();var r=f.createTextRange();r.select();r.text="Changed 3A";e.EndDocumentUpdate(token);return "PASS";}catch(error){return String(error.message||error);}}
function undoneDescription3(){return document.getElementById("tiTitle").value===window.titleBefore3?"PASS":"description undo";}
function redoneDescription3(){return document.getElementById("tiTitle").value==="Changed 3A"?"PASS":"description redo";}
'@
    $js=$js.Replace('@@MISSING@@',$(if($MissingSchema){'true'}else{'false'})).Replace('@@CHILD@@',((Join-Path $scripts 'child.js')|ConvertTo-Json -Compress)).Replace('@@EXISTING@@',((Join-Path $scripts 'existing.txt')|ConvertTo-Json -Compress))
    $js | Set-Content (Join-Path $scripts 'stage3.js') -Encoding utf8
    'function Run(){var e=window.external;if(eval("("+e.GetLastScriptingError()+")").hasError)throw new Error("inherited parent error");try{e.LogMessage("bad","child");}catch(error){};window.child3=eval("("+e.GetLastScriptingError()+")").method==="LogMessage";e.ClearLastScriptingError();if(eval("("+e.GetLastScriptingError()+")").hasError)throw new Error("child clear");throw new Error("intentional child failure");}' | Set-Content (Join-Path $scripts 'child.js') -Encoding utf8
    'original' | Set-Content (Join-Path $scripts 'existing.txt') -Encoding utf8
    $fixture=Join-Path $iso.Root "api3.$Kind"; $report=Join-Path $iso.Root 'api3.txt'
    $body=if($Kind -eq 'fb2'){'<body><section><p>ordinary paragraph</p></section></body>'}else{''}
    ('<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>API3</book-title><lang>en</lang></title-info><document-info><id>api3</id><version>1</version></document-info></description>'+$body+'<binary id="image" content-type="image/png">AAECAw==</binary></FictionBook>') | Set-Content $fixture -Encoding utf8
    if($MissingSchema){Move-Item -LiteralPath (Join-Path $iso.Root 'FictionBook.xsd') -Destination (Join-Path $iso.Root 'FictionBook.xsd.disabled')}
    $env:FBE_NEXT_TEST_MODE='1'; $env:FBE_NEXT_TEST_SCENARIO=if($MissingSchema){'scripting-stage3a-missing-schema'}else{'scripting-stage3a'}; $env:FBE_NEXT_SCRIPT_API_ROOT=$scripts
    $env:FBE_NEXT_TEST_SETTINGS_DIRECTORY=Join-Path $iso.DataPath 'Settings'; $env:FBE_NEXT_TRACE=if($Trace){'1'}else{'0'}
    $process=Start-Process $iso.Exe -WorkingDirectory $iso.Root -ArgumentList @("--$Mode",'-b',$report,$fixture) -WindowStyle Hidden -PassThru
    $code=Wait-IsolatedFbeProcess -Process $process -Isolation $iso -Scenario 'scripting-stage3a' -Report $report -TimeoutSeconds $TimeoutSeconds
    $text=if(Test-Path $report){Get-Content $report -Raw}else{'missing report'}
    if($code -ne 0 -or $text -notmatch 'result=PASS'){throw "Stage 3A failed: $code; $text"}
    $diagnostics=if($Mode -eq 'portable'){Join-Path $iso.DataPath 'Diagnostics'}else{Join-Path $env:FBE_NEXT_TEST_SETTINGS_DIRECTORY 'Diagnostics'}
    $logs=@(Get-ChildItem $diagnostics -Filter 'fbe-*.log' -ErrorAction SilentlyContinue)
    if($Trace -and ($text -notmatch 'trace=1' -or !$logs.Count)){throw 'Trace scenario did not enable logging'}
    if(!$Trace -and $Mode -eq 'portable' -and $text -notmatch 'trace=0'){throw 'Disabled trace scenario was not exercised'}
    $allLogs=''
    foreach($log in $logs){$content=Get-Content $log.FullName -Raw;if($content -match 'UNICODE_PAYLOAD_|Привет'){throw 'LogMessage bypassed privacy filtering'};$allLogs+=$content}
    if($Trace -and !$MissingSchema){foreach($level in 'debug','info','warning','error'){if($allLogs -notmatch "level=$level; category=script-api; code=SA144"){throw "Missing logging level $level"}}}
    if([IO.File]::ReadAllText((Join-Path $scripts 'existing.txt')).Trim() -ne 'original'){throw 'Rejected write changed existing file'}
    if($text -match '(?m)^handles=[0-9,]+'){Write-Host $Matches[0]}
    $passed=$true; Write-Host "Scripting stage 3A ($Mode/$Kind; trace=$Trace): PASS."
} finally {
    foreach($name in $names){[Environment]::SetEnvironmentVariable($name,$old[$name],'Process')}
    Complete-IsolatedFbeRuntime -Isolation $iso -Passed $passed
}
