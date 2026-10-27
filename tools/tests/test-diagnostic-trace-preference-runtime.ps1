[CmdletBinding()]
param([string]$Executable=(Join-Path $PSScriptRoot '../../out/Release/FBE.exe'))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$iso=New-IsolatedFbeRuntime -FbeExe $Executable -Name trace-preference
$passed=$false
$names=@('FBE_NEXT_TEST_MODE','FBE_NEXT_TEST_SCENARIO','FBE_NEXT_TRACE','FBE_NEXT_TEST_TRACE_EXPECTED','FBE_NEXT_TEST_TRACE_TOGGLE','FBE_NEXT_TEST_SETTINGS_DIRECTORY')
$saved=@{};foreach($name in $names){$saved[$name]=[Environment]::GetEnvironmentVariable($name)}
$evidence=Join-Path (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path ('build/trace-preference-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $evidence | Out-Null
try {
 $null=New-IsolatedFbeSettingsSeed -Isolation $iso -StartupScriptsDirectory (Join-Path $iso.Root empty-scripts)
 $fixture=Join-Path $iso.Root trace.fb2
 [IO.File]::WriteAllText($fixture,'<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><nickname>Test</nickname></author><book-title>Trace preference</book-title><lang>en</lang></title-info><document-info><id>trace-pref</id><version>1.0</version></document-info></description><body><section><p>fixture</p></section></body></FictionBook>',[Text.UTF8Encoding]::new($false))
 $env:FBE_NEXT_TEST_MODE='1';$env:FBE_NEXT_TEST_SCENARIO='diagnostic-trace-preference';$env:FBE_NEXT_TRACE=$null
 $env:FBE_NEXT_TEST_SETTINGS_DIRECTORY=Join-Path $iso.DataPath Settings
 $runs=@(@{Expected='0';Toggle='1'},@{Expected='1';Toggle='1'},@{Expected='0';Toggle='0'})
 for($run=0;$run -lt $runs.Count;++$run) {
  $env:FBE_NEXT_TEST_TRACE_EXPECTED=$runs[$run].Expected;$env:FBE_NEXT_TEST_TRACE_TOGGLE=$runs[$run].Toggle
  $report=Join-Path $iso.Root ("run-$run.txt")
  $process=Start-Process -FilePath $iso.Exe -WorkingDirectory $iso.Runtime -ArgumentList @('--portable','-b',$report,$fixture) -WindowStyle Hidden -PassThru
  $exitCode=Wait-IsolatedFbeProcess -Process $process -Isolation $iso -Scenario $env:FBE_NEXT_TEST_SCENARIO -Report $report -TimeoutSeconds 30
  $text=if(Test-Path -LiteralPath $report){Get-Content -LiteralPath $report -Raw}else{'missing'}
  if(Test-Path -LiteralPath $report){Copy-Item -LiteralPath $report -Destination $evidence}
  if($exitCode -ne 0 -or $text -notmatch '(?m)^result=PASS\r?$' -or $text -match '(?m)^[a-z_]+=0\r?$'){throw "Trace preference restart $run failed: $exitCode`n$text"}
 }
 Get-FileHash -LiteralPath $Executable | Format-List | Out-File (Join-Path $evidence executable-sha256.txt)
 $passed=$true;Write-Host "PASS: real diagnostics Toggle command, normal -> extended -> normal across three processes, stored preference, mode control, no live-mode change before restart. $evidence"
} finally {foreach($name in $names){[Environment]::SetEnvironmentVariable($name,$saved[$name])};Complete-IsolatedFbeRuntime -Isolation $iso -Passed $passed}
