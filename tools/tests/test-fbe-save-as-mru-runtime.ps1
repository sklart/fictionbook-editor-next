[CmdletBinding()]
param([Parameter(Mandatory)][string]$FbeExe, [ValidateRange(30,300)][int]$TimeoutSeconds = 180)
$ErrorActionPreference='Stop'
if (-not (Test-Path -LiteralPath $FbeExe -PathType Leaf)) { throw "Не найден FBE.exe: $FbeExe" }
$root=Join-Path ([IO.Path]::GetTempPath()) ('fbe-save-mru-'+[guid]::NewGuid().ToString('N')); New-Item -ItemType Directory -Path $root -Force|Out-Null
$fbeDirectory = Split-Path -Parent (Resolve-Path -LiteralPath $FbeExe).Path
$portableIni = Join-Path $fbeDirectory 'portable.ini'
$hadPortableIni = Test-Path -LiteralPath $portableIni
$oldPortableIni = if ($hadPortableIni) { [IO.File]::ReadAllBytes($portableIni) } else { $null }
$portableData = 'SaveAsMruRuntimeData-' + [guid]::NewGuid().ToString('N')
$portableDataPath = Join-Path $fbeDirectory $portableData
try {
  [IO.File]::WriteAllText($portableIni, "[Portable]`r`nDataPath=$portableData`r`n", [Text.UTF8Encoding]::new($false))
  New-Item -ItemType Directory -Path $portableDataPath -Force | Out-Null
  $a=Join-Path $root 'A.fb2'; $b=Join-Path $root 'Книга&B.fb2'; $report=Join-Path $root 'report.txt'
  [IO.File]::WriteAllText($a,'<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><nickname>T</nickname></author><book-title>T</book-title><lang>en</lang></title-info><document-info><author><nickname>T</nickname></author><program-used>T</program-used><date value="2026-01-01">x</date><id>11111111-1111-1111-1111-111111111111</id><version>1.0</version></document-info></description><body><section><p>x</p></section></body></FictionBook>',[Text.UTF8Encoding]::new($false))
  $fail=Join-Path $root 'missing-parent\fail.fb2'; $old=@{}; foreach($n in 'FBE_NEXT_TEST_MODE','FBE_NEXT_TEST_SCENARIO','FBE_NEXT_TEST_SAVE_PATH','FBE_NEXT_TEST_SAVE_FAIL_PATH'){ $old[$n]=[Environment]::GetEnvironmentVariable($n,'Process') }; try { $env:FBE_NEXT_TEST_MODE='1';$env:FBE_NEXT_TEST_SCENARIO='save-as-mru-runtime';$env:FBE_NEXT_TEST_SAVE_PATH=$b;$env:FBE_NEXT_TEST_SAVE_FAIL_PATH=$fail; $p=Start-Process $FbeExe -ArgumentList ('--portable -b "{0}" "{1}"' -f $report,$a) -WorkingDirectory (Split-Path $FbeExe) -PassThru; if(-not $p.WaitForExit($TimeoutSeconds*1000)){Stop-Process -Id $p.Id -Force;throw 'Save As MRU runtime timed out.'};$s=Get-Content $report -Raw; foreach($x in 'saved=1','first=1','unique=1','source=1','cancel=1','failed=1','cancel_unchanged=1','failed_unchanged=1'){if($p.ExitCode -ne 0 -or $s -notmatch [regex]::Escape($x)){throw "Save As MRU runtime failed: $s"}}; $env:FBE_NEXT_TEST_SCENARIO='save-as-mru-restart-runtime';$p=Start-Process $FbeExe -ArgumentList ('--portable -b "{0}"' -f $report) -WorkingDirectory (Split-Path $FbeExe) -PassThru;if(-not $p.WaitForExit($TimeoutSeconds*1000)){Stop-Process -Id $p.Id -Force;throw 'Save As MRU restart timed out.'};$s=Get-Content $report -Raw;if($p.ExitCode -ne 0 -or $s -notmatch 'order=1'){throw "Save As MRU restart failed: $s"} } finally {foreach($n in $old.Keys){[Environment]::SetEnvironmentVariable($n,$old[$n],'Process')}}
} finally {
  if ($hadPortableIni) { [IO.File]::WriteAllBytes($portableIni, $oldPortableIni) }
  else { Remove-Item -LiteralPath $portableIni -Force -ErrorAction SilentlyContinue }
  Remove-Item -LiteralPath $portableDataPath -Recurse -Force -ErrorAction SilentlyContinue
  Remove-Item -LiteralPath $root -Recurse -Force -ErrorAction SilentlyContinue
}
Write-Host 'Save As MRU runtime passed.'
