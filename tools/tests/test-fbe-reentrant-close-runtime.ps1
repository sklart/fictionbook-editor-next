[CmdletBinding()]
param([Parameter(Mandatory)][string]$FbeExe, [ValidateRange(30,180)][int]$TimeoutSeconds=90)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
foreach($operation in @('close','new','open')) {
 $iso=New-IsolatedFbeRuntime -FbeExe $FbeExe -Name "reentrant-$operation"; $passed=$false
 try {
  $null=New-IsolatedFbeSettingsSeed -Isolation $iso -StartupScriptsDirectory (Join-Path $iso.Root 'empty-scripts')
  $fixture=Join-Path $iso.Root 'input.fb2'; $other=Join-Path $iso.Root 'other.fb2'; $first=Join-Path $iso.Root 'first.fb2'; $report=Join-Path $iso.Root 'report.txt'
  $xml='<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><nickname>Test</nickname></author><book-title>Binary dirty</book-title><lang>en</lang></title-info><document-info><author><nickname>Test</nickname></author><date value="2026-01-01">2026</date><id>binary-dirty</id><version>1.0</version></document-info></description><body><section><p>Text</p></section></body><binary id="payload.bin" content-type="application/octet-stream">AAECA/7/</binary></FictionBook>'
  [IO.File]::WriteAllText($fixture,$xml,[Text.UTF8Encoding]::new($false)); [IO.File]::WriteAllText($other,$xml,[Text.UTF8Encoding]::new($false))
  $env:FBE_NEXT_TEST_MODE='1'; $env:FBE_NEXT_TEST_SCENARIO="save-decision-reentrant-$operation"; $env:FBE_NEXT_TEST_DECISION_OPEN_PATH=$other; $env:FBE_NEXT_TEST_FIRST_SAVE_PATH=$first
  $proc=Start-Process -FilePath $iso.Exe -WorkingDirectory $iso.Runtime -WindowStyle Hidden -ArgumentList @('--portable','-b',('"'+$report+'"'),('"'+$fixture+'"')) -PassThru
  $code=Wait-IsolatedFbeProcess -Process $proc -Isolation $iso -Scenario $env:FBE_NEXT_TEST_SCENARIO -Report $report -TimeoutSeconds $TimeoutSeconds
  $state=if(Test-Path $report){Get-Content $report -Raw}else{'<missing>'}; Write-Host "$operation exit=$code $state"; if($code -ne 0 -or $state -notmatch 'dirty-before=1 dialog=1 answered=1 cancelled=1 retained=1 copied=1 saved=1 reopened=1 binary-same=1 closed=1'){throw "Operation failed or report missing: $operation"}
  foreach($item in @(@($first,'CQgHBg=='),@($fixture,'BQQDAg=='))) {
   $doc=[Xml.XmlDocument]::new(); $doc.XmlResolver=$null; $doc.Load($item[0]); $ns=[Xml.XmlNamespaceManager]::new($doc.NameTable); $ns.AddNamespace('f','http://www.gribuser.ru/xml/fictionbook/2.0'); $nodes=$doc.SelectNodes('/f:FictionBook/f:binary',$ns)
   if($nodes.Count -ne 1 -or $nodes[0].GetAttribute('id') -cne 'payload.bin' -or $nodes[0].GetAttribute('content-type') -cne 'application/octet-stream' -or [Convert]::ToBase64String([Convert]::FromBase64String($nodes[0].InnerText)) -cne $item[1]) { throw "Wrong binary in $($item[0])" }
  }
  $passed=$true
 } finally { Remove-Item Env:FBE_NEXT_TEST_MODE,Env:FBE_NEXT_TEST_SCENARIO,Env:FBE_NEXT_TEST_DECISION_OPEN_PATH,Env:FBE_NEXT_TEST_FIRST_SAVE_PATH -ErrorAction SilentlyContinue; Complete-IsolatedFbeRuntime -Isolation $iso -Passed $passed }
}
Write-Host 'Reentrant Close/New/Open and Save/Reopen: PASS.'