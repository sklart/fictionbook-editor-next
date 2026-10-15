[CmdletBinding()]param()
$ErrorActionPreference='Stop'
$repo=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$work=Join-Path $repo ('build\tests\shell-protocol-'+[Guid]::NewGuid().ToString('N'))
$null=[IO.Directory]::CreateDirectory($work)
. (Join-Path $repo 'tools\build\shell-integration-lifecycle.ps1')
function Assert([bool]$Value,[string]$Message){if(-not $Value){throw $Message}}
$hive='Software\FBENextInstallerTests\'+[Guid]::NewGuid().ToString('N')
$compiler=& (Join-Path $repo 'tools\build\resolve-nsis.ps1')
$include=Join-Path $repo 'packaging\nsis\Installer\ShellMaintenanceStatus.nsh'
$installer=Get-Content -Raw -LiteralPath (Join-Path $repo 'packaging\nsis\Installer\MakeInstaller.nsi')
$cleanupFunction=[regex]::Match($installer,'(?s)Function \$\{prefix\}CleanupShellMaintenance.*?FunctionEnd').Value.Replace('${prefix}','').Replace('  !ifndef FBE_DEPLOYMENT_TEST_SCOPE_PROBE','').Replace('  !endif','')
Assert ([bool]$cleanupFunction) 'Production cleanup function missing.'
$cleanupFunction=$cleanupFunction.Replace('  DetailPrint "Maintenance cleanup exit=$0: $1"','  WriteINIStr "@WORK@\proof.ini" "Proof" "CleanupExit" "$0"')
$harness=@'
Unicode true
RequestExecutionLevel user
SilentInstall silent
!include LogicLib.nsh
!include FileFunc.nsh
!include x64.nsh
!define FBE_SHELL_ROOT "@WORK@"
!include "@INCLUDE@"
!define PRODUCT_UNINST_ROOT_KEY HKCU
!define PRODUCT_UNINST_KEY "@HIVE@"
!define PRODUCT_SYSTEM_INTEGRATION_REGVAL "SystemIntegrationInstalled"
OutFile "@WORK@\protocol.exe"
Var ShellStatusValid
Var ShellToolsDirectory
Var ShellPayloadResult
Var ShellOperationCode
Var ShellEffectiveInstalled
Var ShellCleanupPending
Var ShellRecoveryPending
Var ShellSystemInstalled
Section
  StrCpy $ShellToolsDirectory "@WORK@"
  ${GetParameters} $0
  ${GetOptions} $0 "/EXIT=" $9
  !insertmacro FBE_ReadShellMaintenanceStatus
  !insertmacro FBE_ApplyShellMaintenanceStatus
  WriteINIStr "@WORK@\proof.ini" "Proof" "Valid" "$ShellStatusValid"
  WriteINIStr "@WORK@\proof.ini" "Proof" "Code" "$ShellOperationCode"
  ${GetOptions} $0 "/CLEANUP" $2
  ${IfNot} ${Errors}
    StrCpy $ShellToolsDirectory "@WORK@\Maintenance\nsis-current"
    SetErrorLevel 55
    Call CleanupShellMaintenance
    GetErrorLevel $2
    WriteINIStr "@WORK@\proof.ini" "Proof" "PreservedExit" "$2"
    SetErrorLevel 0
  ${EndIf}
