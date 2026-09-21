[CmdletBinding()]
param([string]$FbeExe = (Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe'), [int]$TimeoutSeconds = 90)
$ErrorActionPreference = 'Stop'
$dir = Join-Path ([IO.Path]::GetTempPath()) ('fbe-spell-scroll-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $dir | Out-Null
try {
  $fixture = Join-Path $dir 'scroll.fb2'; $report = Join-Path $dir 'scroll.tsv'
  $body = (1..2000 | ForEach-Object { "<p>scroll spell paragraph $_</p>" }) -join ''
  "<?xml version=`"1.0`" encoding=`"utf-8`"?><FictionBook xmlns=`"http://www.gribuser.ru/xml/fictionbook/2.0`"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>scroll</book-title><lang>en</lang></title-info><document-info><program-used>test</program-used><id>scroll</id><version>1.0</version></document-info></description><body><section>$body</section></body></FictionBook>" | Set-Content -LiteralPath $fixture -Encoding utf8
  $oldMode,$oldScenario=$env:FBE_NEXT_TEST_MODE,$env:FBE_NEXT_TEST_SCENARIO; $env:FBE_NEXT_TEST_MODE='1';$env:FBE_NEXT_TEST_SCENARIO='spellcheck-scroll'
  try { $p=Start-Process -FilePath $FbeExe -ArgumentList @('-b',$report,$fixture) -PassThru; if(!$p.WaitForExit($TimeoutSeconds*1000)){Stop-Process $p -Force;throw 'spell scroll runtime timed out'} } finally {$env:FBE_NEXT_TEST_MODE,$env:FBE_NEXT_TEST_SCENARIO=$oldMode,$oldScenario}
  if($p.ExitCode -ne 0 -or !(Test-Path $report)){throw 'spell scroll runtime failed'}; $r=@{};Get-Content $report|%{$x=$_-split "`t",2;if($x.Count-eq 2){$r[$x[0]]=[UInt64]$x[1]}}
  if($r.spell_scroll_checks -lt 1 -or $r.command_state_updates -ne 0 -or $r.selection_context_builds -ne 0 -or $r.toolbar_updates -ne 0){throw "scroll metrics invalid: $($r|ConvertTo-Json -Compress)"}; Write-Host 'Spellcheck scroll runtime passed.'
} finally {Remove-Item $dir -Recurse -Force -ErrorAction SilentlyContinue}
