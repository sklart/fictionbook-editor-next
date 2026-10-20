[CmdletBinding()]
param([string]$FbeExe=(Join-Path $PSScriptRoot '../../out/Release/FBE.exe'),
    [ValidateSet('portable','installed')][string]$Mode='portable',
    [ValidateSet('dialogs','clipboard','clipboard-ole','status')][string]$Part='dialogs')
$ErrorActionPreference='Stop'
if ($Part -eq 'clipboard-ole' -and ($env:GITHUB_ACTIONS -ne 'true' -or
    $env:RUNNER_ENVIRONMENT -ne 'github-hosted' -or $env:FBE_NEXT_CLIPBOARD_CI_ACCOUNT -ne [Security.Principal.WindowsIdentity]::GetCurrent().User.Value)) {
    throw 'Real OLE Clipboard test requires the disposable CI account launcher'
}
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$iso=New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'api3c'
$passed=$false
$names=@('FBE_NEXT_TEST_MODE','FBE_NEXT_TEST_SCENARIO','FBE_NEXT_SCRIPT_API_ROOT','FBE_NEXT_TEST_SETTINGS_DIRECTORY','FBE_NEXT_TRACE')
$old=@{};foreach($name in $names){$old[$name]=[Environment]::GetEnvironmentVariable($name)}
try {
    $null=New-IsolatedFbeSettingsSeed $iso
    $root=Join-Path $iso.Root 'api3c';New-Item -ItemType Directory $root | Out-Null
    $script=@'
var ext=window.external, result3c="not run";
function j3c(s){return eval("("+s+")");}
function a3c(ok,label){if(!ok)throw new Error(label);}
function bad3c(f){var bad=false;try{f();}catch(e){bad=true;}a3c(bad,"missing COM rejection");}
function Run(){try{
 var names=["ShowMessage","SelectFromList","SelectMultipleFromList","ChooseColor","GetClipboardText","SetClipboardText","GetClipboardHtml","SetClipboardHtml","PushStatusBarText","PopStatusBarText","ClearStatusBarText"];
 for(var i=0;i<names.length;i++)a3c(ext.HasScriptingMethod(names[i]),names[i]);
 var caps=j3c(ext.GetScriptingCapabilities());a3c(ext.GetScriptingApiVersion()==="1.4.0"&&caps.apiVersion==="1.4.0"&&caps.capabilities.extendedDialogs&&caps.capabilities.clipboardText&&caps.capabilities.clipboardHtml&&caps.capabilities.statusStack,"version/capabilities");
 a3c(j3c(ext.SelectFromList("{}")).status==="error","missing items");
 a3c(j3c(ext.SelectFromList('{"items":[{"id":"a","label":"a"},{"id":"a","label":"b"}]}')).status==="error","duplicate ID");
 a3c(j3c(ext.SelectFromList('{"items":[{"id":"a","label":"a"}],"selectedId":"absent"}')).status==="error","absent ID");
 a3c(j3c(ext.SelectFromList('{"items":[{"id":"a","label":"\\ud800"}]}')).status==="error","invalid Unicode");
 a3c(j3c(ext.ChooseColor("#12345Z")).status==="error","bad color");
 a3c(j3c(ext.ShowMessage("x","x","bad","info")).status==="error","bad buttons");
 bad3c(function(){ext.ShowMessage("x");});bad3c(function(){ext.SelectFromList(1);});
 var paragraphs=document.getElementById("fbw_body").getElementsByTagName("P");
 var range=document.body.createTextRange();range.moveToElementText(paragraphs[0]);range.collapse(true);range.moveEnd("character",8);range.select();result3c="PASS";
}catch(e){result3c=String(e.message||e);}}
function stage3cResult(){return result3c;}
function stage3cPushA(){window.tokenA=ext.PushStatusBarText("outer");return "PASS";}
function stage3cPushB(){window.tokenB=ext.PushStatusBarText("inner");return "PASS";}
function stage3cBadPop(){try{bad3c(function(){ext.PopStatusBarText(window.tokenA);});bad3c(function(){ext.PopStatusBarText("foreign");});return "PASS";}catch(e){return String(e.message||e);}}
function stage3cPopB(){try{a3c(ext.PopStatusBarText(window.tokenB),"pop B");bad3c(function(){ext.PopStatusBarText(window.tokenB);});return "PASS";}catch(e){return String(e.message||e);}}
function stage3cPopA(){ext.PopStatusBarText(window.tokenA);return "PASS";}
function stage3cClear(){ext.PushStatusBarText("local");ext.ClearStatusBarText();return "PASS";}
function stage3cLegacy(){ext.SetStatusBarText("legacy-current");return "PASS";}
function stage3cNested(){try{
 apiRunCmd(@@CHILD@@);a3c(window.childChecked,"foreign parent token");
 var failed=false;try{apiRunCmd(@@ERROR@@);}catch(e){failed=true;}a3c(failed,"nested JScript failure");
 failed=false;try{apiRunCmd(@@COMERROR@@);}catch(e){failed=true;}a3c(failed,"nested COM failure");return "PASS";
}catch(e){return String(e.message||e);}}
function stage3cClipboard(){var step="text";try{
 var text="FBE45_PRIVATE_CLIPBOARD_CANARY Буфер 😀\r\nСледующая строка";
 a3c(ext.SetClipboardText(text)&&ext.GetClipboardText()===text,"Unicode text");
 a3c(ext.GetClipboardHtml()==="","absent HTML");
 a3c(ext.SetClipboardText("")&&ext.GetClipboardText()==="","empty text");
 var maximum="x";for(var n=0;n<20;n++)maximum+=maximum;
 a3c(ext.SetClipboardText(maximum)&&ext.GetClipboardText()===maximum,"maximum text and HGLOBAL padding");
 step="html";ext.SetClipboardHtml("<p><b>Кириллица 😀</b></p>","Кириллица 😀");
 a3c(ext.GetClipboardHtml()==="<p><strong>Кириллица 😀</strong></p>"&&ext.GetClipboardText()==="Кириллица 😀","HTML/UTF8 roundtrip");
 step="invalid-input";var kept=ext.GetClipboardHtml();
 bad3c(function(){ext.SetClipboardHtml("<object></object>","x");});
 bad3c(function(){ext.SetClipboardHtml("<a href='javascript:alert(1)'>x</a>","x");});
 bad3c(function(){ext.SetClipboardText("bad\ud800");});
 bad3c(function(){ext.SetClipboardHtml("<p>x</p>","bad\u0000tail");});
 a3c(ext.GetClipboardHtml()===kept,"prepare failure preserved both formats");
 step="normalize";ext.SetClipboardHtml("<p onclick='alert(1)'>safe<script>alert(1)</script></p>","safe");
 var clean=ext.GetClipboardHtml();a3c(clean.indexOf("onclick")<0&&clean.indexOf("script")<0,"active content removed");
 bad3c(function(){ext.GetClipboardText(1);});bad3c(function(){ext.SetClipboardText(1);});
 return "PASS";
}catch(e){return step+": "+String(e.message||e);}}
function stage3cBusy(){try{bad3c(function(){ext.GetClipboardText();});bad3c(function(){ext.GetClipboardHtml();});return "PASS";}catch(e){return String(e.message||e);}}
function stage3cOleClipboard(){try{
 var text="FBE45_PRIVATE_CLIPBOARD_CANARY Буфер 😀\r\nСледующая строка";
 a3c(ext.SetClipboardText(text)&&ext.GetClipboardText()===text,"OLE Unicode text roundtrip");
 a3c(ext.SetClipboardHtml("<p onclick='bad()'><b>Кириллица 😀</b><script>bad()</script></p>",text),"OLE HTML set");
 a3c(ext.GetClipboardText()===text,"OLE exact plain text");
 a3c(ext.GetClipboardHtml()==="<p><strong>Кириллица 😀</strong></p>","OLE sanitized HTML");
 return "PASS";
}catch(e){return "OLE roundtrip assertion failed";}}
function stage3cBadHtml(){try{bad3c(function(){ext.GetClipboardHtml();});return "PASS";}catch(e){return String(e.message||e);}}
function stage3cBadText(){try{bad3c(function(){ext.GetClipboardText();});return "PASS";}catch(e){return String(e.message||e);}}
function stage3cDialog(title,index){try{
 var selection=ext.GetSelectionText()+"|"+ext.GetSelectionHtml();a3c(ext.GetSelectionText().length>0,"nonempty selection fixture");
 var r,opts='{"title":"'+title+'","prompt":"Выберите элемент","items":[{"id":"a","label":"Кириллица '+new Array(100).join('длинная подпись ')+'"},{"id":"b","label":"Вторая строка"}]';
 if(index===0||index===1){r=j3c(ext.ShowMessage("Сообщение",title,"yesnocancel","question"));a3c(index===0?r.status==="ok"&&r.button==="no":r.status==="cancelled"&&r.button==="cancel","message result");}
 if(index===2){r=j3c(ext.SelectFromList(opts+',"selectedId":"b"}'));a3c(r.status==="ok"&&r.selectedId==="b","single ID");}
 if(index===3){r=j3c(ext.SelectMultipleFromList(opts+',"selectedIds":["b","a"]}'));a3c(r.status==="ok"&&r.selectedIds.join(',')==="a,b","multiple IDs/order");}
 if(index===4){r=j3c(ext.SelectMultipleFromList(opts+'}'));a3c(r.status==="ok"&&r.selectedIds.length===0,"accepted empty set");}
 if(index===5){r=j3c(ext.SelectFromList(opts+'}'));a3c(r.status==="cancelled","list cancelled");}
 if(index===6||index===7){r=j3c(ext.ChooseColor("#12aBeF"));a3c(index===6?r.status==="ok"&&r.color==="#12ABEF":r.status==="cancelled","color result");}
 a3c(selection===ext.GetSelectionText()+"|"+ext.GetSelectionHtml(),"dialog preserved selection");return "PASS";
}catch(e){return String(e.message||e);}}
'@
    foreach($name in @('CHILD','ERROR','COMERROR')) {$script=$script.Replace(('@@'+$name+'@@'),((Join-Path $root ($name.ToLowerInvariant()+'.js'))|ConvertTo-Json -Compress))}
    $script | Set-Content (Join-Path $root 'stage3c.js') -Encoding utf8
    'function Run(){var e=window.external;var bad=false;try{e.PopStatusBarText(window.tokenA);}catch(error){bad=true;}window.childChecked=bad;e.PushStatusBarText("child");e.ClearStatusBarText();e.PushStatusBarText("child-leak");}' | Set-Content (Join-Path $root 'child.js') -Encoding utf8
    'function Run(){window.external.PushStatusBarText("exception-leak");throw new Error("intentional stage3c failure");}' | Set-Content (Join-Path $root 'error.js') -Encoding utf8
    'function Run(){var e=window.external;e.PushStatusBarText("COM-leak");e.PopStatusBarText("foreign");}' | Set-Content (Join-Path $root 'comerror.js') -Encoding utf8
    $fixture=Join-Path $iso.Root 'api3c.fb2';$report=Join-Path $iso.Root 'api3c.txt'
    '<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>API3C</book-title><lang>en</lang></title-info><document-info><id>api3c</id><version>1</version></document-info></description><body><section><p>ordinary paragraph</p></section></body></FictionBook>' | Set-Content $fixture -Encoding utf8
    $env:FBE_NEXT_TEST_MODE='1';$env:FBE_NEXT_TEST_SCENARIO="scripting-stage3c-$Part";$env:FBE_NEXT_SCRIPT_API_ROOT=$root
    $env:FBE_NEXT_TEST_SETTINGS_DIRECTORY=Join-Path $iso.DataPath 'Settings';$env:FBE_NEXT_TRACE='0'
    $process=Start-Process $iso.Exe -WorkingDirectory $iso.Root -ArgumentList @("--$Mode",'-b',$report,$fixture) -WindowStyle Hidden -PassThru
    $code=Wait-IsolatedFbeProcess -Process $process -Isolation $iso -Scenario "stage3c-$Part" -Report $report -TimeoutSeconds 45
    $text=if(Test-Path $report){Get-Content $report -Raw}else{'missing report'}
    if($code -ne 0 -or $text -notmatch 'result=PASS'){throw "Stage3C $Part failed: $text"}
    if($Part -in @('clipboard','clipboard-ole')) {
        foreach($log in @(Get-ChildItem -LiteralPath $iso.DataPath -Recurse -File -Filter '*.log')) {
            if((Get-Content -LiteralPath $log.FullName -Raw) -match 'FBE45_PRIVATE_CLIPBOARD_CANARY'){throw 'Clipboard payload leaked into diagnostics'}
        }
    }
    Write-Host $text; $passed=$true;Write-Host "Stage3C $Part ($Mode): PASS"
}finally{foreach($name in $names){[Environment]::SetEnvironmentVariable($name,$old[$name])};Complete-IsolatedFbeRuntime -Isolation $iso -Passed $passed}
