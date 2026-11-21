[CmdletBinding()]
param([Parameter(Mandatory)][string]$FbeExe,[ValidateSet('body','description')][string]$Case='body',[ValidateSet('save','save-as')][string]$Operation='save',[switch]$Hook,[int]$TimeoutSeconds=90)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$iso=New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'save-timer';$passed=$false
$names=@('FBE_NEXT_TEST_MODE','FBE_NEXT_TEST_SCENARIO','FBE_NEXT_SCRIPT_API_ROOT','FBE_NEXT_TEST_SAVE_TIMER','FBE_NEXT_TEST_FIRST_SAVE_PATH','FBE_NEXT_TEST_SAVE_PATH');$old=@{};foreach($name in $names){$old[$name]=[Environment]::GetEnvironmentVariable($name,'Process')}
try {
 $null=New-IsolatedFbeSettingsSeed -Isolation $iso -StartupScriptsDirectory (Join-Path $iso.Root 'empty-scripts')
 $xml='<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><nickname>Test</nickname></author><book-title>Before</book-title><lang>en</lang></title-info><document-info><author><nickname>Test</nickname></author><date value="2026-01-01">2026</date><id>save-timer</id><version>1.0</version></document-info></description><body><section><p id="probe">Before body</p></section></body></FictionBook>'
 $fixture=Join-Path $iso.Root 'input.fb2';[IO.File]::WriteAllText($fixture,$xml,[Text.UTF8Encoding]::new($false))
 $edit=if($Case -eq 'body'){'document.getElementById("probe").innerText="AFTER_SAVE_BODY"'}else{'document.getElementById("tiTitle").value="AFTER_SAVE_TITLE"'}
 $script=if($Hook){'var original=GetBinaries;function Run(){document.getElementById("diVersion").value="not-a-number";window.GetBinaries=function(d){var result=original(d);window.GetBinaries=original;'+$edit+';return result;};}'}else{'function Run(){document.getElementById("diVersion").value="not-a-number";window.setTimeout(function(){'+$edit+';},1);}'}
 [IO.File]::WriteAllText((Join-Path $iso.Root 'timer.js'),$script,[Text.UTF8Encoding]::new($false))
 $target=if($Operation -eq 'save'){$fixture}else{Join-Path $iso.Root 'save-as.fb2'};$first=Join-Path $iso.Root 'first-save.fb2'
 $env:FBE_NEXT_TEST_MODE='1';$env:FBE_NEXT_TEST_SCENARIO=if($Operation -eq 'save'){'issue72-normal-save'}else{'issue72-normal-save-as'};$env:FBE_NEXT_TEST_SAVE_PATH=$target;$env:FBE_NEXT_TEST_FIRST_SAVE_PATH=$first;$env:FBE_NEXT_TEST_SAVE_TIMER=$Case;$env:FBE_NEXT_SCRIPT_API_ROOT=$iso.Root;$report=Join-Path $iso.Root 'report.txt'
 $process=Start-Process $iso.Exe -WorkingDirectory $iso.Root -ArgumentList @('--portable','-b',('"'+$report+'"'),('"'+$fixture+'"')) -WindowStyle Hidden -PassThru
 $code=Wait-IsolatedFbeProcess -Process $process -Isolation $iso -Scenario 'save-timer' -Report $report -TimeoutSeconds $TimeoutSeconds
 if($code -ne 0 -or !(Test-Path -LiteralPath $report)){throw "Save timer did not report: exit=$code"}
 $state=Get-Content -LiteralPath $report -Raw;Write-Host "case=$Case operation=$Operation hook=$Hook`n$state"
 if($state -notmatch '(?m)^timer-ran=1\r?$'){throw 'INCONCLUSIVE: timer did not run during Save.'}
 if($state -notmatch '(?m)^saved=1\r?$'){throw 'INCONCLUSIVE: Save did not complete.'}
 if($state -notmatch '(?m)^dirty-after=1\r?$') {
  $firstResult=[Xml.XmlDocument]::new();$firstResult.XmlResolver=$null;$firstResult.Load($target)
  $missing=if($Case -eq 'body'){'AFTER_SAVE_BODY'}else{'AFTER_SAVE_TITLE'}
  if(-not $firstResult.OuterXml.Contains($missing)){throw "FAIL: $missing differs from saved file but document is clean."}
  throw 'INCONCLUSIVE: callback was included in the first save.'
 }
 if($state -notmatch '(?m)^notice=1\r?$' -or $state -notmatch '(?m)^first-copy=1\r?$' -or $state -notmatch '(?m)^decision=1\r?$' -or $state -notmatch '(?m)^closed=1\r?$'){throw 'Close and second Save did not complete.'}
 $saved=[Xml.XmlDocument]::new();$saved.XmlResolver=$null;$saved.Load($first);$key=if($Case -eq 'body'){'AFTER_SAVE_BODY'}else{'AFTER_SAVE_TITLE'}
 if($saved.OuterXml.Contains($key)){throw 'INCONCLUSIVE: edit was serialized; no reentrant-after-serialization observation.'}
 if($state -notmatch '(?m)^dirty-after=1\r?$'){throw "FAIL: $key differs from saved file but document is clean."}
 $saved.Load($target); if(-not $saved.OuterXml.Contains($key)){throw 'Close/Save lost the later edit.'}
 $env:FBE_NEXT_TEST_SCENARIO='issue72-timer-reopen';$reopenReport=Join-Path $iso.Root 'reopen-report.txt'
 $reopen=Start-Process $iso.Exe -WorkingDirectory $iso.Root -ArgumentList @('--portable','-b',('"'+$reopenReport+'"'),('"'+$target+'"')) -WindowStyle Hidden -PassThru
 $reopenCode=Wait-IsolatedFbeProcess -Process $reopen -Isolation $iso -Scenario 'save-timer-reopen' -Report $reopenReport -TimeoutSeconds $TimeoutSeconds
 if($reopenCode -ne 0 -or !(Test-Path -LiteralPath $reopenReport) -or (Get-Content -LiteralPath $reopenReport -Raw) -notmatch 'result=PASS'){throw 'Reopening did not restore the later edit.'}
 Write-Host "first-sha256=$((Get-FileHash -LiteralPath $first -Algorithm SHA256).Hash) final-sha256=$((Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash)"
 $passed=$true;Write-Host "Save timer $Case PASS."
}finally{foreach($name in $names){[Environment]::SetEnvironmentVariable($name,$old[$name],'Process')};Complete-IsolatedFbeRuntime -Isolation $iso -Passed $passed}
