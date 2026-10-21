[CmdletBinding()]
param([string]$FbeExe=(Join-Path $PSScriptRoot '../../out/Release/FBE.exe'),
    [ValidateSet('portable','installed')][string]$Mode='portable',
    [ValidateSet('fb2','fbd')][string]$DocumentType='fb2',
    [ValidateSet('normal','zip','rar')][string]$Container='normal',
    [ValidateSet('utf-8','utf-16')][string]$InputEncoding='utf-8')
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$iso=New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'api3d'
$passed=$false
$junction=$null
$names=@('FBE_NEXT_TEST_MODE','FBE_NEXT_TEST_SCENARIO','FBE_NEXT_SCRIPT_API_ROOT','FBE_NEXT_TEST_SETTINGS_DIRECTORY','FBE_NEXT_TRACE')
$old=@{};foreach($name in $names){$old[$name]=[Environment]::GetEnvironmentVariable($name)}
try {
    $null=New-IsolatedFbeSettingsSeed $iso
    $root=Join-Path $iso.Root 'api3d';New-Item -ItemType Directory $root | Out-Null
    $conflict=Join-Path $root "conflict.$DocumentType"
    [IO.File]::WriteAllBytes($conflict,[byte[]](0,255,13,10,42))
    $script=@'
function Run(){}
var ext=window.external,root=@@ROOT@@,suffix=@@TYPE@@,rar=@@RAR@@,normal=@@NORMAL@@;
function check3d(ok,message){if(!ok)throw new Error(message);}
function fails3d(fn){var failed=false;try{fn();}catch(e){failed=true;}check3d(failed,"expected failure");}
function copies3d(){try{
 check3d(ext.GetScriptingApiVersion()==="1.5.0","API version");
 var caps=ext.GetScriptingCapabilities();check3d(caps.indexOf('"documentSave":true')>=0&&caps.indexOf('"documentCopies":true')>=0&&caps.indexOf('"documentSerialization":true')>=0,"capabilities");
 var probe=document.getElementById("probe");if(probe){var range=document.body.createTextRange();range.moveToElementText(probe);range.select();}
 var before=document.body.outerHTML;
 var selected=ext.GetSelectionHtml(),mode=ext.GetCurrentViewMode();
 var xml=ext.SerializeCurrentDocument();check3d(xml.indexOf("FictionBook")>=0,"snapshot root");
 check3d(before===document.body.outerHTML,"snapshot changed visual DOM");
 fails3d(function(){ext.SaveDocumentCopy(root+"\\conflict."+suffix,false);});
 fails3d(function(){ext.SaveSerializedDocumentCopy(root+"\\conflict."+suffix,'{"overwrite":false}');});
 var path=root+"\\copy."+suffix;check3d(ext.SaveDocumentCopy(path,false),"copy");
 fails3d(function(){ext.SaveDocumentCopy(path,false);});
 var lastError=ext.GetLastScriptingError();
 fails3d(function(){ext.SaveSerializedDocumentCopy(path,'{"overwrite":false}');});
 check3d(ext.GetLastScriptingError()!=="","serialized conflict error");
 lastError=ext.GetLastScriptingError();
 check3d(ext.SaveDocumentCopy(path,true),"explicit overwrite");
 check3d(ext.GetLastScriptingError()===lastError,"success cleared previous error");
 fails3d(function(){ext.SaveDocumentCopy(path,1);});
 fails3d(function(){ext.SaveDocumentCopy(path);});
 check3d(ext.SaveSerializedDocumentCopy(root+"\\export."+suffix,'{"encoding":"utf-8"}'),"explicit export");
 fails3d(function(){ext.SaveSerializedDocumentCopy(path,'{"overwrite":true,"overwrite":false}');});
 fails3d(function(){ext.SaveSerializedDocumentCopy(path,'{"unknown":1}');});
 fails3d(function(){ext.SaveDocumentCopy(root+"\\bad.zip",false);});
 fails3d(function(){ext.SaveDocumentCopy("relative."+suffix,false);});
 fails3d(function(){ext.SaveDocumentCopy("C:drive-relative."+suffix,false);});
 fails3d(function(){ext.SaveDocumentCopy("\\root-relative."+suffix,false);});
 fails3d(function(){ext.SaveDocumentCopy(root+"\\missing-parent\\copy."+suffix,false);});
 fails3d(function(){ext.SaveDocumentCopy(root+"\\original-alias."+suffix,true);});
 if(normal)fails3d(function(){ext.SaveDocumentCopy(root+"\\source-dir\\api3d."+suffix,true);});
 fails3d(function(){ext.SaveDocumentCopy(root+"\\..\\api3d\\ORIGINAL-ALIAS."+suffix,true);});
 fails3d(function(){ext.SaveSerializedDocumentCopy(root+"\\original-alias."+suffix,'{"overwrite":true}');});
 var other=suffix==="fb2"?"fbd":"fb2";
 if(suffix==="fbd")fails3d(function(){ext.SaveSerializedDocumentCopy(root+"\\incompatible."+other,'{"format":"'+other+'"}');});
 else check3d(ext.SaveSerializedDocumentCopy(root+"\\converted.fbd",'{"format":"fbd"}'),"lossless format export");
 check3d(selected===ext.GetSelectionHtml()&&mode===ext.GetCurrentViewMode(),"selection/view changed");
 return "PASS";
}catch(e){return String(e.message||e);}}
function blocked3d(){try{var token=ext.BeginDocumentUpdate("blocked save",'{}');try{fails3d(function(){ext.SerializeCurrentDocument();});}finally{ext.EndDocumentUpdate(token);}return "PASS";}catch(e){return String(e.message||e);}}
function blockedUndo3d(){try{ext.BeginUndoUnit(document,"blocked save");try{fails3d(function(){ext.SerializeCurrentDocument();});}finally{ext.EndUndoUnit(document);}return "PASS";}catch(e){return String(e.message||e);}}
function selectState3d(){var controls=document.getElementsByTagName("SELECT"),state="";for(var i=0;i<controls.length;i++)state+=controls[i].id+":"+controls[i].selectedIndex+":"+controls[i].value+";";return state;}
function prepareDescription3d(){try{
 if(!rar){document.getElementById("tiLang").value="ru";document.getElementById("stiLang").value="ru";
 document.getElementById("tiKwd").value="Current keywords";
 var custom=document.getElementById("ci").getElementsByTagName("DIV")[0];custom.all.type.value="test";custom.all.val.value="Current textarea";
 var input=document.getElementById("tiKwd");input.focus();var range=input.createTextRange();range.moveStart("character",2);range.moveEnd("character",-2);range.select();}
 return "PASS";}catch(e){return String(e.message||e);}}
function descriptionXml3d(xml){if(rar)return;var parsed=new ActiveXObject("Msxml2.DOMDocument.6.0");check3d(parsed.loadXML(xml),"Description XML");
 check3d(parsed.selectSingleNode("//*[local-name()='title-info']/*[local-name()='lang']").text==="ru","current tiLang lost");
 check3d(parsed.selectSingleNode("//*[local-name()='src-title-info']/*[local-name()='lang']").text==="ru","current stiLang lost");
 check3d(parsed.selectSingleNode("//*[local-name()='keywords']").text==="Current keywords","current INPUT lost");
 check3d(parsed.selectSingleNode("//*[local-name()='custom-info']").text==="Current textarea","current TEXTAREA lost");
 check3d(parsed.selectSingleNode("//*[local-name()='genre']").text==="prose","genre lost");
 check3d(parsed.selectSingleNode("//*[local-name()='coverpage']/*[local-name()='image']").attributes.getNamedItem("l:href").text==="#cover","cover lost");}
function description3d(){try{
 check3d(ext.GetCurrentViewMode()==="description","Description view");
 // Check once before observing SELECT properties: observing MSHTML controls
 // can materialize attributes and must not hide a stale-markup defect.
 var before=document.body.outerHTML,active=document.activeElement,selection=document.selection.createRange().text;
 descriptionXml3d(ext.SerializeCurrentDocument());
 check3d(before===document.body.outerHTML,"unobserved Description changed");
 var selects=selectState3d();before=document.body.outerHTML;
 descriptionXml3d(ext.SerializeCurrentDocument());
 check3d(ext.SaveDocumentCopy(root+"\\description."+suffix,false),"Description copy");
 check3d(selects===selectState3d()&&before===document.body.outerHTML&&active===document.activeElement&&selection===document.selection.createRange().text&&ext.GetCurrentViewMode()==="description","Description changed");return "PASS";
}catch(e){return String(e.message||e);}}
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
 if(rar)fails3d(function(){ext.SaveCurrentDocument();});else check3d(ext.SaveCurrentDocument(),"current save");
 check3d(ext.SaveCurrentDocumentAs(root+"\\saveas."+suffix,false),"save as");
 fails3d(function(){ext.SaveCurrentDocumentAs(root+"\\saveas."+suffix,false);});
 fails3d(function(){ext.SaveDocumentCopy(root+"\\saveas."+suffix,true);});
 check3d(ext.SaveCurrentDocument(),"save after save as");return "PASS";
}catch(e){return String(e.message||e);}}
function saveFailure3d(){try{fails3d(function(){ext.SaveCurrentDocument();});return "PASS";}catch(e){return String(e.message||e);}}
function replaceFailure3d(){try{fails3d(function(){ext.SaveDocumentCopy(root+"\\locked-target."+suffix,true);});return "PASS";}catch(e){return String(e.message||e);}}
function snapshotFailure3d(){try{fails3d(function(){ext.SaveDocumentCopy(root+"\\invalid-table."+suffix,false);});check3d(!ext.FileExists(root+"\\invalid-table."+suffix),"failed serializer wrote file");return "PASS";}catch(e){return String(e.message||e);}}
function large3d(){try{fails3d(function(){ext.SerializeCurrentDocument();});check3d(ext.SaveSerializedDocumentCopy(root+"\\large.fb2",'{"encoding":"utf-8"}'),"large file export");return "PASS";}catch(e){return String(e.message||e);}}
'@
    $script=$script.Replace('@@ROOT@@',($root|ConvertTo-Json -Compress)).Replace('@@TYPE@@',($DocumentType|ConvertTo-Json -Compress)).Replace('@@RAR@@',(($Container -eq 'rar')|ConvertTo-Json -Compress)).Replace('@@NORMAL@@',(($Container -eq 'normal')|ConvertTo-Json -Compress))
    $script | Set-Content (Join-Path $root 'stage3d.js') -Encoding utf8
    $fixture=Join-Path $iso.Root "api3d.$DocumentType";$report=Join-Path $iso.Root 'api3d.txt'
    $png='iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAwMCAO+a3ioAAAAASUVORK5CYII='
    $body=if($DocumentType -eq 'fb2'){'<body><section><p>ordinary paragraph Кириллица 😀 <a l:href="#n" type="note">1</a></p><p id="probe">editable paragraph</p><p><image l:href="#cover"/></p><table><tr><td colspan="2">Cell A</td></tr><tr><td>Cell B</td><td>Cell C</td></tr><tr><td rowspan="2">Cell D</td><td>Cell E</td></tr><tr><td>Cell F</td></tr></table></section></body><body name="notes"><section id="n"><p>note paragraph</p></section></body>'}else{''}
    ('<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0" xmlns:l="http://www.w3.org/1999/xlink"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>API3D</book-title><coverpage><image l:href="#cover"/></coverpage><lang>en</lang></title-info><src-title-info><genre>prose</genre><author><first-name>S</first-name><last-name>T</last-name></author><book-title>Original source title</book-title><lang>en</lang></src-title-info><document-info><id>api3d</id><version>1</version></document-info></description>'+$body+'<binary id="cover" content-type="image/png">'+$png+'</binary></FictionBook>') | Set-Content $fixture -Encoding utf8
    $inputPath=$fixture
    if($InputEncoding -eq 'utf-16') {
        $fixtureText=(Get-Content $fixture -Raw).Replace('encoding="utf-8"','encoding="utf-16"')
        [IO.File]::WriteAllText($fixture,$fixtureText,[Text.UnicodeEncoding]::new($false,$true))
    }
    $junction=Join-Path $root 'source-dir'
    if($Container -eq 'normal') {New-Item -ItemType Junction -Path $junction -Target $iso.Root | Out-Null}
    if($Container -eq 'zip') {
        Add-Type -AssemblyName System.IO.Compression
        $inputPath=Join-Path $iso.Root 'book.zip'
        $stream=[IO.File]::Create($inputPath);$zip=[IO.Compression.ZipArchive]::new($stream,[IO.Compression.ZipArchiveMode]::Create)
        try {foreach($entryName in @("book.$DocumentType",'untouched.txt')) {$entry=$zip.CreateEntry($entryName);$writer=[IO.StreamWriter]::new($entry.Open(),[Text.UTF8Encoding]::new($false));try{$writer.Write($(if($entryName -eq 'untouched.txt'){'ZIP sentinel'}else{Get-Content $fixture -Raw}))}finally{$writer.Dispose()}}}finally{$zip.Dispose();$stream.Dispose()}
    } elseif($Container -eq 'rar') {
        if($DocumentType -ne 'fb2'){throw 'The bounded RAR fixture contains FB2'}
        $inputPath=Join-Path $iso.Root 'book.rar'
        [IO.File]::WriteAllBytes($inputPath,[Convert]::FromBase64String((Get-Content (Join-Path $PSScriptRoot 'fixtures/archive-runtime-rar5.b64') -Raw).Trim()))
        $rarHash=(Get-FileHash $inputPath -Algorithm SHA256).Hash
    }
    $env:FBE_NEXT_TEST_MODE='1';$env:FBE_NEXT_TEST_SCENARIO='scripting-stage3d';$env:FBE_NEXT_SCRIPT_API_ROOT=$root
    $env:FBE_NEXT_TEST_SETTINGS_DIRECTORY=Join-Path $iso.DataPath 'Settings';$env:FBE_NEXT_TRACE='1'
    $process=Start-Process $iso.Exe -WorkingDirectory $iso.Root -ArgumentList @("--$Mode",'-b',$report,$inputPath) -WindowStyle Hidden -PassThru
    $code=Wait-IsolatedFbeProcess -Process $process -Isolation $iso -Scenario 'stage3d' -Report $report -TimeoutSeconds 60
    $text=if(Test-Path $report){Get-Content $report -Raw}else{'missing report'}
    if($code -ne 0 -or $text -notmatch 'result=PASS'){throw "Stage3D failed (exit=$code): $text"}
    if(-not [Linq.Enumerable]::SequenceEqual([byte[]](0,255,13,10,42),[IO.File]::ReadAllBytes($conflict))){throw 'Conflict changed existing destination bytes'}
    if(@(Get-ChildItem -LiteralPath $root -Filter 'fbs*.tmp').Count){throw 'Unused snapshot temporary file remains'}
    [xml]$copy=Get-Content (Join-Path $root "copy.$DocumentType") -Raw
    [xml]$source=Get-Content (Join-Path $root "source.$DocumentType") -Raw
    if($Container -eq 'normal') {
        if($copy.FirstChild.Value -notmatch ('encoding="'+$InputEncoding+'"')) {throw 'Current encoding changed in copy'}
        [xml]$export=Get-Content (Join-Path $root "export.$DocumentType") -Raw
        if($export.FirstChild.Value -notmatch 'encoding="utf-8"') {throw 'Explicit UTF-8 was not written'}
    }
    if($Container -ne 'rar') {
        if($copy.DocumentElement.InnerText -notmatch 'API3D Edited' -or $copy.DocumentElement.InnerText -notmatch $(if($DocumentType -eq 'fb2'){'Cell A'}else{'API3D'})){throw 'Copy content mismatch'}
        if($DocumentType -eq 'fb2' -and $copy.DocumentElement.InnerText -notmatch 'UNSAVED_BODY'){throw 'Unsaved body was not copied'}
        if($DocumentType -eq 'fb2' -and (-not $copy.SelectSingleNode('//*[local-name()="td"][@rowspan="2"]') -or
            -not $copy.SelectSingleNode('//*[local-name()="td"][@colspan="2"]') -or $copy.DocumentElement.InnerText -notmatch 'Cell F')) {throw 'Table spans/content changed'}
        $binary=$copy.SelectSingleNode('//*[local-name()="binary"]')
        if(-not $binary -or -not [Linq.Enumerable]::SequenceEqual([Convert]::FromBase64String($png),[Convert]::FromBase64String($binary.InnerText))){throw 'Binary bytes changed'}
        if($DocumentType -eq 'fb2') {
            [xml]$converted=Get-Content (Join-Path $root 'converted.fbd') -Raw
            if($converted.SelectNodes('//*[local-name()="body"]').Count -ne 2 -or $converted.DocumentElement.InnerText -notmatch 'Cell C'){throw 'Format export dropped content'}
        }
    }
    if($Container -ne 'rar') {
        [xml]$description=Get-Content (Join-Path $root "description.$DocumentType") -Raw
        foreach($tag in 'title-info','src-title-info') {
            if($description.SelectSingleNode("//*[local-name()='$tag']/*[local-name()='lang']").InnerText -cne 'ru'){throw "Description copy lost $tag language"}
        }
        if($description.SelectSingleNode('//*[local-name()="custom-info"]').InnerText -cne 'Current textarea'){throw 'Description copy lost TEXTAREA'}
    }
    if($source.DocumentElement.InnerText -notmatch 'SourceCurrent'){throw 'Source copy mismatch'}
    if($DocumentType -eq 'fb2') {
        $settings=[Xml.XmlReaderSettings]::new();$settings.ValidationType=[Xml.ValidationType]::Schema
        $settings.Schemas.XmlResolver=[Xml.XmlUrlResolver]::new()
        $null=$settings.Schemas.Add('http://www.gribuser.ru/xml/fictionbook/2.0',(Join-Path $iso.Root 'FictionBook.xsd'));$settings.Schemas.Compile()
        foreach($name in 'copy.fb2','export.fb2','source.fb2','description.fb2','saveas.fb2','large.fb2') {
            $candidate=Join-Path $root $name
            if(Test-Path $candidate) {$reader=[Xml.XmlReader]::Create($candidate,$settings);try{while($reader.Read()){}}finally{$reader.Close()}}
        }
    }
    if($Container -eq 'rar' -and (Get-FileHash $inputPath -Algorithm SHA256).Hash -ne $rarHash){throw 'RAR was modified'}
    if($Container -eq 'zip') {
        $zip=[IO.Compression.ZipFile]::OpenRead($inputPath)
        try{
            $reader=[IO.StreamReader]::new($zip.GetEntry('untouched.txt').Open());try{if($reader.ReadToEnd() -cne 'ZIP sentinel'){throw 'Other ZIP entry changed'}}finally{$reader.Dispose()}
            $reader=[IO.StreamReader]::new($zip.GetEntry("book.$DocumentType").Open());try{if($reader.ReadToEnd() -notmatch 'API3D Edited'){throw 'ZIP document entry was not updated'}}finally{$reader.Dispose()}
        }finally{$zip.Dispose()}
    }
    foreach($log in @(Get-ChildItem -LiteralPath $iso.DataPath -Recurse -File -Filter '*.log')) {
        if((Get-Content -LiteralPath $log.FullName -Raw) -match ('UNSAVED_BODY|SourceCurrent|'+[regex]::Escape($png))){throw 'Book payload leaked into diagnostics'}
    }
    Write-Host $text;$passed=$true;Write-Host "Stage3D $Mode/$DocumentType/$Container/$InputEncoding PASS"
}finally{
    if($junction -and (Test-Path -LiteralPath $junction)) {Remove-Item -LiteralPath $junction -Force}
    foreach($name in $names){[Environment]::SetEnvironmentVariable($name,$old[$name])}
    Complete-IsolatedFbeRuntime -Isolation $iso -Passed $passed
}
