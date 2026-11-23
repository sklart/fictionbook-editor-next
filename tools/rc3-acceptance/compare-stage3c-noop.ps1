[CmdletBinding()]
param([Parameter(Mandatory)][string]$BaselineExe, [Parameter(Mandatory)][string]$CandidateExe, [int]$TimeoutSeconds = 90)
$ErrorActionPreference='Stop'
if ($env:GITHUB_ACTIONS -ne 'true' -or $env:RUNNER_ENVIRONMENT -ne 'github-hosted') { throw 'Comparison requires a disposable GitHub-hosted runner.' }
$repoRoot=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
. (Join-Path $repoRoot 'tools/tests/RuntimeTestIsolation.ps1')
$evidence=Join-Path $repoRoot 'out/tests/rc3-stage3c-comparison'
New-Item -ItemType Directory -Force -Path $evidence | Out-Null
$shared=Join-Path $evidence ('shared-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $shared | Out-Null
$noop=Join-Path $shared 'noop.js'; $trace=Join-Path $shared 'trace.txt'
[IO.File]::WriteAllText($noop,'function Run(){}',[Text.UTF8Encoding]::new($false))
$script=@'
var ext=window.external, result3c="COMPARE_NOT_RUN";
function Run(){var rows=[];try{
 rows.push("before_save="+(ext.IsDocumentModified()?1:0));
 var saved=ext.SaveCurrentDocument();rows.push("save_succeeded="+(saved?1:0));
 rows.push("after_save="+(ext.IsDocumentModified()?1:0));
 if(!saved)throw new Error("checkpoint save failed");
 apiRunCmd(@@NOOP@@);
 rows.push("after_noop="+(ext.IsDocumentModified()?1:0));
 result3c="COMPARE_STOP";
 }catch(error){result3c="COMPARE_ERROR "+String(error.message||error.description||error);}
 rows.push("script_result="+result3c);
 try{ext.WriteTextFile(@@TRACE@@,rows.join("\r\n"),"utf-8");}catch(ignore){}
}
function stage3cResult(){return result3c;}
'@
$script=$script.Replace('@@NOOP@@',($noop | ConvertTo-Json -Compress)).Replace('@@TRACE@@',($trace | ConvertTo-Json -Compress))
$fixture='<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>API3C</book-title><lang>en</lang></title-info><document-info><id>api3c</id><version>1</version></document-info></description><body><section><p>ordinary paragraph</p></section></body></FictionBook>'
$names=@('FBE_NEXT_TEST_MODE','FBE_NEXT_TEST_SCENARIO','FBE_NEXT_SCRIPT_API_ROOT','FBE_NEXT_TEST_SETTINGS_DIRECTORY','FBE_NEXT_TRACE')
$old=@{};foreach($name in $names){$old[$name]=[Environment]::GetEnvironmentVariable($name,'Process')}
$results=[Collections.Generic.List[object]]::new()
try {
    foreach($entry in @(@('3.0.10',$BaselineExe),@('3.0.11',$CandidateExe))) {
        $label=$entry[0];$source=(Resolve-Path -LiteralPath $entry[1]).Path
        $iso=New-IsolatedFbeRuntime -FbeExe $source -Name ('rc3-compare-'+$label)
        $cleaned=$false
        try {
            $null=New-IsolatedFbeSettingsSeed $iso -StartupScriptsDirectory (Join-Path $iso.Root 'ProbeScripts')
            $scriptRoot=Join-Path $iso.Root 'api3c';New-Item -ItemType Directory -Path $scriptRoot | Out-Null
            $scriptPath=Join-Path $scriptRoot 'stage3c.js';$fixturePath=Join-Path $iso.Root 'api3c.fb2';$report=Join-Path $iso.Root 'api3c.txt'
            [IO.File]::WriteAllText($scriptPath,$script,[Text.UTF8Encoding]::new($false))
            [IO.File]::WriteAllText($fixturePath,$fixture,[Text.UTF8Encoding]::new($false))
            $fixtureInputSha=(Get-FileHash -Algorithm SHA256 $fixturePath).Hash
            if(Test-Path -LiteralPath $trace){Remove-Item -LiteralPath $trace -Force}
            $env:FBE_NEXT_TEST_MODE='1';$env:FBE_NEXT_TEST_SCENARIO='scripting-stage3c-dialogs';$env:FBE_NEXT_SCRIPT_API_ROOT=$scriptRoot
            $env:FBE_NEXT_TEST_SETTINGS_DIRECTORY=Join-Path $iso.DataPath 'Settings';$env:FBE_NEXT_TRACE='0'
            $process=Start-Process -FilePath $iso.Exe -WorkingDirectory $iso.Root -ArgumentList @('--portable','-b',('"'+$report+'"'),('"'+$fixturePath+'"')) -WindowStyle Hidden -PassThru
            $exit=Wait-IsolatedFbeProcess -Process $process -Isolation $iso -Scenario ('rc3-compare-'+$label) -Report $report -TimeoutSeconds $TimeoutSeconds
            $reportText=if(Test-Path -LiteralPath $report){Get-Content -Raw -LiteralPath $report}else{'missing report'}
            $traceText=if(Test-Path -LiteralPath $trace){Get-Content -Raw -LiteralPath $trace}else{'missing trace'}
            $destination=Join-Path $evidence $label;New-Item -ItemType Directory -Force -Path $destination | Out-Null
            foreach($path in @($report,$trace,$scriptPath,$fixturePath)) { if(Test-Path -LiteralPath $path -PathType Leaf){Copy-Item -LiteralPath $path -Destination (Join-Path $destination (Split-Path -Leaf $path)) -Force} }
            if($exit -eq 0 -or $reportText -notmatch 'COMPARE_STOP' -or $traceText -notmatch '(?m)^save_succeeded=1\r?$' -or $traceText -notmatch '(?m)^after_save=0\r?$' -or $traceText -notmatch '(?m)^after_noop=([01])\r?$' -or $traceText -notmatch '(?m)^script_result=COMPARE_STOP\r?$') { throw "Comparison did not reach its deliberate stop for ${label}: $reportText $traceText" }
            $dirty=[regex]::Match($traceText,'(?m)^after_noop=([01])\r?$').Groups[1].Value -eq '1'
            $results.Add([pscustomobject]@{version=$label;exeSha256=(Get-FileHash -Algorithm SHA256 $source).Hash;scriptSha256=(Get-FileHash -Algorithm SHA256 $scriptPath).Hash;fixtureInputSha256=$fixtureInputSha;fixtureAfterSaveSha256=(Get-FileHash -Algorithm SHA256 $fixturePath).Hash;noOpDirty=$dirty;trace=$traceText})
            $cleaned=$true
        } finally { Complete-IsolatedFbeRuntime -Isolation $iso -Passed $cleaned }
    }
} finally {foreach($name in $names){[Environment]::SetEnvironmentVariable($name,$old[$name],'Process')}}
if($results.Count -ne 2 -or $results[0].scriptSha256 -cne $results[1].scriptSha256 -or $results[0].fixtureInputSha256 -cne $results[1].fixtureInputSha256) { throw 'Comparison did not use identical scripts and input documents.' }
$summary=@{scope='partial comparison, not release gate';results=@($results);sameNoOpDirty=($results[0].noOpDirty -eq $results[1].noOpDirty)}
$summary | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $evidence 'summary.json') -Encoding utf8
$results | Format-List | Out-String | Write-Host
if(-not $summary.sameNoOpDirty) { throw 'Versions differ after the same no-op sequence.' }
Write-Host 'Comparison recorded equal no-op dirty observations; this is not a Stage3C PASS.'
