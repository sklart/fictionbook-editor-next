[CmdletBinding()]
param([Parameter(Mandatory)][string]$FbeExe,[int]$TimeoutSeconds=60)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$isolation=New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'quality-icon-theme';$passed=$false
$oldMode=$env:FBE_NEXT_TEST_MODE;$oldScenario=$env:FBE_NEXT_TEST_SCENARIO
try {
 $null=New-IsolatedFbeSettingsSeed -Isolation $isolation
 $fixture=Join-Path $isolation.Root 'fixture.fb2';$report=Join-Path $isolation.Root 'report.txt'
 '<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>Icon</book-title><lang>en</lang></title-info><document-info><id>icon-test</id><version>1.0</version></document-info></description><body><section><p>Icon</p></section></body></FictionBook>' | Set-Content -LiteralPath $fixture -Encoding UTF8
 $env:FBE_NEXT_TEST_MODE='1';$env:FBE_NEXT_TEST_SCENARIO='quality-icon-theme-runtime'
 $process=Start-Process $isolation.Exe -WorkingDirectory $isolation.Root -ArgumentList @('--portable','-b',$report,$fixture) -WindowStyle Hidden -PassThru
 $code=Wait-IsolatedFbeProcess -Process $process -Isolation $isolation -Scenario 'quality-icon-theme-runtime' -Report $report -TimeoutSeconds $TimeoutSeconds
 $text=Get-Content -LiteralPath $report -Raw; if($code -ne 0 -or $text -notmatch 'result=PASS'){throw "Quality icon scenario failed: $text"}
 $passed=$true;Write-Output $text
} finally {$env:FBE_NEXT_TEST_MODE=$oldMode;$env:FBE_NEXT_TEST_SCENARIO=$oldScenario;Complete-IsolatedFbeRuntime -Isolation $isolation -Passed $passed}
