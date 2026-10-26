[CmdletBinding()]
param([Parameter(Mandatory)][string]$FbeExe,[string]$OutputDirectory=(Join-Path $PSScriptRoot '../../out/tests/binary-import-decoder'))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$out=[IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Force $out|Out-Null
$iso=New-IsolatedFbeRuntime -FbeExe $FbeExe -Name binary-import-decoder
$passed=$false
$names=@('FBE_NEXT_TEST_MODE','FBE_NEXT_TEST_SCENARIO','FBE_NEXT_SCRIPT_API_ROOT','FBE_NEXT_TEST_SETTINGS_DIRECTORY')
$old=@{};foreach($name in $names){$old[$name]=[Environment]::GetEnvironmentVariable($name,'Process')}
try {
 $null=New-IsolatedFbeSettingsSeed $iso
 $fixture='<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>Decoder</book-title><lang>en</lang></title-info><document-info><id>decoder</id><version>1</version></document-info></description><body><section><p>Decoder test</p></section></body><binary id="one" content-type="application/octet-stream">TQ==</binary></FictionBook>'
 [IO.File]::WriteAllText((Join-Path $iso.Root 'fixture.fb2'),$fixture,[Text.UTF8Encoding]::new($false))
 # Run the actual main-window function; intercept only its external UI/store sinks.
 # Independent byte oracles use VBArray, not the product encoder/decoder implementation.
 $probe=@'
var _decoderResult="NOT RUN";
(function(){
 var oldReset=BinaryStoreChanged,oldAdd=apiAddBinary,oldLists=FillLists,oldMessage=MsgBox;
 var received=[],messages=[];
 function assert(ok,label){if(!ok)throw new Error(label);}
 function check(content,expected,messageCount){
  var dom=new ActiveXObject("Msxml2.DOMDocument.6.0");dom.async=false;dom.preserveWhiteSpace=true;
  assert(dom.loadXML('<FictionBook xmlns="'+fbNS+'">'+content+'</FictionBook>'),"fixture XML");
  dom.setProperty("SelectionNamespaces","xmlns:fb='"+fbNS+"'");
  var before=String(dom.xml);received=[];messages=[];
  PutBinaries(dom);
  assert(String(dom.xml)===before,"input XML/type attributes changed");
  assert(received.length===expected.length,"accepted count");
  assert(messages.length===messageCount,"error reporting count");
  for(var i=0;i<expected.length;i++){
   assert(received[i].id===expected[i][0],"binary id");
   assert(received[i].mime==="application/octet-stream","binary mime");
   assert(received[i].bytes.join(',')===expected[i][1],"decoded bytes");
  }
 }
 function binary(id,text){return '<binary id="'+id+'" content-type="application/octet-stream">'+text+'</binary>';}
 try {
  BinaryStoreChanged=function(){};FillLists=function(){};MsgBox=function(text){messages.push(String(text));};
  apiAddBinary=function(path,id,mime,value){received.push({id:String(id),mime:String(mime),bytes:value==null?[]:(new VBArray(value)).toArray()});};
  check(binary("one","TQ==")+binary("white"," T W E=\r\n"),[["one","77"],["white","77,97"]],0);
  check(binary("split","T<![CDATA[W]]><!-- split -->E="),[["split","77,97"]],0);
  check(binary("empty",""),[["empty",""]],0);
  check(binary("bad","%%%INVALID%%")+binary("after","AAEC/w=="),[["after","0,1,2,255"]],1);
  check(binary("bad1","!")+binary("bad2","!")+binary("bad3","!")+binary("bad4","!")+binary("bad5","!"),[],4);
  _decoderResult="PASS";
 }catch(error){_decoderResult="FAIL: "+String(error.message||error);}
 finally {BinaryStoreChanged=oldReset;apiAddBinary=oldAdd;FillLists=oldLists;MsgBox=oldMessage;}
 document.documentElement.setAttribute("data-decoder-regression",_decoderResult);
})();
'@
 $literal=$probe|ConvertTo-Json -Compress
 $script='var compatResult="NOT RUN";function CompatResult(){return compatResult;}function Run(){try{document.parentWindow.execScript('+$literal+',"JScript");compatResult=String(document.documentElement.getAttribute("data-decoder-regression"));document.documentElement.removeAttribute("data-decoder-regression");}catch(error){compatResult="FAIL: "+String(error.message||error);}}'
 [IO.File]::WriteAllText((Join-Path $iso.Root 'decoder.js'),$script,[Text.UTF8Encoding]::new($false))
 [IO.File]::WriteAllText((Join-Path $iso.Root 'suite.xml'),'<suite><step name="decoder-valid-empty-invalid-input-unchanged" script="decoder.js"/></suite>',[Text.UTF8Encoding]::new($false))
 $env:FBE_NEXT_TEST_MODE='1';$env:FBE_NEXT_TEST_SCENARIO='binary-scripting-compatibility';$env:FBE_NEXT_SCRIPT_API_ROOT=$iso.Root
 $env:FBE_NEXT_TEST_SETTINGS_DIRECTORY=Join-Path $iso.DataPath 'Settings'
 $report=Join-Path $iso.Root 'report.txt'
 $process=Start-Process $iso.Exe -WorkingDirectory $iso.Root -ArgumentList @('--portable','-b',$report,(Join-Path $iso.Root 'fixture.fb2')) -WindowStyle Hidden -PassThru
 $code=Wait-IsolatedFbeProcess -Process $process -Isolation $iso -Scenario binary-scripting-compatibility -Report $report -TimeoutSeconds 180
 if(Test-Path $report){Copy-Item $report (Join-Path $out 'report.txt') -Force}
 if($code -ne 0 -or -not(Test-Path $report) -or (Get-Content $report -Raw) -notmatch 'result=PASS'){throw 'Binary import decoder regression failed; see isolated report'}
 @{status='PASS';groups=5;mainJsSha256=(Get-FileHash (Join-Path $iso.Root 'main.js')).Hash;exeSha256=(Get-FileHash $iso.Exe).Hash}|ConvertTo-Json|Set-Content (Join-Path $out 'summary.json') -Encoding utf8
 $passed=$true;Write-Host 'Binary import decoder: valid/whitespace/CDATA/comments, empty, malformed+continue, error aggregation and unchanged input XML PASS'
}finally{foreach($name in $names){[Environment]::SetEnvironmentVariable($name,$old[$name],'Process')};Complete-IsolatedFbeRuntime -Isolation $iso -Passed $passed}
