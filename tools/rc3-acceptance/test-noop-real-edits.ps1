[CmdletBinding()]
param([Parameter(Mandatory)][string]$FbeExe, [int]$TimeoutSeconds = 120, [ValidateSet('all','body-replacement')][string]$Case = 'all')
$ErrorActionPreference = 'Stop'
if ($env:GITHUB_ACTIONS -ne 'true' -or $env:RUNNER_ENVIRONMENT -ne 'github-hosted') {
    throw 'This diagnostic is restricted to a disposable GitHub-hosted runner.'
}
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
. (Join-Path $repoRoot 'tools/tests/RuntimeTestIsolation.ps1')
. (Join-Path $PSScriptRoot 'NoopClassification.ps1')
$evidenceRoot = Join-Path $repoRoot 'out/tests/rc3-noop-real-edits'
New-Item -ItemType Directory -Force -Path $evidenceRoot | Out-Null
$names = @('FBE_NEXT_TEST_MODE','FBE_NEXT_TEST_SCENARIO','FBE_NEXT_SCRIPT_API_ROOT','FBE_NEXT_TEST_SETTINGS_DIRECTORY')
$old = @{}; foreach ($name in $names) { $old[$name] = [Environment]::GetEnvironmentVariable($name, 'Process') }
$results = [Collections.Generic.List[object]]::new()
try {
    $scenarios=if($Case -eq 'body-replacement'){@('body-replacement')}else{@('body','description','binary','body-replacement')}
    foreach ($scenario in $scenarios) {
        $iso = New-IsolatedFbeRuntime -FbeExe $FbeExe -Name ('rc3-noop-' + $scenario)
        $passed = $false; $details = ''; $processCode = $null; $noopDirty = $null
        $scenarioEvidence = Join-Path $evidenceRoot $scenario
        New-Item -ItemType Directory -Force -Path $scenarioEvidence | Out-Null
        try {
            $null = New-IsolatedFbeSettingsSeed $iso -StartupScriptsDirectory (Join-Path $iso.Root 'ProbeScripts')
            $fixture = Join-Path $iso.Root 'fixture.fb2'
            $xml = '<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0" xmlns:l="http://www.w3.org/1999/xlink"><description><title-info><genre>prose</genre><author><nickname>Tester</nickname></author><book-title>Original title</book-title><lang>en</lang></title-info><document-info><author><nickname>Tester</nickname></author><date value="2026-01-01">2026-01-01</date><id>rc3-noop-real-edits</id><version>1.0</version></document-info></description><body><section><p>Original body</p><p><a l:href="#payload.bin">binary reference</a></p></section></body><binary id="payload.bin" content-type="application/octet-stream">AAECA/7/</binary></FictionBook>'
            [IO.File]::WriteAllText($fixture, $xml, [Text.UTF8Encoding]::new($false))
            [IO.File]::WriteAllBytes((Join-Path $iso.Root 'original.bin'), [byte[]](0,1,2,3,254,255))
            [IO.File]::WriteAllBytes((Join-Path $iso.Root 'first.bin'), [byte[]](1,3,5,7))
            [IO.File]::WriteAllBytes((Join-Path $iso.Root 'second.bin'), [byte[]](9,8,7,6,5))
            [IO.File]::WriteAllText((Join-Path $iso.Root 'noop.js'), 'function Run(){}', [Text.UTF8Encoding]::new($false))
            $lookup = switch ($scenario) {
                {$_ -in 'body','body-replacement'} { 'function value(){var p=document.getElementById("fbw_body").getElementsByTagName("P");for(var i=0;i<p.length;i++)if(String(p[i].innerText).indexOf("body")>=0)return String(p[i].innerText);throw new Error("body paragraph missing");}' }
                'description' { 'function value(){return String(document.getElementById("tiTitle").value);}' }
                default { 'function value(){var n=new ActiveXObject("Msxml2.DOMDocument.6.0");n.loadXML("<data/>");n.documentElement.dataType="bin.base64";n.documentElement.nodeTypedValue=GetImageData("payload.bin");return String(n.documentElement.text).replace(/\s/g,"");}' }
            }
            $edit = switch ($scenario) {
                {$_ -in 'body','body-replacement'} { 'function edit(which){var p=document.getElementById("fbw_body").getElementsByTagName("P");for(var i=0;i<p.length;i++)if(String(p[i].innerText).indexOf("body")>=0){e.BeginUndoUnit(document,"RC3 body edit");try{p[i].innerText=which===1?"First body":"Second body";}finally{e.EndUndoUnit(document);}return;}throw new Error("body paragraph missing");}' }
                'description' { 'function edit(which){var f=document.getElementById("tiTitle"),token=e.BeginDocumentUpdate("RC3 description edit","{}");try{f.focus();var r=f.createTextRange();r.select();r.text=which===1?"First title":"Second title";}finally{e.EndDocumentUpdate(token);}}' }
                default { 'function edit(which){e.ReplaceBinaryFromFile("payload.bin",which===1?@@FIRST@@:@@SECOND@@,"application/octet-stream");}' }
            }
            $edit = $edit.Replace('@@FIRST@@', ((Join-Path $iso.Root 'first.bin') | ConvertTo-Json -Compress)).Replace('@@SECOND@@', ((Join-Path $iso.Root 'second.bin') | ConvertTo-Json -Compress))
            $expectedFirst = switch ($scenario) { {$_ -in 'body','body-replacement'} { 'First body' } 'description' { 'First title' } default { 'AQMFBw==' } }
            $expectedSecond = switch ($scenario) { {$_ -in 'body','body-replacement'} { 'Second body' } 'description' { 'Second title' } default { 'CQgHBgU=' } }
            # The child edits at top level, during script loading, after its own
            # save has cleared the loader's existing dirty indication.
            $child = @'
window.rc3TopLevelResult="NOT RUN";
try {
 a(e.SaveCurrentDocument(),"top-level checkpoint save");
 var childClean=e.IsDocumentModified();mark("top_level_checkpoint_dirty",childClean?1:0);a(!childClean,"top-level checkpoint not clean");
 @@SECOND_EDIT@@
 var childDirty=e.IsDocumentModified();mark("top_level_edit_dirty",childDirty?1:0);a(childDirty,"top-level edit reported clean");
 window.rc3TopLevelResult="PASS";
} catch(error) { window.rc3TopLevelResult="FAIL "+String(error.message||error.description||error); }
function Run(){}
'@
            $secondEdit=if($scenario -eq 'body-replacement'){'var original=e.GetSourceText(),replacement=original.replace("First body","Second body");a(replacement!==original,"replacement marker missing");a(e.ApplySourceText(replacement,"RC3 nested replacement"),"nested replacement failed");'}else{'edit(2);'}
            $child=$child.Replace('@@SECOND_EDIT@@',$secondEdit)
            [IO.File]::WriteAllText((Join-Path $iso.Root 'top-level.js'), $child, [Text.UTF8Encoding]::new($false))
            $main = @'
var e=window.external, result="NOT RUN", trace=[];
@@LOOKUP@@
@@EDIT@@
function a(ok,label){if(!ok)throw new Error(label);}
function mark(k,v){trace.push(k+"="+String(v));}
function Run(){try{
 a(e.SaveCurrentDocument(),"checkpoint save");
 var before=e.IsDocumentModified();mark("checkpoint_dirty",before?1:0);a(!before,"checkpoint not clean");
 apiRunCmd(@@NOOP@@);var after=e.IsDocumentModified();mark("noop_dirty",after?1:0);
 edit(1);mark("first_dirty",e.IsDocumentModified()?1:0);a(e.IsDocumentModified(),"first edit reported clean");a(value()===@@EXPECTED1@@,"first edit content");
 a(e.SaveCurrentDocument(),"first save");mark("first_save_dirty",e.IsDocumentModified()?1:0);a(!e.IsDocumentModified(),"first save did not become clean");
 apiRunCmd(@@CHILD@@);a(window.rc3TopLevelResult==="PASS",String(window.rc3TopLevelResult));
 mark("second_dirty",e.IsDocumentModified()?1:0);a(e.IsDocumentModified(),"second edit reported clean");a(value()===@@EXPECTED2@@,"second edit content");
 result="PASS";
 }catch(error){result="FAIL "+String(error.message||error.description||error);}
 finally{mark("script_result",result);try{e.WriteTextFile(@@TRACE@@,trace.join("\r\n"),"utf-8");}catch(ignore){}}
}
function CompatResult(){return result;}
'@
            $main = $main.Replace('@@LOOKUP@@',$lookup).Replace('@@EDIT@@',$edit).Replace('@@NOOP@@',((Join-Path $iso.Root 'noop.js') | ConvertTo-Json -Compress)).Replace('@@CHILD@@',((Join-Path $iso.Root 'top-level.js') | ConvertTo-Json -Compress)).Replace('@@TRACE@@',((Join-Path $iso.Root 'trace.txt') | ConvertTo-Json -Compress)).Replace('@@EXPECTED1@@',($expectedFirst | ConvertTo-Json -Compress)).Replace('@@EXPECTED2@@',($expectedSecond | ConvertTo-Json -Compress))
            [IO.File]::WriteAllText((Join-Path $iso.Root 'edit.js'), $main, [Text.UTF8Encoding]::new($false))
            foreach ($check in @(@('undo.js',$expectedFirst),@('redo.js',$expectedSecond),@('reopen.js',$expectedSecond))) {
                $code = $lookup + "`r`nfunction Run(){} function CompatResult(){try{return value()===" + ($check[1] | ConvertTo-Json -Compress) + '?"PASS":"FAIL unexpected content: "+value();}catch(e){return "FAIL "+String(e.message||e);}}'
                [IO.File]::WriteAllText((Join-Path $iso.Root $check[0]), $code, [Text.UTF8Encoding]::new($false))
            }
            $recoveryScript = 'var e=window.external;' + $lookup + $edit + "`r`nfunction Run(){if(!e.SaveCurrentDocument())throw new Error('recovery checkpoint save');if(e.IsDocumentModified())throw new Error('recovery checkpoint dirty');edit(1);if(!e.IsDocumentModified())throw new Error('recovery edit clean');} function CompatResult(){try{return value()===" + ($expectedFirst | ConvertTo-Json -Compress) + '?"PASS":"FAIL recovery value";}catch(e){return "FAIL "+String(e.message||e);}}'
            [IO.File]::WriteAllText((Join-Path $iso.Root 'recovery.js'), $recoveryScript, [Text.UTF8Encoding]::new($false))
            $binaryFirst = if ($scenario -eq 'binary') { ' id="payload.bin" expected="first.bin"' } else { ' id="payload.bin" expected="original.bin"' }
            $binarySecond = if ($scenario -eq 'binary') { ' id="payload.bin" expected="second.bin"' } else { ' id="payload.bin" expected="original.bin"' }
            $suite = '<suite><step name="edit" script="edit.js"' + $binarySecond + '/><step name="theme-dirty" command="theme-dirty"/><step name="undo" command="undo" script="undo.js"' + $binaryFirst + '/><step name="redo" command="redo" script="redo.js" save="saved.fb2"' + $binarySecond + '/><step name="close" command="close"/><step name="reopen" command="open" open="saved.fb2" script="reopen.js"' + $binarySecond + '/><step name="recovery-edit" script="recovery.js"' + $binaryFirst + '/><step name="recovery" command="recovery"/></suite>'
            [IO.File]::WriteAllText((Join-Path $iso.Root 'suite.xml'), $suite, [Text.UTF8Encoding]::new($false))
            $env:FBE_NEXT_TEST_MODE='1'; $env:FBE_NEXT_TEST_SCENARIO='binary-scripting-compatibility'; $env:FBE_NEXT_SCRIPT_API_ROOT=$iso.Root
            $env:FBE_NEXT_TEST_SETTINGS_DIRECTORY=Join-Path $iso.DataPath 'Settings'
            $report=Join-Path $iso.Root 'report.txt'
            $process=Start-Process -FilePath $iso.Exe -WorkingDirectory $iso.Root -ArgumentList @('--portable','-b',('"'+$report+'"'),('"'+$fixture+'"')) -WindowStyle Hidden -PassThru
            $processCode=Wait-IsolatedFbeProcess -Process $process -Isolation $iso -Scenario ('rc3-noop-'+$scenario) -Report $report -TimeoutSeconds $TimeoutSeconds
            $details=if(Test-Path -LiteralPath $report){Get-Content -LiteralPath $report -Raw}else{'missing report'}
            $tracePath=Join-Path $iso.Root 'trace.txt'
            $trace=if(Test-Path -LiteralPath $tracePath){Get-Content -LiteralPath $tracePath -Raw}else{''}
            if ($trace -match '(?m)^noop_dirty=([01])\r?$') { $noopDirty = $Matches[1] -eq '1' }
            foreach($file in @('report.txt','trace.txt','saved.fb2','recovery.fb2','suite.xml')) {
                $source=Join-Path $iso.Root $file
                if(Test-Path -LiteralPath $source -PathType Leaf){Copy-Item -LiteralPath $source -Destination (Join-Path $scenarioEvidence $file) -Force}
            }
            if($processCode -ne 0 -or $details -notmatch '(?m)^result=PASS\r?$' -or $details -notmatch '(?m)^completed=8\r?$') { throw "Runtime scenario failed: $details" }
            foreach($line in @('checkpoint_dirty=0','first_dirty=1','first_save_dirty=0','top_level_checkpoint_dirty=0','top_level_edit_dirty=1','second_dirty=1','script_result=PASS')) {
                if($trace -notmatch ('(?m)^'+[regex]::Escape($line)+'\r?$')) { throw "Missing state: $line" }
            }
            if($null -eq $noopDirty) { throw 'No-op dirty state was not recorded.' }
            $saved=Join-Path $iso.Root 'saved.fb2'
            $doc=[Xml.XmlDocument]::new(); $doc.XmlResolver=$null; $doc.Load($saved)
            $ns=[Xml.XmlNamespaceManager]::new($doc.NameTable); $ns.AddNamespace('f','http://www.gribuser.ru/xml/fictionbook/2.0')
            $ns.AddNamespace('l','http://www.w3.org/1999/xlink')
            $body=$doc.SelectSingleNode('/f:FictionBook/f:body/f:section/f:p',$ns).InnerText
            $title=$doc.SelectSingleNode('/f:FictionBook/f:description/f:title-info/f:book-title',$ns).InnerText
            $binary=$doc.SelectSingleNode('/f:FictionBook/f:binary[@id="payload.bin"]',$ns)
            if($scenario -in 'body','body-replacement' -and $body -cne 'Second body'){throw 'Saved Body mismatch.'}
            if($scenario -eq 'description' -and $title -cne 'Second title'){throw 'Saved Description mismatch.'}
            $savedPayload = if($scenario -eq 'binary') { 'CQgHBgU=' } else { 'AAECA/7/' }
            if(-not $binary -or $binary.GetAttribute('content-type') -cne 'application/octet-stream' -or
                [Convert]::ToBase64String([Convert]::FromBase64String($binary.InnerText)) -cne $savedPayload){throw 'Saved binary mismatch.'}
            $link=$doc.SelectSingleNode('/f:FictionBook/f:body/f:section/f:p/f:a[@l:href="#payload.bin"]',$ns)
            if(-not $link){throw 'Saved binary reference missing.'}
            $recovery=[Xml.XmlDocument]::new(); $recovery.XmlResolver=$null; $recovery.Load((Join-Path $iso.Root 'recovery.fb2'))
            $recoveryNs=[Xml.XmlNamespaceManager]::new($recovery.NameTable); $recoveryNs.AddNamespace('f','http://www.gribuser.ru/xml/fictionbook/2.0'); $recoveryNs.AddNamespace('l','http://www.w3.org/1999/xlink')
            $recoveryBody=$recovery.SelectSingleNode('/f:FictionBook/f:body/f:section/f:p',$recoveryNs).InnerText
            $recoveryTitle=$recovery.SelectSingleNode('/f:FictionBook/f:description/f:title-info/f:book-title',$recoveryNs).InnerText
            $recoveryBinary=$recovery.SelectSingleNode('/f:FictionBook/f:binary[@id="payload.bin"]',$recoveryNs)
            if($scenario -in 'body','body-replacement' -and $recoveryBody -cne 'First body'){throw 'Recovery Body mismatch.'}
            if($scenario -eq 'description' -and $recoveryTitle -cne 'First title'){throw 'Recovery Description mismatch.'}
            $recoveryPayload = if($scenario -eq 'binary') { 'AQMFBw==' } else { 'AAECA/7/' }
            if(-not $recoveryBinary -or $recoveryBinary.GetAttribute('content-type') -cne 'application/octet-stream' -or
                [Convert]::ToBase64String([Convert]::FromBase64String($recoveryBinary.InnerText)) -cne $recoveryPayload){throw 'Recovery binary mismatch.'}
            if(-not $recovery.SelectSingleNode('/f:FictionBook/f:body/f:section/f:p/f:a[@l:href="#payload.bin"]',$recoveryNs)){throw 'Recovery binary reference missing.'}
            $hashes=@{saved=(Get-FileHash -Algorithm SHA256 $saved).Hash;recovery=(Get-FileHash -Algorithm SHA256 (Join-Path $iso.Root 'recovery.fb2')).Hash}
            $hashes | ConvertTo-Json | Set-Content (Join-Path $scenarioEvidence 'content-hashes.json') -Encoding utf8
            $passed=$true
        } catch { $details = $_.ToString(); Write-Warning "$scenario diagnostic: $details" }
        finally {
            try { Complete-IsolatedFbeRuntime -Isolation $iso -Passed $passed } catch { $passed=$false; $details += "; cleanup: $_" }
        }
        $results.Add([pscustomobject]@{case=$scenario;noOpDirty=$noopDirty;realEditSaveUndoRedoReopen=$(if($passed){'PASS'}else{'FAIL'});exitCode=$processCode;details=$details})
    }
} finally { foreach ($name in $names) { [Environment]::SetEnvironmentVariable($name, $old[$name], 'Process') } }
$noOpStatus=Get-Rc3NoopStatus -Results @($results) -ExpectedCases $scenarios
$summary=@{scope='required no-op and real-edit Save/Reopen/Recovery contour';exeSha256=(Get-FileHash -LiteralPath $FbeExe -Algorithm SHA256).Hash;results=@($results);noOpStatus=$noOpStatus}
$summary | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $evidenceRoot 'summary.json') -Encoding utf8
$results | Format-Table -AutoSize | Out-String | Write-Host
if($summary.noOpStatus -cne 'PASS') { throw 'No-op and real-edit diagnostic did not fully pass; see summary.json.' }
Write-Host "Real-edit diagnostic PASS; No-op Script Dirty: $($summary.noOpStatus)."
