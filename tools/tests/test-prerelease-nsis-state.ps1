[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
$repo=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$source=[IO.File]::ReadAllText((Join-Path $repo 'packaging/nsis/Installer/MakeInstaller.nsi'))
$capture=[regex]::Match($source,'(?s)  StrCpy \$PreviousAssociationOwned 0.*?  \$\{EndIf\}').Value
$writes=([regex]::Matches($source,'(?m)^  WriteRegDWORD .*"(?:AssociationRegistered|ValidateVerbInstalled)" \$Previous\w+Owned\s*$') | ForEach-Object {$_.Value.TrimEnd()}) -join "`r`n"
$wrapper=[regex]::Match($source,'(?s)Function CreateStartMenuShortcutsAsUser\s+.*?FunctionEnd').Value
if(-not $capture -or -not $writes -or -not $wrapper){throw 'Missing production installer state operations'}
$work=Join-Path $repo ('build/tests/nsis-state-'+[guid]::NewGuid().ToString('N'))
$null=[IO.Directory]::CreateDirectory($work)
$key='Software\FBENextInstallerTests\'+[guid]::NewGuid().ToString('N')
$script=@'
Unicode true
!include "LogicLib.nsh"
Name "FBE state regression"
OutFile "@EXE@"
RequestExecutionLevel user
SilentInstall silent
!define PRODUCT_UNINST_ROOT_KEY HKCU
!define PRODUCT_UNINST_KEY "@KEY@"
!define PRODUCT_NAME "@NAME@"
Var PreviousAssociationOwned
Var PreviousValidateOwned
Var ICONS_GROUP
Function CreateStartMenuShortcuts
  StrCmp $ICONS_GROUP "Нестандартная группа [FBE]" group_ok
    SetErrorLevel 9
    Quit
group_ok:
FunctionEnd
@WRAPPER@
Section
  StrCpy $INSTDIR "$TEMP\FBE-State-Test"
  WriteRegStr HKCU "${PRODUCT_UNINST_KEY}" "InstallLocation" $INSTDIR
  WriteRegDWORD HKCU "${PRODUCT_UNINST_KEY}" "AssociationRegistered" 1
  WriteRegDWORD HKCU "${PRODUCT_UNINST_KEY}" "ValidateVerbInstalled" 1
  @CAPTURE@
  @WRITES@
  ReadRegDWORD $0 HKCU "${PRODUCT_UNINST_KEY}" "AssociationRegistered"
  IntCmp $0 1 +3
    SetErrorLevel 1
    Quit
  ReadRegDWORD $0 HKCU "${PRODUCT_UNINST_KEY}" "ValidateVerbInstalled"
  IntCmp $0 1 +3
    SetErrorLevel 2
    Quit
  StrCpy $INSTDIR "$TEMP\Different-FBE-State-Test"
  @CAPTURE@
  IntCmp $PreviousAssociationOwned 0 +3
    SetErrorLevel 3
    Quit
  IntCmp $PreviousValidateOwned 0 +3
    SetErrorLevel 4
    Quit
  StrCpy $R7 "Нестандартная группа [FBE]"
  StrCpy $ICONS_GROUP "stale outer selection"
  Call CreateStartMenuShortcutsAsUser
  DeleteRegKey HKCU "${PRODUCT_UNINST_KEY}"
  SetErrorLevel 0
SectionEnd
'@
# Execute the actual production statements against an isolated HKCU record.
# PRODUCT_NAME's long spelling appears only in the two production write lines.
$writes=$writes.Replace('Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}','${PRODUCT_UNINST_KEY}')
$script=$script.Replace('@EXE@',(Join-Path $work 'state.exe')).Replace('@KEY@',$key).Replace('@NAME@','FBE regression').Replace('@CAPTURE@',$capture).Replace('@WRITES@',$writes).Replace('@WRAPPER@',$wrapper)
$path=Join-Path $work 'state.nsi';[IO.File]::WriteAllText($path,$script,[Text.UTF8Encoding]::new($true))
$compiler=& (Join-Path $repo 'tools/build/resolve-nsis.ps1')
try {
    & $compiler /V2 $path
    if($LASTEXITCODE -ne 0){throw 'NSIS state regression did not compile'}
    $process=Start-Process -FilePath (Join-Path $work 'state.exe') -WindowStyle Hidden -Wait -PassThru
    if($process.ExitCode -ne 0){throw "NSIS state regression failed: $($process.ExitCode)"}
    Write-Host "PASS: real NSIS retained same-install ownership, rejected foreign-install state, passed current menu group through register wrapper. Real UAC not exercised. Evidence: $work"
} finally {[Microsoft.Win32.Registry]::CurrentUser.DeleteSubKeyTree($key,$false)}