SectionEnd
@CLEANUP_FUNCTION@
'@
$harness=$harness.Replace('@CLEANUP_FUNCTION@',$cleanupFunction).Replace('@INCLUDE@',$include).Replace('@HIVE@',$hive).Replace('@WORK@',$work)
$source=Join-Path $work 'protocol.nsi';[IO.File]::WriteAllText($source,$harness,[Text.Encoding]::UTF8)
& $compiler /V2 $source
Assert ($LASTEXITCODE -eq 0) 'Protocol harness compile failed.'
$status=Join-Path $work 'Status.ini'
$final=New-ShellResult 'ok' 'INSTALLED' $true $false $false 'done'
function Check-Reply([string]$Name,[int]$ExitCode,[bool]$Valid){
    $key=[Microsoft.Win32.Registry]::CurrentUser.CreateSubKey($hive)
    try{foreach($field in @('PropertyHandlerInstalled','SystemIntegrationInstalled','ShellCleanupPending')){$key.SetValue($field,1,[Microsoft.Win32.RegistryValueKind]::DWord)};$key.SetValue('ShellRecoveryPending',0,[Microsoft.Win32.RegistryValueKind]::DWord)}finally{$key.Close()}
    $process=Start-Process -FilePath (Join-Path $work 'protocol.exe') -ArgumentList @('/S',('/EXIT='+$ExitCode)) -Wait -PassThru -WindowStyle Hidden
    Assert ($process.ExitCode -eq 0) "Harness failed: $Name"
    $proof=[IO.File]::ReadAllText((Join-Path $work 'proof.ini'))
    Assert ($proof -match ('Valid='+[int]$Valid)) "Wrong protocol verdict: $Name; $proof"
    $key=[Microsoft.Win32.Registry]::CurrentUser.OpenSubKey($hive)
    try {
        $expected=@{PropertyHandlerInstalled=1;SystemIntegrationInstalled=1;ShellCleanupPending=1;ShellRecoveryPending=1}
        if($Valid){
            $values=@{};foreach($line in [IO.File]::ReadAllLines($status)){$split=$line.IndexOf('=');if($split -gt 0){$values[$line.Substring(0,$split)]=$line.Substring($split+1)}}
            $expected=@{PropertyHandlerInstalled=[int]$values.EffectiveInstalled;SystemIntegrationInstalled=[int]$values.SystemIntegrationInstalled;ShellCleanupPending=[int]$values.CleanupPending;ShellRecoveryPending=[int]$values.RecoveryPending}
        }
        foreach($field in $expected.Keys){Assert ($key.GetValue($field) -eq $expected[$field]) "Confirmed state lost: $Name / $field"}
    }finally{$key.Close()}
}
try {
    Write-ShellFinalStatus $status $final;Check-Reply 'complete ok' 0 $true
    [IO.File]::Delete($status);Check-Reply 'missing status' 0 $false
    [IO.File]::WriteAllText($status,'broken status');Check-Reply 'corrupt status' 0 $false
    $worker=Join-Path $work 'crash-worker.ps1';$ready=Join-Path $work 'ready'
    [IO.File]::WriteAllText($worker,"param([string]`$Status,[string]`$Ready)`r`n[IO.File]::WriteAllLines(`$Status,[string[]]@('[Shell]','Protocol=1','Phase=starting','Code=STARTING'),[Text.Encoding]::Unicode)`r`n[IO.File]::WriteAllText(`$Ready,'ready')`r`nStart-Sleep -Seconds 20",[Text.Encoding]::UTF8)
    $workerProcess=Start-Process -FilePath "$env:WINDIR\System32\WindowsPowerShell\v1.0\powershell.exe" -ArgumentList @('-NoProfile','-ExecutionPolicy','Bypass','-File',('"'+$worker+'"'),'-Status',('"'+$status+'"'),'-Ready',('"'+$ready+'"')) -PassThru -WindowStyle Hidden
    try {for($i=0;$i -lt 100 -and -not [IO.File]::Exists($ready);++$i){Start-Sleep -Milliseconds 50};Assert ([IO.File]::Exists($ready)) 'Worker did not reach STARTING.';$workerProcess.Kill();$workerProcess.WaitForExit();Check-Reply 'killed after STARTING' $workerProcess.ExitCode $false}finally{if(-not $workerProcess.HasExited){$workerProcess.Kill();$workerProcess.WaitForExit()};$workerProcess.Close()}
    [IO.File]::WriteAllLines($status,[string[]]@('[Shell]','Result=pending','Code=STARTING'),[Text.Encoding]::Unicode);Check-Reply 'old STARTING pending' 0 $false
    foreach($field in @('Result','Protocol','Phase','ProcessExitCode','Code','EffectiveInstalled','CleanupPending','RecoveryPending','SystemIntegrationInstalled')){
        Write-ShellFinalStatus $status $final
        $lines=@([IO.File]::ReadAllLines($status) | Where-Object {-not $_.StartsWith($field+'=')});[IO.File]::WriteAllLines($status,[string[]]$lines,[Text.Encoding]::Unicode)
        Check-Reply ('missing '+$field) 0 $false
    }
    foreach($resultName in @('ok','pending')){
        $reply=New-ShellResult $resultName 'FILES_IN_USE' $true $false $false 'test';Write-ShellFinalStatus $status $reply;Check-Reply ('nonzero '+$resultName) 9 $false
    }
    Write-ShellFinalStatus $status $final;[IO.File]::WriteAllText($status,([IO.File]::ReadAllText($status).Replace('SystemIntegrationInstalled=1','SystemIntegrationInstalled=0')),[Text.Encoding]::Unicode);Check-Reply 'inconsistent flags' 0 $false
    $reply=New-ShellResult 'pending' 'FILES_IN_USE' $true $false $false 'confirmed wait';Write-ShellFinalStatus $status $reply;Check-Reply 'confirmed pending' 0 $true
    $reply=New-ShellResult 'error' 'UPDATE_FAILED' $true $false $false 'rollback completed';Write-ShellFinalStatus $status $reply;Check-Reply 'handled error final' 0 $true

    $reply=New-ShellResult 'pending' 'CLEANUP_PENDING' $false $true $false 'confirmed cleanup';Write-ShellFinalStatus $status $reply;Check-Reply 'confirmed cleanup' 0 $true
    $reply=New-ShellResult 'error' 'RECOVERY_REQUIRED' $true $true $true 'uncertain cleanup';Write-ShellFinalStatus $status $reply;Check-Reply 'confirmed recovery flags' 0 $true
    $reply=New-ShellResult 'pending' 'CLEANUP_PENDING' $false $false $false 'inconsistent cleanup';Write-ShellFinalStatus $status $reply;Check-Reply 'inconsistent cleanup code' 0 $false
    $reply=New-ShellResult 'ok' 'INSTALLED' $false $false $false 'inconsistent installed';Write-ShellFinalStatus $status $reply;Check-Reply 'inconsistent installed code' 0 $false
    $reply=New-ShellResult 'ok' 'UNKNOWN' $true $false $false 'unknown code';Write-ShellFinalStatus $status $reply;Check-Reply 'unknown result code' 0 $false
    $cliStatus=Join-Path $work 'cli.ini'
    & "$env:WINDIR\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -File (Join-Path $repo 'tools\build\manage-shell-integration.ps1') -Operation Update -Directory (Join-Path $work 'Shared') -InstallationDirectory (Join-Path $work 'Application') -PreviousProperty 1 -StatusFilePath $cliStatus
    Assert ($LASTEXITCODE -eq 0) 'Handled initialization error did not deliver its final protocol record.'
    Assert ([IO.File]::ReadAllText($cliStatus) -match 'Phase=complete' -and [IO.File]::ReadAllText($cliStatus) -match 'Result=error') 'CLI final error was not published.'
    Copy-Item -LiteralPath $cliStatus -Destination $status;Check-Reply 'actual CLI final error' 0 $true

    $maintenance=Join-Path $work 'Maintenance';$null=[IO.Directory]::CreateDirectory($maintenance)
    function New-Session([string]$Name,[int]$Launcher=2147483647){
        $dir=Join-Path $maintenance $Name;$null=[IO.Directory]::CreateDirectory($dir)
        [IO.File]::WriteAllLines((Join-Path $dir 'Session.ini'),[string[]]@('[Maintenance]','Version=1',('Directory='+$dir),'Owner=FBE Next Shell Maintenance',('LauncherPid='+$Launcher)),[Text.Encoding]::Unicode)
        [IO.File]::WriteAllText((Join-Path $dir 'Status.ini'),'test');return $dir
    }
    $success=New-Session 'complete';Remove-ShellMaintenanceSessions $maintenance $success;Assert (-not [IO.Directory]::Exists($success)) 'Completed session retained.'
    $stale=New-Session 'crashed';Remove-ShellMaintenanceSessions $maintenance;Assert (-not [IO.Directory]::Exists($stale)) 'Crashed session retained.'
    $active=New-Session 'active' $PID;Remove-ShellMaintenanceSessions $maintenance;Assert ([IO.Directory]::Exists($active)) 'Active launcher session deleted.'
    $lease=Open-ShellMaintenanceLease $active
    try {Remove-ShellMaintenanceSessions $maintenance $active;Assert ([IO.File]::Exists((Join-Path $active 'Status.ini'))) 'Running worker files deleted.'}finally{$lease.Close()}
    Remove-ShellMaintenanceSessions $maintenance $active;Assert (-not [IO.Directory]::Exists($active)) 'Released active session retained.'
    $locked=New-Session 'locked';$lock=[IO.File]::OpenRead((Join-Path $locked 'Status.ini'))
    try {Remove-ShellMaintenanceSessions $maintenance;Assert ([IO.File]::Exists((Join-Path $locked 'Session.ini'))) 'Cleanup failure lost ownership marker.'}finally{$lock.Close()}
    Remove-ShellMaintenanceSessions $maintenance;Assert (-not [IO.Directory]::Exists($locked)) 'Cleanup retry failed.'
    $foreign=New-Session 'foreign';[IO.File]::WriteAllText((Join-Path $foreign 'unrelated.txt'),'keep');Remove-ShellMaintenanceSessions $maintenance;Assert ([IO.File]::Exists((Join-Path $foreign 'Status.ini'))) 'Unknown files caused partial cleanup.'
    $unmarked=Join-Path $maintenance 'unmarked';$null=[IO.Directory]::CreateDirectory($unmarked);[IO.File]::WriteAllText((Join-Path $unmarked 'Status.ini'),'keep');Remove-ShellMaintenanceSessions $maintenance;Assert ([IO.Directory]::Exists($unmarked)) 'Unmarked directory deleted.'
    $state=Join-Path $work 'Shell\State';$null=[IO.Directory]::CreateDirectory($state);$journal=Join-Path $state 'Transaction.clixml';[IO.File]::WriteAllText($journal,'keep recovery');Remove-ShellMaintenanceSessions $maintenance;Assert ([IO.File]::ReadAllText($journal) -eq 'keep recovery') 'Permanent journal changed.'
    $outside=Join-Path $work 'outside';$null=[IO.Directory]::CreateDirectory($outside);[IO.File]::WriteAllText((Join-Path $outside 'keep.txt'),'keep')
    $junction=Join-Path $maintenance 'junction';New-Item -ItemType Junction -Path $junction -Target $outside | Out-Null
    Remove-ShellMaintenanceSessions $maintenance;Assert ([IO.File]::ReadAllText((Join-Path $outside 'keep.txt')) -eq 'keep') 'Junction target changed.'
    $current=New-Session 'nsis-current';Copy-Item -LiteralPath (Join-Path $repo 'tools\build\shell-integration-lifecycle.ps1') -Destination $current
    $lockedStatus=[IO.File]::OpenRead((Join-Path $current 'Status.ini'))
    try {
        $process=Start-Process -FilePath (Join-Path $work 'protocol.exe') -ArgumentList @('/S','/EXIT=0','/CLEANUP') -Wait -PassThru -WindowStyle Hidden
        Assert ($process.ExitCode -eq 0 -and [IO.Directory]::Exists($current)) 'NSIS failed cleanup changed the operation result or deleted a locked session.'
        Assert ([IO.File]::ReadAllText((Join-Path $work 'proof.ini')) -match 'PreservedExit=55') 'Cleanup changed the preceding NSIS result.'
    }finally{$lockedStatus.Close()}
    $process=Start-Process -FilePath (Join-Path $work 'protocol.exe') -ArgumentList @('/S','/EXIT=0','/CLEANUP') -Wait -PassThru -WindowStyle Hidden
    Assert ($process.ExitCode -eq 0 -and -not [IO.Directory]::Exists($current)) 'Actual NSIS cleanup command failed.'
    Assert ([IO.File]::Exists($journal)) 'Actual NSIS cleanup touched the recovery journal.'
    Write-Host "Shell protocol and maintenance cleanup checks passed. Artifacts: $work"
}finally{[Microsoft.Win32.Registry]::CurrentUser.DeleteSubKeyTree($hive)}
