[CmdletBinding()]
param([Parameter(Mandatory)][string]$FbeExe, [int]$TimeoutSeconds = 90)
$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
. (Join-Path $repoRoot 'tools/tests/RuntimeTestIsolation.ps1')
$evidence = Join-Path $repoRoot 'out/tests/description-noop-policy'
New-Item -ItemType Directory -Force $evidence | Out-Null
$names = @('FBE_NEXT_TEST_MODE','FBE_NEXT_TEST_SCENARIO','FBE_NEXT_SCRIPT_API_ROOT','FBE_NEXT_TEST_SETTINGS_DIRECTORY')
$old = @{}; foreach($name in $names){$old[$name] = [Environment]::GetEnvironmentVariable($name,'Process')}
$results = [Collections.Generic.List[object]]::new()
$noopWasDirty = $null
$cases = @(
    @{name='accepted-noop';policy='known-script-load';script='';failure=''},
    @{name='bundled-noop-matrix';policy='snapshot';script='';failure=''},
    @{name='strict-noop';policy='unchanged';script='';failure=''},
    @{name='body-edit';policy='known-script-load';script='document.getElementById("fbw_body").getElementsByTagName("P")[0].innerText="Changed body";';failure='live=0'},
    @{name='description-edit';policy='known-script-load';script='document.getElementById("tiTitle").value="Changed title";';failure='live=0'},
    @{name='binary-edit';policy='known-script-load';script='e.ReplaceBinaryFromFile("payload.bin",@@BINARY@@,"application/octet-stream");';failure='binary=0'},
    @{name='checkbox-live';policy='known-script-load';prepare='<input id="noopCheckbox" type="checkbox">';script='document.getElementById("noopCheckbox").checked=true;';failure='live=0'},
    @{name='multiselect-live';policy='known-script-load';prepare='<select id="noopSelect" multiple><option value="first" selected>First</option><option value="second">Second</option></select>';script='var s=document.getElementById("noopSelect");if(s.value!=="first")throw new Error("multiselect fixture");s.options[1].selected=true;if(s.value!=="first")throw new Error("first selected option changed");';failure='live=0'},
    @{name='checkpoint-save';policy='known-script-load';script='var p=document.getElementById("fbw_body").getElementsByTagName("P")[0],original=p.innerText;p.innerText=original+" changed";p.innerText=original;if(!e.SaveCurrentDocument())throw new Error("checkpoint save failed");';failure='savepoints=0'},
    @{name='native-history';policy='known-script-load';script='var p=document.getElementById("fbw_body").getElementsByTagName("P")[0],s=p.innerText;e.BeginUndoUnit(document,"No-op policy history probe");try{p.innerText="Temporary edit";p.innerText=s;}finally{e.EndUndoUnit(document);}';failure='history=0'}
)
try {
    foreach($scenario in $cases) {
        $iso = New-IsolatedFbeRuntime -FbeExe $FbeExe -Name ('noop-policy-' + $scenario.name)
        $passed = $false
        try {
            $null = New-IsolatedFbeSettingsSeed $iso -StartupScriptsDirectory (Join-Path $iso.Root 'ProbeScripts')
            $fixture = Join-Path $iso.Root 'fixture.fb2'
            $xml = '<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0" xmlns:l="http://www.w3.org/1999/xlink"><description><title-info><genre>prose</genre><author><nickname>Tester</nickname></author><book-title>Original title</book-title><lang>en</lang></title-info><document-info><author><nickname>Tester</nickname></author><date value="2026-01-01">2026-01-01</date><id>description-noop-policy</id><version>1.0</version></document-info></description><body><section><p>Original body</p></section></body><binary id="payload.bin" content-type="application/octet-stream">AAECA/7/</binary></FictionBook>'
            [IO.File]::WriteAllText($fixture,$xml,[Text.UTF8Encoding]::new($false))
            $binary = Join-Path $iso.Root 'replacement.bin'
            [IO.File]::WriteAllBytes($binary,[byte[]](9,8,7,6,5))
            $body = $scenario.script.Replace('@@BINARY@@',($binary | ConvertTo-Json -Compress))
            $script = 'var e=window.external,result="NOT RUN";function Run(){try{'+$body+'result="PASS";}catch(error){result="FAIL "+String(error.message||error);}}function CompatResult(){return result;}'
            [IO.File]::WriteAllText((Join-Path $iso.Root 'probe.js'),$script,[Text.UTF8Encoding]::new($false))
            $prepare = ''
            if($scenario.ContainsKey('prepare')) {
                $setup = 'var result="NOT RUN";function Run(){try{document.getElementById("fbw_desc").insertAdjacentHTML("beforeEnd",'+($scenario.prepare | ConvertTo-Json -Compress)+');if(!window.external.SaveCurrentDocument())throw new Error("prepare save");if(window.external.IsDocumentModified())throw new Error("prepare dirty");result="PASS";}catch(error){result="FAIL "+String(error.message||error);}}function CompatResult(){return result;}'
                [IO.File]::WriteAllText((Join-Path $iso.Root 'prepare.js'),$setup,[Text.UTF8Encoding]::new($false))
                $prepare = '<step name="prepare-live-controls" script="prepare.js"/>'
            }
            $suite = '<suite>'+$prepare+'<step name="'+$scenario.name+'" script="probe.js" rawDirty="'+$scenario.policy+'"/></suite>'
            if ($scenario.name -eq 'bundled-noop-matrix') {
                $encoding = [Text.UTF8Encoding]::new($true)
                $scripts = Join-Path $repoRoot 'runtime/Scripts'
                $cancel = [IO.File]::ReadAllText((Join-Path $scripts '05_Иллюстрации/05_Удалить все вложения.js'))
                $reportScript = [IO.File]::ReadAllText((Join-Path $scripts '10_Статистика/03_Статистика документа.js'))
                # Observe the actual prompt and report while keeping their native dialogs and responses.
                $cancel += @"

var candidateCancelRun=Run;
Run=function(){var ask=window.AskYesNo,seen=false;try{
 window.AskYesNo=function(msg){var yes=ask(msg);seen=!yes && String(msg).indexOf("удалить все вложения")>=0;return yes;};
 candidateCancelRun();
}finally{window.AskYesNo=ask;}window.noopResult=seen?"PASS":"missing cancellation";};
function CompatResult(){return window.noopResult;}
"@
                $reportScript += @"

var candidateReportRun=Run;
Run=function(){var show=window.MsgBox,seen=false;try{
 window.MsgBox=function(msg){seen=String(msg).indexOf("Общий размер текста")>=0;return show(msg);};
 candidateReportRun();
}finally{window.MsgBox=show;}window.noopResult=seen?"PASS":"missing report";};
function CompatResult(){return window.noopResult;}
"@
                [IO.File]::WriteAllText((Join-Path $iso.Root 'cancel.js'),$cancel,$encoding)
                [IO.File]::WriteAllText((Join-Path $iso.Root 'report.js'),$reportScript,$encoding)
                $edit = 'function Run(){var e=window.external;e.BeginUndoUnit(document,"Fixture edit");try{document.getElementById("fbw_body").getElementsByTagName("p")[0].innerText+=" edited";}finally{e.EndUndoUnit(document);}}function CompatResult(){return "PASS";}'
                [IO.File]::WriteAllText((Join-Path $iso.Root 'edit.js'),$edit,$encoding)
                $steps = foreach ($state in 'clean','dirty','redo') {
                    if ($state -eq 'dirty') { '<step name="make-edit" script="edit.js"/>' }
                    if ($state -eq 'redo') { '<step name="create-redo" command="undo"/>' }
                    foreach ($name in 'probe','cancel','report') {
                        "<step name=`"$state-$name`" script=`"$name.js`" snapshot=`"$state`" answer=`"no`"/>"
                    }
                }
                $suite = '<suite>' + ($steps -join '') + '</suite>'
            }
            [IO.File]::WriteAllText((Join-Path $iso.Root 'suite.xml'),$suite,[Text.UTF8Encoding]::new($false))
            $env:FBE_NEXT_TEST_MODE='1';$env:FBE_NEXT_TEST_SCENARIO='binary-scripting-compatibility';$env:FBE_NEXT_SCRIPT_API_ROOT=$iso.Root
            $env:FBE_NEXT_TEST_SETTINGS_DIRECTORY=Join-Path $iso.DataPath 'Settings'
            $report=Join-Path $iso.Root 'report.txt'
            $process=Start-Process $iso.Exe -WorkingDirectory $iso.Root -ArgumentList @('--portable','-b',('"'+$report+'"'),('"'+$fixture+'"')) -WindowStyle Hidden -PassThru
            $code=Wait-IsolatedFbeProcess -Process $process -Isolation $iso -Scenario ('noop-policy-'+$scenario.name) -Report $report -TimeoutSeconds $TimeoutSeconds
            $text=if(Test-Path $report){Get-Content $report -Raw}else{''}
            if(Test-Path $report){Copy-Item $report (Join-Path $evidence ($scenario.name+'.txt')) -Force}
            if($scenario.name -eq 'accepted-noop') {
                if($code -ne 0 -or $text -notmatch '(?m)^result=PASS\r?$' -or $text -notmatch '(?m)^completed=1\r?$'){throw "No-op was not accepted: $text"}
                $noopWasDirty=$text -match '(?m)^script-setup-raw-dirty=FAIL\r?$'
                $expected=if($noopWasDirty){'KNOWN FAILURE'}else{'PASS'}
                if($text -notmatch ('(?m)^script-setup-dirty='+$expected+'\r?$')){throw "No-op classification missing: $text"}
            } elseif ($scenario.name -eq 'bundled-noop-matrix') {
                if ($code -ne 0 -or $text -notmatch '(?m)^completed=11\r?$' -or
                    [regex]::Matches($text,'(?m)^script-setup-dirty=PASS\r?$').Count -ne 9) { throw "Bundled no-op matrix failed: $text" }
            } elseif($scenario.name -eq 'strict-noop') {
                if($null -eq $noopWasDirty){throw 'Known-noop control did not run.'}
                if($noopWasDirty) {
                    if($code -ne 1 -or $text -notmatch 'raw dirty changed: before=0 after=1' -or $text -notmatch '(?m)^result=FAIL\r?$'){throw "Strict dirty regression was bypassed: $text"}
                } elseif($code -ne 0 -or $text -notmatch '(?m)^result=PASS\r?$'){throw "Clean strict no-op failed: $text"}
            } else {
                if($code -ne 1 -or $text -notmatch '(?m)^result=FAIL\r?$' -or $text -notmatch '(?m)^script-setup-dirty=FAIL\r?$' -or $text -notmatch [regex]::Escape($scenario.failure)) {
                    throw "Mutation was not rejected by its invariant ($($scenario.name)): $text"
                }
            }
            $results.Add([pscustomobject]@{case=$scenario.name;status='PASS';processExitCode=$code;expectedInvariantFailure=$scenario.failure})
            $passed=$true
        } catch {
            $results.Add([pscustomobject]@{case=$scenario.name;status='FAIL';details=$_.Exception.Message})
            Write-Warning "$($scenario.name) failed: $($_.Exception.Message)"
        } finally { Complete-IsolatedFbeRuntime -Isolation $iso -Passed $passed }
    }
    $failures = @($results | Where-Object status -eq 'FAIL')
    $status = if ($failures.Count) { 'FAIL' } else { 'PASS' }
    @{status=$status;cases=$results;scope='native no-op acceptance and rejection of real mutations'} | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $evidence 'summary.json') -Encoding utf8
    if ($failures.Count) { throw "Description no-op policy failed: $($failures.case -join ', ')" }
    Write-Host "Description no-op policy: $($results.Count)/$($cases.Count) PASS; strict default and actual mutation failures preserved."
} finally { foreach($name in $names){[Environment]::SetEnvironmentVariable($name,$old[$name],'Process')} }
