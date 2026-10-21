[CmdletBinding()]
param([string]$FbeExe=(Join-Path $PSScriptRoot '../../out/Release/FBE.exe'),
    [ValidateSet('portable','installed')][string]$Mode='portable',
    [ValidateSet('fb2','fbd')][string]$DocumentType='fb2')
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$iso=New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'api3d'
$passed=$false
$names=@('FBE_NEXT_TEST_MODE','FBE_NEXT_TEST_SCENARIO','FBE_NEXT_SCRIPT_API_ROOT','FBE_NEXT_TEST_SETTINGS_DIRECTORY','FBE_NEXT_TRACE')
$old=@{};foreach($name in $names){$old[$name]=[Environment]::GetEnvironmentVariable($name)}
try {
    $null=New-IsolatedFbeSettingsSeed $iso
    $root=Join-Path $iso.Root 'api3d';New-Item -ItemType Directory $root | Out-Null
    $script=@'
function Run(){}
var ext=window.external,root=@@ROOT@@,suffix=@@TYPE@@;
function check3d(ok,message){if(!ok)throw new Error(message);}
function fails3d(fn){var failed=false;try{fn();}catch(e){failed=true;}check3d(failed,"expected failure");}
function copies3d(){try{
 var before=document.body.outerHTML;
 var xml=ext.SerializeCurrentDocument();check3d(xml.indexOf("API3D")>=0,"snapshot title");
 check3d(before===document.body.outerHTML,"snapshot changed visual DOM");
 var path=root+"\\copy."+suffix;check3d(ext.SaveDocumentCopy(path,false),"copy");
 fails3d(function(){ext.SaveDocumentCopy(path,false);});
 fails3d(function(){ext.SaveDocumentCopy(path,1);});
 fails3d(function(){ext.SaveDocumentCopy(path);});
 check3d(ext.SaveSerializedDocumentCopy(root+"\\export."+suffix,'{"encoding":"utf-8"}'),"explicit export");
 fails3d(function(){ext.SaveSerializedDocumentCopy(path,'{"overwrite":true,"overwrite":false}');});
 fails3d(function(){ext.SaveSerializedDocumentCopy(path,'{"unknown":1}');});
 fails3d(function(){ext.SaveDocumentCopy(root+"\\bad.zip",false);});
 return "PASS";
}catch(e){return String(e.message||e);}}
function blocked3d(){try{var token=ext.BeginDocumentUpdate("blocked save",'{}');try{fails3d(function(){ext.SerializeCurrentDocument();});}finally{ext.EndDocumentUpdate(token);}return "PASS";}catch(e){return String(e.message||e);}}
function source3d(){try{
 var xml=ext.SerializeCurrentDocument();check3d(xml.indexOf("SourceCurrent")>=0&&xml.indexOf("API3D")<0,"stale Body export");
 check3d(ext.SaveDocumentCopy(root+"\\source."+suffix,false),"Source copy");
 fails3d(function(){ext.SaveCurrentDocument();});fails3d(function(){ext.SaveCurrentDocumentAs(root+"\\source-save."+suffix,false);});
 check3d(!ext.FileExists(root+"\\source-save."+suffix),"Source Save As created file");return "PASS";
}catch(e){return String(e.message||e);}}
function invalidSource3d(){try{
 fails3d(function(){ext.SerializeCurrentDocument();});fails3d(function(){ext.SaveDocumentCopy(root+"\\invalid."+suffix,false);});
 check3d(!ext.FileExists(root+"\\invalid."+suffix),"invalid Source created a file");return "PASS";
}catch(e){return String(e.message||e);}}
function saves3d(){try{
 check3d(ext.SaveCurrentDocument(),"current save");
 check3d(ext.SaveCurrentDocumentAs(root+"\\saveas."+suffix,false),"save as");
 fails3d(function(){ext.SaveCurrentDocumentAs(root+"\\saveas."+suffix,false);});
 fails3d(function(){ext.SaveDocumentCopy(root+"\\saveas."+suffix,true);});
 check3d(ext.SaveCurrentDocument(),"save after save as");return "PASS";
}catch(e){return String(e.message||e);}}
function saveFailure3d(){try{fails3d(function(){ext.SaveCurrentDocument();});return "PASS";}catch(e){return String(e.message||e);}}
'@
    $script=$script.Replace('@@ROOT@@',($root|ConvertTo-Json -Compress)).Replace('@@TYPE@@',($DocumentType|ConvertTo-Json -Compress))
    $script | Set-Content (Join-Path $root 'stage3d.js') -Encoding utf8
    $fixture=Join-Path $iso.Root "api3d.$DocumentType";$report=Join-Path $iso.Root 'api3d.txt'
    $body=if($DocumentType -eq 'fb2'){'<body><section><p>ordinary paragraph</p></section></body><body name="notes"><section id="n"><p>note paragraph</p></section></body>'}else{''}
    ('<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>API3D</book-title><lang>en</lang></title-info><document-info><id>api3d</id><version>1</version></document-info></description>'+$body+'</FictionBook>') | Set-Content $fixture -Encoding utf8
    $env:FBE_NEXT_TEST_MODE='1';$env:FBE_NEXT_TEST_SCENARIO='scripting-stage3d';$env:FBE_NEXT_SCRIPT_API_ROOT=$root
    $env:FBE_NEXT_TEST_SETTINGS_DIRECTORY=Join-Path $iso.DataPath 'Settings';$env:FBE_NEXT_TRACE='0'
    $process=Start-Process $iso.Exe -WorkingDirectory $iso.Root -ArgumentList @("--$Mode",'-b',$report,$fixture) -WindowStyle Hidden -PassThru
    $code=Wait-IsolatedFbeProcess -Process $process -Isolation $iso -Scenario 'stage3d' -Report $report -TimeoutSeconds 45
    $text=if(Test-Path $report){Get-Content $report -Raw}else{'missing report'}
    if($code -ne 0 -or $text -notmatch 'result=PASS'){throw "Stage3D failed: $text"}
    [xml]$copy=Get-Content (Join-Path $root "copy.$DocumentType") -Raw
    [xml]$source=Get-Content (Join-Path $root "source.$DocumentType") -Raw
    if($copy.DocumentElement.InnerText -notmatch 'API3D' -or $source.DocumentElement.InnerText -notmatch 'SourceCurrent'){throw 'Copy content mismatch'}
    Write-Host $text;$passed=$true;Write-Host "Stage3D $Mode/$DocumentType PASS"
}finally{foreach($name in $names){[Environment]::SetEnvironmentVariable($name,$old[$name])};Complete-IsolatedFbeRuntime -Isolation $iso -Passed $passed}
