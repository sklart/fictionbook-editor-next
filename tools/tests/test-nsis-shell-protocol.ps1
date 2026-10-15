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
$stageFunction=[regex]::Match($installer,'(?s)Function \$\{prefix\}StageShellMaintenance.*?FunctionEnd').Value.Replace('${prefix}','').Replace('  !ifndef FBE_DEPLOYMENT_TEST_SCOPE_PROBE','')
$stageFunction=[regex]::Replace($stageFunction,'(?m)^  !endif\r?\n(?=FunctionEnd)','')
$ordinaryMacro=[regex]::Match($installer,'(?s)!macro FBE_RequireOrdinaryPath .*?!macroend').Value
Assert ([bool]$stageFunction -and [bool]$ordinaryMacro) 'Production stage function missing.'
$input=Join-Path $work 'Input';$null=[IO.Directory]::CreateDirectory((Join-Path $input 'InstallerTools'))
foreach($name in @('manage-shell-integration.ps1','shell-integration-lifecycle.ps1','update-shell-files.ps1','register-modern-property-handler.ps1','register-sequence-property-schema.ps1','unregister-modern-property-handler.ps1')){[IO.File]::WriteAllText((Join-Path $input ('InstallerTools\'+$name)),'fixture')}
foreach($name in $script:ShellPayloadNames){[IO.File]::WriteAllText((Join-Path $input $name),'fixture')}
# The injected branches create concrete I/O failures around the actual stage instructions.
$stageFunction=$stageFunction.Replace('  GetTempFileName $ShellToolsDirectory "${FBE_SHELL_ROOT}\Maintenance"',@"
  `${If} `$StageFault == "locked-temp"
    StrCpy `$ShellToolsDirectory "@WORK@\Maintenance\locked.tmp"
  `${ElseIf} `$StageFault == "temp-failure"
    GetTempFileName `$ShellToolsDirectory "@WORK@\missing\parent"
  `${Else}
    GetTempFileName `$ShellToolsDirectory "`${FBE_SHELL_ROOT}\Maintenance"
  `${EndIf}
"@)
$stageFunction=$stageFunction.Replace("  System::Call 'kernel32::CreateDirectoryW(w `"`$ShellToolsDirectory`", p 0) i .r0'",@"
  `${If} `$StageFault == "directory-failure"
    FileOpen `$0 "`$ShellToolsDirectory" w
    FileWrite `$0 "foreign sentinel"
    FileClose `$0
  `${EndIf}
  System::Call 'kernel32::CreateDirectoryW(w "`$ShellToolsDirectory", p 0) i .r0'
"@)
$stageFunction=$stageFunction.Replace('  FileOpen $ShellStageHandle "$ShellToolsDirectory\Session.ini.next" w',@"
  `${If} `$StageFault == "open-marker"
    FileOpen `$ShellStageHandle "@WORK@\locked-marker.ini" w
  `${ElseIf} `$StageFault == "write-marker"
    FileOpen `$0 "`$ShellToolsDirectory\Session.ini.next" w
    FileClose `$0
    FileOpen `$ShellStageHandle "`$ShellToolsDirectory\Session.ini.next" r
  `${Else}
    FileOpen `$ShellStageHandle "`$ShellToolsDirectory\Session.ini.next" w
  `${EndIf}
"@)
$stageFunction=$stageFunction.Replace('  StrCpy $ShellStageStep "close session marker"',@"
  `${If} `$StageFault == "close-marker"
    FileClose `$ShellStageHandle
  `${EndIf}
  StrCpy `$ShellStageStep "close session marker"
"@)
$stageFunction=$stageFunction.Replace('  File "${INPUTDIR}\FBE.Sequence.propdesc"',@"
  File "`${INPUTDIR}\FBE.Sequence.propdesc"
  `${If} `$StageFault == "extract-failure"
    SetErrors
  `${EndIf}
"@)
# The message is suppressed only in the isolated harness.
$stageFunction=$stageFunction.Replace('  MessageBox MB_OK|MB_ICONEXCLAMATION "$(ShellMaintenancePrepareFailed)" /SD IDOK','')
$harness=@'
Unicode true
RequestExecutionLevel user
SilentInstall silent
!include LogicLib.nsh
!include FileFunc.nsh
!include x64.nsh
!define FBE_SHELL_ROOT "@WORK@"
!define INPUTDIR "@WORK@\Input"
LangString UnsafeShellPayloadLocation 1033 "Unsafe maintenance path"
@ORDINARY_MACRO@
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
Var ShellStageReady
Var ShellStageCreated
Var ShellStageHandle
Var ShellStageStep
Var ShellRetainUninstaller
Var ShellPreviousLocation
Var ShellPreviousProperty
Var ShellPreviousSystem
Var StageFault
Section
  StrCpy $ShellToolsDirectory "@WORK@"
  ${GetParameters} $0
  ${GetOptions} $0 "/STAGE=" $StageFault
  ${IfNot} ${Errors}
    StrCpy $INSTDIR "@WORK@"
    Call StageShellMaintenance
    WriteINIStr "@WORK@\proof.ini" "Stage" "Ready" "$ShellStageReady"
    WriteINIStr "@WORK@\proof.ini" "Stage" "Path" "$ShellToolsDirectory"
    WriteINIStr "@WORK@\proof.ini" "Stage" "Step" "$ShellStageStep"
    WriteINIStr "@WORK@\proof.ini" "Stage" "Retain" "$ShellRetainUninstaller"
    Goto protocol_done
  ${EndIf}
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
protocol_done:
SectionEnd
@STAGE_FUNCTION@
@CLEANUP_FUNCTION@
'@
$harness=$harness.Replace('@STAGE_FUNCTION@',$stageFunction).Replace('@ORDINARY_MACRO@',$ordinaryMacro).Replace('@CLEANUP_FUNCTION@',$cleanupFunction).Replace('@INCLUDE@',$include).Replace('@HIVE@',$hive).Replace('@WORK@',$work)
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
    function New-Session([string]$Name,[int]$Launcher=2147483647,[string]$Created=''){
        if(-not $Created){if($Launcher -eq $PID){$Created=[Diagnostics.Process]::GetCurrentProcess().StartTime.ToUniversalTime().ToFileTimeUtc().ToString()}else{$Created=[DateTime]::UtcNow.ToFileTimeUtc().ToString()}}
        $dir=Join-Path $maintenance $Name;$null=[IO.Directory]::CreateDirectory($dir)
        [IO.File]::WriteAllLines((Join-Path $dir 'Session.ini'),[string[]]@('[Maintenance]','Version=2','Phase=complete',('LaunchId='+[Guid]::NewGuid().ToString('B')),('LauncherCreatedUtcFileTime='+$Created),('Directory='+$dir),'Owner=FBE Next Shell Maintenance',('LauncherPid='+$Launcher)),[Text.Encoding]::Unicode)
        [IO.File]::WriteAllText((Join-Path $dir 'Status.ini'),'test');return $dir
    }
    $success=New-Session 'complete';Remove-ShellMaintenanceSessions $maintenance $success;Assert (-not [IO.Directory]::Exists($success)) 'Completed session retained.'
    $stale=New-Session 'crashed';Remove-ShellMaintenanceSessions $maintenance;Assert (-not [IO.Directory]::Exists($stale)) 'Crashed session retained.'
    $reused=New-Session 'reused-pid' $PID ([DateTime]::UtcNow.AddDays(-1).ToFileTimeUtc().ToString());Remove-ShellMaintenanceSessions $maintenance;Assert (-not [IO.Directory]::Exists($reused)) 'Reused PID kept a completed session.'
    $legacy=New-Session 'legacy';$legacyMarker=Join-Path $legacy 'Session.ini';[IO.File]::WriteAllLines($legacyMarker,[string[]]@('[Maintenance]','Version=1',('Directory='+$legacy),'Owner=FBE Next Shell Maintenance','LauncherPid=2147483647'),[Text.Encoding]::Unicode)
    Remove-ShellMaintenanceSessions $maintenance $legacy;Assert ([IO.Directory]::Exists($legacy)) 'Old marker without process creation time was deleted.'
    $partial=New-Session 'partial';$partialMarker=Join-Path $partial 'Session.ini';[IO.File]::WriteAllText($partialMarker,[IO.File]::ReadAllText($partialMarker).Replace('Phase=complete','Phase=preparing'),[Text.Encoding]::Unicode)
    Remove-ShellMaintenanceSessions $maintenance $partial;Assert ([IO.Directory]::Exists($partial)) 'Partial session marker was accepted.'
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
    $owner=Join-Path $work 'Shell\Owner.ini';[IO.File]::WriteAllText($owner,'confirmed owner');$ownerHash=Get-ShellHash $owner;$journalHash=Get-ShellHash $journal
    function Check-Stage([string]$Fault,[bool]$Ready){
        $key=[Microsoft.Win32.Registry]::CurrentUser.CreateSubKey($hive)
        try{foreach($field in @('PropertyHandlerInstalled','SystemIntegrationInstalled','ShellCleanupPending','ShellRecoveryPending')){$key.SetValue($field,1,[Microsoft.Win32.RegistryValueKind]::DWord)}}finally{$key.Close()}
        $process=Start-Process -FilePath (Join-Path $work 'protocol.exe') -ArgumentList @('/S',('/STAGE='+$Fault)) -Wait -PassThru -WindowStyle Hidden
        Assert ($process.ExitCode -eq $(if($Ready){0}else{5})) "Wrong stage result: $Fault / $($process.ExitCode)"
        $proof=@{};foreach($line in [IO.File]::ReadAllLines((Join-Path $work 'proof.ini'))){$i=$line.IndexOf('=');if($i -gt 0){$proof[$line.Substring(0,$i)]=$line.Substring($i+1)}}
        Assert ($proof.Ready -eq [string][int]$Ready) "Stage readiness mismatch: $Fault"
        if($Ready){
            $marker=Join-Path $proof.Path 'Session.ini';$record=@{};foreach($line in [IO.File]::ReadAllLines($marker)){$i=$line.IndexOf('=');if($i -gt 0){$record[$line.Substring(0,$i)]=$line.Substring($i+1)}}
            Assert ($record.Phase -eq 'complete' -and $record.Version -eq '2' -and $record.LauncherPid -eq [string]$process.Id) 'Native stage did not publish a complete launcher identity.'
            Assert ([long]$record.LauncherCreatedUtcFileTime -eq $process.StartTime.ToUniversalTime().ToFileTimeUtc()) 'Native launcher creation time differs from the actual process.'
            Assert ([Guid]$record.LaunchId -ne [Guid]::Empty) 'Native launch identifier missing.'
            Remove-ShellMaintenanceSessions $maintenance
            Assert (-not [IO.Directory]::Exists($proof.Path)) 'Completed native stage could not be cleaned.'
        }else{
            Assert ($proof.Retain -eq '1') 'Stage failure did not retain the uninstaller.'
            if($proof.Path){Assert (-not [IO.File]::Exists((Join-Path $proof.Path 'Session.ini'))) "Partial stage published a valid marker: $Fault"}
            if($Fault -ne 'locked-temp' -and $Fault -ne 'directory-failure' -and $Fault -ne 'temp-failure'){Assert (-not [IO.Directory]::Exists($proof.Path)) "Owned failed stage was not cleaned: $Fault"}
        }
        $key=[Microsoft.Win32.Registry]::CurrentUser.OpenSubKey($hive)
        try{foreach($field in @('PropertyHandlerInstalled','SystemIntegrationInstalled','ShellCleanupPending','ShellRecoveryPending')){Assert ($key.GetValue($field) -eq 1) "Preparation modified confirmed state: $field"}}finally{$key.Close()}
        Assert ((Get-ShellHash $owner) -eq $ownerHash -and (Get-ShellHash $journal) -eq $journalHash) 'Preparation changed Owner or recovery state.'
        return $proof.Path
    }
    $null=Check-Stage 'success' $true
    $null=Check-Stage 'temp-failure' $false
    $collision=Check-Stage 'directory-failure' $false;Assert ([IO.File]::ReadAllText($collision) -eq 'foreign sentinel') 'Stage deleted an unconfirmed collision file.'
    $lockedMarker=Join-Path $work 'locked-marker.ini';[IO.File]::WriteAllText($lockedMarker,'foreign marker');$markerLock=[IO.File]::OpenRead($lockedMarker)
    try{$null=Check-Stage 'open-marker' $false}finally{$markerLock.Close()}
    Assert ([IO.File]::ReadAllText($lockedMarker) -eq 'foreign marker') 'Failed marker write changed a foreign file.'
    foreach($fault in @('write-marker','close-marker','extract-failure')){$null=Check-Stage $fault $false}
    $lockedTemp=Join-Path $maintenance 'locked.tmp';[IO.File]::WriteAllText($lockedTemp,'reserved');$tempLock=[IO.File]::OpenRead($lockedTemp)
    try{$null=Check-Stage 'locked-temp' $false;Assert ([IO.File]::Exists($lockedTemp)) 'Locked reserved file was lost.'}finally{$tempLock.Close()}
    $null=Check-Stage 'locked-temp' $true
    Write-Host "Shell protocol and maintenance cleanup checks passed. Artifacts: $work"
}finally{[Microsoft.Win32.Registry]::CurrentUser.DeleteSubKeyTree($hive)}
