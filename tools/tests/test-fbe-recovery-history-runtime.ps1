[CmdletBinding()]
param([Parameter(Mandatory)][string]$FbeExe, [ValidateSet('plain','links','emphasis','table','binary','binary-second','binary-clean','binary-forced','binary-noop','description','source-invalid','reentrant-new','invalid-second','write-error')][string]$Case = 'links', [switch]$Restart)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$exe=$FbeExe
$iso=New-IsolatedFbeRuntime -FbeExe $exe -Name 'recovery-redo-links'
$passed=$false
try {
 $null=New-IsolatedFbeSettingsSeed -Isolation $iso -StartupScriptsDirectory (Join-Path $iso.Root 'empty-scripts')
 $p=if($Case -eq 'links'){'<p>First <a l:href="#target">one</a> <a l:href="#target">two</a> <a l:href="#target">three</a>.</p>'}elseif($Case -eq 'emphasis'){'<p>First <emphasis>one</emphasis><emphasis>two</emphasis>.</p>'}else{'<p>First plain text.</p>'}
 $extra=if($Case -eq 'table'){'<table id="grid"><tr><td colspan="2">Cell one</td></tr><tr><td>Cell two</td><td>Cell three</td></tr></table>'}elseif($Case -eq 'invalid'){'<subtitle><strong><p>Invalid nested paragraph.</p></strong></subtitle>'}else{''}
 $binary=if($Case -in @('binary','binary-second','binary-clean','binary-forced','binary-noop')){'<binary id="payload.bin" content-type="application/octet-stream">AAECA/7/</binary>'}else{''}
 $xml='<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0" xmlns:l="http://www.w3.org/1999/xlink"><description><title-info><genre>prose</genre><author><nickname>Test</nickname></author><book-title>Recovery</book-title><lang>en</lang></title-info><document-info><author><nickname>Test</nickname></author><date value="2026-01-01">2026</date><id>recovery-redo</id><version>1.0</version></document-info></description><body><section id="target">'+$p+'<p>Second paragraph.</p>'+$extra+'</section></body>'+$binary+'</FictionBook>'
 $fixture=Join-Path $iso.Root 'input.fb2';[IO.File]::WriteAllText($fixture,$xml,[Text.UTF8Encoding]::new($false))
 $report=Join-Path $iso.Root 'report.txt'; if($Case -eq 'reentrant-new'){$env:FBE_NEXT_TRACE='1';$env:FBE_NEXT_FAULT_INJECT='recovery-reentrant-new'}; $env:FBE_NEXT_TEST_MODE='1';$env:FBE_NEXT_TEST_SCENARIO=if($Case -eq 'source-invalid'){'recovery-source-refusal'}else{'recovery-redo-links-probe'}; if($Case -eq 'source-invalid'){$env:FBE_NEXT_SCRIPT_API_ROOT=$iso.Root}; if($Case -eq 'description'){$env:FBE_NEXT_RECOVERY_TITLE_PROBE='1'} elseif($Case -eq 'binary-second'){$env:FBE_NEXT_RECOVERY_BINARY_SECOND='1'} elseif($Case -in @('binary-clean','binary-forced')){$env:FBE_NEXT_RECOVERY_BINARY_CLEAN='1';if($Case -eq 'binary-forced'){$env:FBE_NEXT_RECOVERY_FORCED='1'}} elseif($Case -eq 'binary-noop'){$env:FBE_NEXT_RECOVERY_BINARY_NOOP='1'}
 if($Case -in @('invalid-second','write-error')){$env:FBE_NEXT_SCRIPT_API_ROOT=$iso.Root; if($Case -eq 'invalid-second'){$env:FBE_NEXT_RECOVERY_SECOND_INVALID='1'}else{$env:FBE_NEXT_RECOVERY_WRITE_ERROR='1'}}
 $process=Start-Process -FilePath $iso.Exe -WorkingDirectory $iso.Runtime -WindowStyle Hidden -ArgumentList @('--portable','-b',('"'+$report+'"'),('"'+$fixture+'"')) -PassThru
 $code=Wait-IsolatedFbeProcess -Process $process -Isolation $iso -Scenario $env:FBE_NEXT_TEST_SCENARIO -Report $report -TimeoutSeconds 90
 if(Test-Path -LiteralPath $report){$text=Get-Content -LiteralPath $report -Raw;Write-Output "case=$Case exit=$code $text"}else{throw "Missing report: $($iso.Root)"}
 $dest=Join-Path $iso.Root 'results';New-Item -ItemType Directory -Force -Path $dest | Out-Null;Copy-Item -LiteralPath $report -Destination (Join-Path $dest ($Case+'.txt'))
 $recoveryRoot=Join-Path $iso.DataPath 'Recovery'
 $snapshots=@(Get-ChildItem -LiteralPath $recoveryRoot -File -Recurse -ErrorAction SilentlyContinue)
 if($snapshots.Count -ne 1){throw "Expected one recovery snapshot; got $($snapshots.Count)"}
 Copy-Item -LiteralPath $snapshots[0].FullName -Destination (Join-Path $dest ($Case+'.fb2'))
 $doc=[Xml.XmlDocument]::new(); $doc.XmlResolver=$null; $doc.Load($snapshots[0].FullName)
 $ns=[Xml.XmlNamespaceManager]::new($doc.NameTable); $ns.AddNamespace('f','http://www.gribuser.ru/xml/fictionbook/2.0'); $ns.AddNamespace('l','http://www.w3.org/1999/xlink')
 $title=$doc.SelectSingleNode('/f:FictionBook/f:description/f:title-info/f:book-title',$ns)
 $para=$doc.SelectSingleNode('/f:FictionBook/f:body/f:section[@id="target"]/f:p[1]',$ns)
 if (!$title -or $title.InnerText -cne $(if($Case -eq 'description'){'RECOVERY_CHANGED_TITLE'}elseif($Case -eq 'source-invalid'){'RecoveryEdited'}else{'Recovery'}) -or !$para -or !$para.InnerText.StartsWith($(if($Case -in @('binary-clean','binary-forced','description','source-invalid')){'First'}else{'FAirst'}))) { throw 'Recovery did not preserve Description or the unsaved text.' }
 if ($Case -eq 'links') {
  $links=$para.SelectNodes('f:a',$ns); if ($links.Count -ne 3) { throw 'Recovery lost links.' }
  foreach($link in $links) { if($link.GetAttribute('href','http://www.w3.org/1999/xlink') -cne '#target') { throw 'Recovery changed a link target.' } }
 }
 if ($Case -in @('binary','binary-second','binary-clean','binary-forced','binary-noop')) {
  $binaryNode=$doc.SelectSingleNode('/f:FictionBook/f:binary[@id="payload.bin"]',$ns)
  if (!$binaryNode -or $binaryNode.GetAttribute('content-type') -cne 'application/octet-stream' -or [Convert]::ToBase64String([Convert]::FromBase64String($binaryNode.InnerText)) -cne $(if($Case -in @('binary-second','binary-clean','binary-forced')){'CQgHBg=='}else{'AAECA/7/'})) { throw 'Recovery changed binary payload or metadata.' }
 }
 if($Case -eq 'source-invalid') {
  $first=Join-Path $iso.Root 'source-first.fb2'; $same=(Test-Path -LiteralPath $first) -and (Get-FileHash -LiteralPath $first).Hash -eq (Get-FileHash -LiteralPath $snapshots[0].FullName).Hash
  if($code -ne 0 -or !$same -or $text -notmatch 'first-saved=1 second-saved=0 source-same=1 history-same=1 history-executed=1') { throw "Invalid Source recovery failed: $text; same=$same" }
  Write-Host 'Invalid Source retained previous recovery and Undo/Redo.'; $passed=$true; return
 }
 if ($Restart) {
  if ($Case -notin @('binary-clean','binary-second','links')) { throw 'Restart is supported for binary-clean, binary-second and links.' }
  if ($code -ne 0) { throw "Recovery creation failed before restart: $text" }
  $restartReport=Join-Path $iso.Root 'restart-report.txt'
  $env:FBE_NEXT_TEST_SCENARIO='normal-recovery-verify'
  $env:FBE_NEXT_RECOVERY_VERIFY_CONTENT=if($Case -eq 'links'){'links'}else{'binary'}
  $restartedProcess=Start-Process -FilePath $iso.Exe -WorkingDirectory $iso.Runtime -WindowStyle Hidden -ArgumentList @('--portable','-b',('"'+$restartReport+'"')) -PassThru
  $restartCode=Wait-IsolatedFbeProcess -Process $restartedProcess -Isolation $iso -Scenario 'normal-recovery-verify' -Report $restartReport -TimeoutSeconds 90
  $restartText=if(Test-Path -LiteralPath $restartReport){Get-Content -LiteralPath $restartReport -Raw}else{'<missing>'}
  foreach($required in @('payload=1','identity=1','dirty=1','cleaned=1')) { if($restartCode -ne 0 -or $restartText -notmatch [regex]::Escape($required)) { throw "Recovery restart failed: $restartText" } }
  Write-Host "Restart preserved recovery content: $Case"
 }
 if($Case -eq 'description') { if($code -ne 0 -or $text -notmatch 'saved=1 wrote=1 live-title=1') { throw "Description recovery failed: $text" }; $passed=$true; return }
 if($Case -in @('binary-clean','binary-forced')) { if($code -ne 0 -or $text -notmatch 'clean-before=1 binary-written=1') { throw "Binary-only edit on clean document was not recovered: $text" }; $passed=$true; return }
 if($Case -eq 'binary-noop') { if($code -ne 0 -or $text -notmatch 'first-saved=1 second-written=0') { throw "Unchanged binary caused a recovery write: $text" }; $passed=$true; return }
 if($Case -eq 'binary-second') { if($code -ne 0 -or $text -notmatch 'first-saved=1 second-written=1') { throw "Binary-only edit was not recovered: $text" }; $passed=$true; return }
 if($Case -in @('invalid-second','write-error')){
  $first=Join-Path $iso.Root 'first-recovery.fb2';if(!(Test-Path -LiteralPath $first)){throw 'First recovery copy missing'}
  Copy-Item -LiteralPath $first -Destination (Join-Path $dest 'invalid-second-first.fb2')
  foreach($name in @('before-invalid.html','after-invalid.html')){Copy-Item -LiteralPath (Join-Path $iso.Root $name) -Destination (Join-Path $dest $name)}
  $same=(Get-FileHash -LiteralPath $first -Algorithm SHA256).Hash -eq (Get-FileHash -LiteralPath $snapshots[0].FullName -Algorithm SHA256).Hash
  Write-Output "first-and-last-equal=$same"
  if($code -ne 0 -or !$same -or $text -notmatch 'first-saved=1 second-saved=0 second-written=0 html-same=1' -or $text -notmatch 'history-same=1 history-executed=1'){throw "Recovery refusal failed: $text"}
  $passed=$true;return
 }
 if($Case -eq 'reentrant-new' -and $text -notmatch 'reentrant-observed=1') { throw 'Reentrant New injection did not run.' }
 if($code -ne 0 -or $text -notmatch 'saved=1 wrote=1 html-same=1' -or $text -notmatch 'history-executed=1') {throw "Probe failed: $text"}
 $passed=$true
} finally {Remove-Item Env:FBE_NEXT_TEST_MODE,Env:FBE_NEXT_TEST_SCENARIO,Env:FBE_NEXT_RECOVERY_REENTERED,Env:FBE_NEXT_TRACE,Env:FBE_NEXT_FAULT_INJECT,Env:FBE_NEXT_RECOVERY_TITLE_PROBE,Env:FBE_NEXT_RECOVERY_VERIFY_CONTENT,Env:FBE_NEXT_RECOVERY_BINARY_SECOND,Env:FBE_NEXT_RECOVERY_BINARY_CLEAN,Env:FBE_NEXT_RECOVERY_FORCED,Env:FBE_NEXT_RECOVERY_BINARY_NOOP,Env:FBE_NEXT_RECOVERY_SECOND_INVALID,Env:FBE_NEXT_RECOVERY_WRITE_ERROR,Env:FBE_NEXT_SCRIPT_API_ROOT -ErrorAction SilentlyContinue;Complete-IsolatedFbeRuntime -Isolation $iso -Passed $passed}
