[CmdletBinding()]
param([string]$FbeExe=(Join-Path $PSScriptRoot '../../out/Release/FBE.exe'))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$repoRoot=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$iso=New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'doc-examples'
$passed=$false
$names=@('TEMP','TMP','FBE_NEXT_TEST_MODE','FBE_NEXT_TEST_SCENARIO','FBE_NEXT_SCRIPT_API_ROOT','FBE_NEXT_TEST_SETTINGS_DIRECTORY')
$old=@{};foreach($name in $names){$old[$name]=[Environment]::GetEnvironmentVariable($name)}
try {
    $null=New-IsolatedFbeSettingsSeed $iso
    $root=Join-Path $iso.Root 'examples';New-Item -ItemType Directory $root | Out-Null
    & py -3 (Join-Path $repoRoot 'tools/docs/check_scripting_api_examples.py') --root $repoRoot --extract-dir $root
    if($LASTEXITCODE -ne 0){throw 'Handbook extraction failed'}
    $sources=@{}
    foreach($name in 'backup','source-export','binary-report','safe-clipboard','preferences') {
        $sources[$name]=[IO.File]::ReadAllText((Join-Path $root "complex-$name-0.js"))
    }
    $sourceJson=$sources|ConvertTo-Json -Compress
    $driver=@'
function Run(){}
var real=window.external, sources=@@SOURCES@@, testRoot=@@ROOT@@, messages=[], files={}, mode="", collision=false;
function assertDoc(value,label){if(!value)throw new Error(label);}
function parseDoc(value){return eval("("+value+")");}
var api={};
// Each noninteractive call retains its real positional COM arguments.
api.ClearLastScriptingError=function(){return real.ClearLastScriptingError();};
api.GetLastScriptingError=function(){return real.GetLastScriptingError();};
api.GetCurrentViewMode=function(){return real.GetCurrentViewMode();};
api.GetExtension=function(p){return real.GetExtension(p);};
api.GetDocumentFileName=function(){return real.GetDocumentFileName();};
api.GetDocumentDirectory=function(){return real.GetDocumentDirectory();};
api.CombinePath=function(a,b){return real.CombinePath(a,b);};
api.GetUUID=function(){return collision?"collision":real.GetUUID();};
api.GetTempDirectory=function(){return real.GetTempDirectory();};
api.PushStatusBarText=function(t){return real.PushStatusBarText(t);};
api.PopStatusBarText=function(t){return real.PopStatusBarText(t);};
api.BeginDocumentUpdate=function(a,o){return real.BeginDocumentUpdate(a,o);};
api.EndDocumentUpdate=function(t){return real.EndDocumentUpdate(t);};
api.SetStyleEx=function(d,p,s){return real.SetStyleEx(d,p,s);};
api.SaveDocumentCopy=function(p,o){var result=real.SaveDocumentCopy(p,o);files.backup=p;return result;};
api.SaveSerializedDocumentCopy=function(p,o){var result=real.SaveSerializedDocumentCopy(p,o);files.source=p;return result;};
api.ValidateCurrentDocument=function(o){return real.ValidateCurrentDocument(o);};
api.GetBinaryInfo=function(id){return real.GetBinaryInfo(id);};
api.WriteTextFileEx=function(p,t,o,e){var result=real.WriteTextFileEx(p,t,o,e);files.report=p;return result;};
api.GetSelectionText=function(){return real.GetSelectionText();};
api.SetClipboardHtml=function(h,t){return real.SetClipboardHtml(h,t);};
api.GetScriptSetting=function(n,d){return real.GetScriptSetting(n,d);};
api.SetScriptSetting=function(n,v){return real.SetScriptSetting(n,v);};
api.GetNBSP=function(){return real.GetNBSP();};
api.MsgBox=function(message){messages.push(String(message));};
// UI is deterministic; this test does not claim native dialog coverage.
api.ShowMessage=function(m,t,b,i){messages.push(String(m));return b==="yesnocancel"?(mode==="cancel"?'{"status":"cancelled","button":"cancel"}':'{"status":"ok","button":"yes"}'):'{"status":"ok","button":"ok"}';};
api.SelectFromList=function(o){var opts=parseDoc(o);assertDoc(opts.items.length===1,"binary list");return mode==="cancel"?'{"status":"cancelled"}':mode==="missing"?'{"status":"ok","selectedId":"absent"}':'{"status":"ok","selectedId":"cover"}';};
api.SelectMultipleFromList=function(o){var opts=parseDoc(o);if(mode==="preferences-reopen")assertDoc(opts.selectedIds.join(",")==="spaces,nbsp","saved preferences");return mode==="cancel"||mode==="preferences-reopen"?'{"status":"cancelled"}':'{"status":"ok","selectedIds":["spaces","nbsp"]}';};
api.SelectSaveFile=function(o){assertDoc(parseDoc(o).defaultExtension==="fb2","save options");if(mode==="cancel")return '{"status":"cancelled"}';var p=testRoot+(mode==="conflict"?"\\existing.fbd":"\\export.fbd");return '{"status":"ok","path":"'+p.replace(/\\/g,"\\\\")+'"}';};
function executeDoc(name,selection){
 messages=[];mode=selection||"";
 try {new Function("window","document",sources[name]+"\nRun();")({external:api},document);return messages.join("\n");}
 finally {collision=false;}
}
function probeText(){var stack=[document.getElementById("probe")],text="";while(stack.length){var node=stack.pop();if(node.nodeType===3)text+=node.nodeValue;else for(var child=node.lastChild;child;child=child.previousSibling)stack.push(child);}return text;}
function backupCancel(){try{executeDoc("backup","cancel");assertDoc(!files.backup,"cancel created backup");return "PASS";}catch(e){return String(e.message||e);}}
function backupConflict(){try{collision=true;executeDoc("backup","conflict");assertDoc(parseDoc(real.GetLastScriptingError()).code==="already-exists","copy conflict HRESULT");assertDoc(!files.backup,"conflict wrote backup");return "PASS";}catch(e){return String(e.message||e);}}
function backupSuccess(){try{var message=executeDoc("backup");assertDoc(files.backup&&document.getElementById("probe").className==="subtitle","backup and style");assertDoc(message.indexOf("Копия:")>=0,"backup notification");return "PASS";}catch(e){return String(e.message||e);}}
function preferencesCancel(){try{executeDoc("preferences","cancel");assertDoc(real.GetScriptSetting("cleanup.spaces",false)===false,"cancel saved preferences");return "PASS";}catch(e){return String(e.message||e);}}
function preferencesSuccess(){try{executeDoc("preferences");assertDoc(probeText().indexOf("г."+real.GetNBSP()+"Москва — столица")===0,"text normalization");assertDoc(document.getElementById("probe").getElementsByTagName("EM").length===1,"inline formatting preserved");assertDoc(real.GetScriptSetting("cleanup.spaces",false)&&real.GetScriptSetting("cleanup.nbsp",false),"preferences saved");return "PASS";}catch(e){return String(e.message||e);}}
function preferencesReopen(){try{executeDoc("preferences","preferences-reopen");return "PASS";}catch(e){return String(e.message||e);}}
function binaryCancel(){try{executeDoc("binary-report","cancel");assertDoc(!files.report,"cancel wrote report");return "PASS";}catch(e){return String(e.message||e);}}
function binaryMissing(){try{executeDoc("binary-report","missing");assertDoc(parseDoc(real.GetLastScriptingError()).hasError&&!files.report,"missing binary accepted");return "PASS";}catch(e){return String(e.message||e);}}
function binarySuccess(){try{executeDoc("binary-report");assertDoc(files.report,"report absent");var report=real.ReadTextFile(files.report,"utf-8");assertDoc(report.indexOf("ID: cover")>=0&&report.indexOf("MIME: image/png")>=0&&report.indexOf("1 x 1")>=0,"binary report content");return "PASS";}catch(e){return String(e.message||e);}}
function prepareSelection(){var range=document.body.createTextRange();range.moveToElementText(document.getElementById("probe"));range.select();return "PASS";}
function clipboardCancel(){try{executeDoc("safe-clipboard","cancel");assertDoc(real.GetClipboardText()==="","cancel changed Clipboard");return "PASS";}catch(e){return String(e.message||e);}}
function clipboardSuccess(){try{executeDoc("safe-clipboard");assertDoc(real.GetClipboardText()===real.GetSelectionText(),"clipboard text");var html=real.GetClipboardHtml();assertDoc(html.indexOf("&lt;")>=0&&html.indexOf("&amp;")>=0,"escaped HTML");return "PASS";}catch(e){return String(e.message||e);}}
function clipboardFailure(){try{var message=executeDoc("safe-clipboard");assertDoc(message.indexOf("Копирование не выполнено:")>=0,"clipboard COM failure handling");return "PASS";}catch(e){return String(e.message||e);}}
function sourceCancel(){try{executeDoc("source-export","cancel");assertDoc(!files.source,"cancel wrote export");return "PASS";}catch(e){return String(e.message||e);}}
function sourceConflict(){try{executeDoc("source-export","conflict");assertDoc(parseDoc(real.GetLastScriptingError()).code==="already-exists"&&!files.source,"export conflict");return "PASS";}catch(e){return String(e.message||e);}}
function sourceSuccess(){try{executeDoc("source-export");assertDoc(files.source,"export absent");var xml=real.ReadTextFile(files.source,"utf-8");assertDoc(xml.indexOf("UnsavedExample")>=0&&xml.indexOf('encoding="utf-8"')>=0,"current Source bytes");real.WriteTextFileEx(testRoot+"\\done.txt","PASS",false,"utf-8");return "PASS";}catch(e){return String(e.message||e);}}
function sourceInvalid(){try{var message=executeDoc("source-export");assertDoc(message.indexOf("Экспорт отменён:")>=0,"invalid Source accepted");return "PASS";}catch(e){return String(e.message||e);}}
'@
    $driver=$driver.Replace('@@SOURCES@@',$sourceJson).Replace('@@ROOT@@',($root|ConvertTo-Json -Compress))
    [IO.File]::WriteAllText((Join-Path $root 'examples.js'),$driver,[Text.UTF8Encoding]::new($true))
    $fixture=Join-Path $iso.Root 'examples.fb2';$report=Join-Path $iso.Root 'examples.txt'
    $png='iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAwMCAO+a3ioAAAAASUVORK5CYII='
    $xml='<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0" xmlns:l="http://www.w3.org/1999/xlink"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>ExampleBook</book-title><lang>ru</lang></title-info><document-info><id>examples</id><version>1</version></document-info></description><body><section><p id="probe">г. Москва  — <emphasis>столица</emphasis>. A &lt; B &amp; C</p></section></body><binary id="cover" content-type="image/png">'+$png+'</binary></FictionBook>'
    [IO.File]::WriteAllText($fixture,$xml,[Text.UTF8Encoding]::new($false))
    $originalHash=(Get-FileHash $fixture).Hash
    $collision=Join-Path $iso.Root 'fbe-backup-collision.fb2';[IO.File]::WriteAllText($collision,'sentinel')
    [IO.File]::WriteAllText((Join-Path $root 'existing.fbd'),'sentinel')
    $env:TEMP=$iso.Root;$env:TMP=$iso.Root
    $env:FBE_NEXT_TEST_MODE='1';$env:FBE_NEXT_TEST_SCENARIO='scripting-doc-examples';$env:FBE_NEXT_SCRIPT_API_ROOT=$root
    $env:FBE_NEXT_TEST_SETTINGS_DIRECTORY=Join-Path $iso.DataPath 'Settings'
    $process=Start-Process $iso.Exe -WorkingDirectory $iso.Root -ArgumentList @('--portable','-b',$report,$fixture) -WindowStyle Hidden -PassThru
    $code=Wait-IsolatedFbeProcess -Process $process -Isolation $iso -Scenario 'doc-examples' -Report $report -TimeoutSeconds 45
    $text=if(Test-Path $report){Get-Content $report -Raw}else{'missing report'}
    if($code -ne 0 -or $text -notmatch 'result=PASS' -or -not(Test-Path (Join-Path $root 'done.txt'))){throw "Handbook examples failed: $text"}
    if((Get-FileHash $fixture).Hash -cne $originalHash){throw 'Original fixture changed on disk'}
    if([IO.File]::ReadAllText($collision) -cne 'sentinel' -or [IO.File]::ReadAllText((Join-Path $root 'existing.fbd')) -cne 'sentinel'){throw 'Conflict replaced destination bytes'}
    $backups=@(Get-ChildItem $iso.Root -Filter 'fbe-backup-*.fb2' | Where-Object Name -ne 'fbe-backup-collision.fb2')
    if($backups.Count -ne 1){throw 'Expected exactly one independent backup'}
    $backup=[Xml.XmlDocument]::new();$backup.Load($backups[0].FullName)
    if($backup.SelectSingleNode('//*[@id="probe"]').LocalName -cne 'p' -or $backup.DocumentElement.InnerText -notmatch 'ExampleBook'){throw 'Backup lost original content'}
    $export=[Xml.XmlDocument]::new();$export.Load((Join-Path $root 'export.fbd'))
    if($export.DocumentElement.InnerText -notmatch 'UnsavedExample' -or $export.SelectNodes('//*[local-name()="body"]').Count -ne 1){throw 'Source export lost content'}
    if(@(Get-ChildItem $iso.Root -Recurse -Filter 'fbs*.tmp').Count){throw 'Snapshot temporary leaked'}
    Write-Host $text;Write-Host 'PASS: handbook E-K, real APIs with scripted UI; system Clipboard untouched'
    $passed=$true
}finally{
    foreach($name in $names){[Environment]::SetEnvironmentVariable($name,$old[$name])}
    Complete-IsolatedFbeRuntime -Isolation $iso -Passed $passed
}

