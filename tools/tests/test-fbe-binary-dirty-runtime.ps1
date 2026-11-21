[CmdletBinding()]
param([Parameter(Mandatory)][string]$FbeExe, [ValidateRange(30,180)][int]$TimeoutSeconds=90, [string]$Case)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$cases=@('read','validate','recovery-clean','replace','id','mime','delete','add','save-error','reentrant-save')
if ($Case) { if ($cases -notcontains $Case) { throw "Unknown case: $Case" }; $cases=@($Case) }
foreach($case in $cases) {
 $iso=New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'binary-dirty'; $passed=$false
 try {
  $null=New-IsolatedFbeSettingsSeed -Isolation $iso -StartupScriptsDirectory (Join-Path $iso.Root 'empty-scripts')
  $fixture=Join-Path $iso.Root 'input.fb2'; $report=Join-Path $iso.Root 'report.txt'
  $xml='<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><nickname>Test</nickname></author><book-title>Binary dirty</book-title><lang>en</lang></title-info><document-info><author><nickname>Test</nickname></author><date value="2026-01-01">2026</date><id>binary-dirty</id><version>1.0</version></document-info></description><body><section><p>Text</p></section></body><binary id="payload.bin" content-type="application/octet-stream">AAECA/7/</binary></FictionBook>'
  [IO.File]::WriteAllText($fixture,$xml,[Text.UTF8Encoding]::new($false)); $before=(Get-FileHash $fixture).Hash
  $env:FBE_NEXT_TEST_MODE='1'; $env:FBE_NEXT_TEST_SCENARIO='binary-dirty-contract'; $env:FBE_NEXT_BINARY_DIRTY_CASE=$case
  $process=Start-Process -FilePath $iso.Exe -WorkingDirectory $iso.Runtime -WindowStyle Hidden -ArgumentList @('--portable','-b',('"'+$report+'"'),('"'+$fixture+'"')) -PassThru
  $code=Wait-IsolatedFbeProcess -Process $process -Isolation $iso -Scenario 'binary-dirty-contract' -Report $report -TimeoutSeconds $TimeoutSeconds
  $state=if(Test-Path $report){Get-Content $report -Raw}else{'<missing>'}; Write-Host "case=$case exit=$code $state"
  if($code -ne 0 -or $state -notmatch 'clean-before=1 changed=[01] saved=[01] save-failed=[01] clean-after=[01]') { throw "Binary dirty failed or report missing: $case $state" }
  if($case -eq 'save-error' -and (Get-FileHash $fixture).Hash -ne $before) { throw 'Failed Save changed the original file.' }
  if($case -in @('replace','id','mime','delete','add','reentrant-save')) {
   $doc=[Xml.XmlDocument]::new();$doc.XmlResolver=$null;$doc.Load($fixture);$ns=[Xml.XmlNamespaceManager]::new($doc.NameTable);$ns.AddNamespace('f','http://www.gribuser.ru/xml/fictionbook/2.0')
   $nodes=$doc.SelectNodes('/f:FictionBook/f:binary',$ns)
   if($case -eq 'delete') { if($nodes.Count -ne 0) { throw 'Removed binary survived Save.' } }
   else {
    $id=if($case -eq 'id'){'changed.bin'}elseif($case -eq 'add'){'added.bin'}else{'payload.bin'}
    $node=$doc.SelectSingleNode("/f:FictionBook/f:binary[@id='$id']",$ns)
    if(!$node) { throw "Saved binary missing: $id" }
    $mime=if($case -eq 'mime'){'image/example'}else{'application/octet-stream'}
    $value=if($case -in @('replace','add')){'CQgHBg=='}else{'AAECA/7/'}
    if($node.GetAttribute('content-type') -cne $mime -or [Convert]::ToBase64String([Convert]::FromBase64String($node.InnerText)) -cne $value) { throw "Saved binary content or MIME changed: $case" }
   }
  }
  $passed=$true
 } finally {
  Remove-Item Env:FBE_NEXT_TEST_MODE,Env:FBE_NEXT_TEST_SCENARIO,Env:FBE_NEXT_BINARY_DIRTY_CASE -ErrorAction SilentlyContinue
  Complete-IsolatedFbeRuntime -Isolation $iso -Passed $passed
 }
}
Write-Host 'Binary dirty contract passed.'
