[CmdletBinding()]
param([string]$FbeExe=(Join-Path $PSScriptRoot '../../out/Release/FBE.exe'))
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$iso=New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'description-settings-caret'
$passed=$false
$names=@('FBE_NEXT_TEST_MODE','FBE_NEXT_TEST_SCENARIO')
$old=@{}; foreach($name in $names){$old[$name]=[Environment]::GetEnvironmentVariable($name,'Process')}
try {
 $null=New-IsolatedFbeSettingsSeed $iso -StartupScriptsDirectory (Join-Path $iso.Root 'ProbeScripts')
 $env:FBE_NEXT_TEST_MODE='1'; $env:FBE_NEXT_TEST_SCENARIO='description-settings-caret'
 $report=Join-Path $iso.Root 'report.txt'
 $book=Join-Path $iso.Root 'book.fb2'
 Copy-Item (Join-Path $PSScriptRoot 'fixtures/description/full.fb2') $book
 $process=Start-Process $iso.Exe -WorkingDirectory $iso.Root -ArgumentList @('--portable','-b',$report,$book) -WindowStyle Hidden -PassThru
 $code=Wait-IsolatedFbeProcess -Process $process -Isolation $iso -Scenario 'description-settings-caret' -Report $report -TimeoutSeconds 45
 if($code -ne 0 -or -not(Test-Path $report) -or -not(Select-String -LiteralPath $report -Pattern '^description-settings-caret=pass$' -Quiet)){throw 'Description settings caret regression failed.'}
 $passed=$true; Write-Host 'Description settings caret runtime PASS.'
} finally {
 foreach($name in $names){[Environment]::SetEnvironmentVariable($name,$old[$name],'Process')}
 Complete-IsolatedFbeRuntime -Isolation $iso -Passed $passed
}
