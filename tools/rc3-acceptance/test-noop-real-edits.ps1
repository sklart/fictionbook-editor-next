[CmdletBinding()]
param([Parameter(Mandatory)][string]$FbeExe, [int]$TimeoutSeconds = 120)
$ErrorActionPreference = 'Stop'
if ($env:GITHUB_ACTIONS -ne 'true' -or $env:RUNNER_ENVIRONMENT -ne 'github-hosted') {
    throw 'This diagnostic is restricted to a disposable GitHub-hosted runner.'
}
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
. (Join-Path $repoRoot 'tools/tests/RuntimeTestIsolation.ps1')
$evidenceRoot = Join-Path $repoRoot 'out/tests/rc3-noop-real-edits'
New-Item -ItemType Directory -Force -Path $evidenceRoot | Out-Null
$names = @('FBE_NEXT_TEST_MODE','FBE_NEXT_TEST_SCENARIO','FBE_NEXT_SCRIPT_API_ROOT','FBE_NEXT_TEST_SETTINGS_DIRECTORY')
$old = @{}; foreach ($name in $names) { $old[$name] = [Environment]::GetEnvironmentVariable($name, 'Process') }
$results = [Collections.Generic.List[object]]::new()
try {
    foreach ($case in @('body','description','binary')) {
        $iso = New-IsolatedFbeRuntime -FbeExe $FbeExe -Name ('rc3-noop-' + $case)
        $passed = $false; $details = ''; $processCode = $null; $noopDirty = $null
        $caseEvidence = Join-Path $evidenceRoot $case
        New-Item -ItemType Directory -Force -Path $caseEvidence | Out-Null
        try {
            $null = New-IsolatedFbeSettingsSeed $iso -StartupScriptsDirectory (Join-Path $iso.Root 'ProbeScripts')
            $fixture = Join-Path $iso.Root 'fixture.fb2'
            $xml = '<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><nickname>Tester</nickname></author><book-title>Original title</book-title><lang>en</lang></title-info><document-info><author><nickname>Tester</nickname></author><date value="2026-01-01">2026-01-01</date><id>rc3-noop-real-edits</id><version>1.0</version></document-info></description><body><section><p>Original body</p></section></body><binary id="payload.bin" content-type="application/octet-stream">AAECA/7/</binary></FictionBook>'
            [IO.File]::WriteAllText($fixture, $xml, [Text.UTF8Encoding]::new($false))
            [IO.File]::WriteAllBytes((Join-Path $iso.Root 'first.bin'), [byte[]](1,3,5,7))
            [IO.File]::WriteAllBytes((Join-Path $iso.Root 'second.bin'), [byte[]](9,8,7,6,5))
            [IO.File]::WriteAllText((Join-Path $iso.Root 'noop.js'), 'function Run(){}', [Text.UTF8Encoding]::new($false))
            $lookup = switch ($case) {
                'body' { 'function value(){var p=document.getElementById("fbw_body").getElementsByTagName("P");for(var i=0;i<p.length;i++)if(String(p[i].innerText).indexOf("body")>=0)return String(p[i].innerText);throw new Error("body paragraph missing");}' }
                'description' { 'function value(){return String(document.getElementById("tiTitle").value);}' }
                default { 'function value(){var n=new ActiveXObject("Msxml2.DOMDocument.6.0");n.loadXML("<data/>");n.documentElement.dataType="bin.base64";n.documentElement.nodeTypedValue=GetImageData("payload.bin");return String(n.documentElement.text).replace(/\s/g,"");}' }
            }
            $edit = switch ($case) {
                'body' { 'function edit(which){var p=document.getElementById("fbw_body").getElementsByTagName("P");for(var i=0;i<p.length;i++)if(String(p[i].innerText).indexOf("body")>=0){e.BeginUndoUnit(document,"RC3 body edit");try{p[i].innerText=which===1?"First body":"Second body";}finally{e.EndUndoUnit(document);}return;}throw new Error("body paragraph missing");}' }
                'description' { 'function edit(which){var f=document.getElementById("tiTitle"),token=e.BeginDocumentUpdate("RC3 description edit","{}");try{f.focus();var r=f.createTextRange();r.select();r.text=which===1?"First title":"Second title";}finally{e.EndDocumentUpdate(token);}}' }
                default { 'function edit(which){e.ReplaceBinaryFromFile("payload.bin",which===1?@@FIRST@@:@@SECOND@@,"application/octet-stream");}' }
            }
            $edit = $edit.Replace('@@FIRST@@', ((Join-Path $iso.Root 'first.bin') | ConvertTo-Json -Compress)).Replace('@@SECOND@@', ((Join-Path $iso.Root 'second.bin') | ConvertTo-Json -Compress))
            $expectedFirst = switch ($case) { 'body' { 'First body' } 'description' { 'First title' } default { 'AQMFBw==' } }
            $expectedSecond = switch ($case) { 'body' { 'Second body' } 'description' { 'Second title' } default { 'CQgHBgU=' } }
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
 edit(2);mark("second_dirty",e.IsDocumentModified()?1:0);a(e.IsDocumentModified(),"second edit reported clean");a(value()===@@EXPECTED2@@,"second edit content");
 result="PASS";
 }catch(error){result="FAIL "+String(error.message||error.description||error);}
 finally{mark("script_result",result);try{e.WriteTextFile(@@TRACE@@,trace.join("\r\n"),"utf-8");}catch(ignore){}}
}
function CompatResult(){return result;}
'@
            $main = $main.Replace('@@LOOKUP@@',$lookup).Replace('@@EDIT@@',$edit).Replace('@@NOOP@@',((Join-Path $iso.Root 'noop.js') | ConvertTo-Json -Compress)).Replace('@@TRACE@@',((Join-Path $iso.Root 'trace.txt') | ConvertTo-Json -Compress)).Replace('@@EXPECTED1@@',($expectedFirst | ConvertTo-Json -Compress)).Replace('@@EXPECTED2@@',($expectedSecond | ConvertTo-Json -Compress))
            [IO.File]::WriteAllText((Join-Path $iso.Root 'edit.js'), $main, [Text.UTF8Encoding]::new($false))
            foreach ($check in @(@('undo.js',$expectedFirst),@('redo.js',$expectedSecond),@('reopen.js',$expectedSecond))) {
                $code = $lookup + "`r`nfunction Run(){} function CompatResult(){try{return value()===" + ($check[1] | ConvertTo-Json -Compress) + '?"PASS":"FAIL unexpected content: "+value();}catch(e){return "FAIL "+String(e.message||e);}}'
                [IO.File]::WriteAllText((Join-Path $iso.Root $check[0]), $code, [Text.UTF8Encoding]::new($false))
            }
            $binaryFirst = if ($case -eq 'binary') { ' id="payload.bin" expected="first.bin"' } else { '' }
            $binarySecond = if ($case -eq 'binary') { ' id="payload.bin" expected="second.bin"' } else { '' }
            $suite = '<suite><step name="edit" script="edit.js"' + $binarySecond + '/><step name="undo" command="undo" script="undo.js"' + $binaryFirst + '/><step name="redo" command="redo" script="redo.js" save="saved.fb2"' + $binarySecond + '/><step name="close" command="close"/><step name="reopen" command="open" open="saved.fb2" script="reopen.js"' + $binarySecond + '/></suite>'
            [IO.File]::WriteAllText((Join-Path $iso.Root 'suite.xml'), $suite, [Text.UTF8Encoding]::new($false))
            $env:FBE_NEXT_TEST_MODE='1'; $env:FBE_NEXT_TEST_SCENARIO='binary-scripting-compatibility'; $env:FBE_NEXT_SCRIPT_API_ROOT=$iso.Root
            $env:FBE_NEXT_TEST_SETTINGS_DIRECTORY=Join-Path $iso.DataPath 'Settings'
            $report=Join-Path $iso.Root 'report.txt'
            $process=Start-Process -FilePath $iso.Exe -WorkingDirectory $iso.Root -ArgumentList @('--portable','-b',('"'+$report+'"'),('"'+$fixture+'"')) -WindowStyle Hidden -PassThru
            $processCode=Wait-IsolatedFbeProcess -Process $process -Isolation $iso -Scenario ('rc3-noop-'+$case) -Report $report -TimeoutSeconds $TimeoutSeconds
            $details=if(Test-Path -LiteralPath $report){Get-Content -LiteralPath $report -Raw}else{'missing report'}
            $tracePath=Join-Path $iso.Root 'trace.txt'
            $trace=if(Test-Path -LiteralPath $tracePath){Get-Content -LiteralPath $tracePath -Raw}else{''}
            if ($trace -match '(?m)^noop_dirty=([01])\r?$') { $noopDirty = $Matches[1] -eq '1' }
            foreach($file in @('report.txt','trace.txt','saved.fb2','suite.xml')) {
                $source=Join-Path $iso.Root $file
                if(Test-Path -LiteralPath $source -PathType Leaf){Copy-Item -LiteralPath $source -Destination (Join-Path $caseEvidence $file) -Force}
            }
            if($processCode -ne 0 -or $details -notmatch '(?m)^result=PASS\r?$' -or $details -notmatch '(?m)^completed=5\r?$') { throw "Runtime scenario failed: $details" }
            foreach($line in @('checkpoint_dirty=0','first_dirty=1','first_save_dirty=0','second_dirty=1','script_result=PASS')) {
                if($trace -notmatch ('(?m)^'+[regex]::Escape($line)+'\r?$')) { throw "Missing state: $line" }
            }
            if($null -eq $noopDirty) { throw 'No-op dirty state was not recorded.' }
            $saved=Join-Path $iso.Root 'saved.fb2'
            $doc=[Xml.XmlDocument]::new(); $doc.XmlResolver=$null; $doc.Load($saved)
            $ns=[Xml.XmlNamespaceManager]::new($doc.NameTable); $ns.AddNamespace('f','http://www.gribuser.ru/xml/fictionbook/2.0')
            $body=$doc.SelectSingleNode('/f:FictionBook/f:body/f:section/f:p',$ns).InnerText
            $title=$doc.SelectSingleNode('/f:FictionBook/f:description/f:title-info/f:book-title',$ns).InnerText
            $binary=$doc.SelectSingleNode('/f:FictionBook/f:binary[@id="payload.bin"]',$ns)
            if($case -eq 'body' -and $body -cne 'Second body'){throw 'Saved Body mismatch.'}
            if($case -eq 'description' -and $title -cne 'Second title'){throw 'Saved Description mismatch.'}
            if($case -eq 'binary' -and (-not $binary -or [Convert]::ToBase64String([Convert]::FromBase64String($binary.InnerText)) -cne 'CQgHBgU=')){throw 'Saved binary bytes mismatch.'}
            $passed=$true
        } catch { $details = $_.ToString(); Write-Warning "$case diagnostic: $details" }
        finally {
            try { Complete-IsolatedFbeRuntime -Isolation $iso -Passed $passed } catch { $passed=$false; $details += "; cleanup: $_" }
        }
        $results.Add([pscustomobject]@{case=$case;noOpDirty=$noopDirty;realEditSaveUndoRedoReopen=$(if($passed){'PASS'}else{'FAIL'});exitCode=$processCode;details=$details})
    }
} finally { foreach ($name in $names) { [Environment]::SetEnvironmentVariable($name, $old[$name], 'Process') } }
$summary=@{scope='partial diagnostic, not release gate';results=@($results);noOpStatus=$(if(@($results | Where-Object { $_.noOpDirty -ne $false }).Count){'FAIL'}else{'PASS'})}
$summary | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $evidenceRoot 'summary.json') -Encoding utf8
$results | Format-Table -AutoSize | Out-String | Write-Host
if($summary.noOpStatus -ne 'PASS' -or @($results | Where-Object realEditSaveUndoRedoReopen -ne 'PASS').Count) { throw 'No-op and real-edit diagnostic did not fully pass; see summary.json.' }
Write-Host 'No-op and real-edit diagnostic PASS.'
