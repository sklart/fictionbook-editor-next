!define /date BUILDNUM "%d %b"
Unicode true
!include "version.nsh"
!ifndef INPUTDIR
!define INPUTDIR "..\..\..\out\package\FictionBookEditor"
!endif
!ifndef PRODUCT_NAME
!define PRODUCT_NAME "FictionBook Editor Next"
!endif
!define PRODUCT_STAGE "Release"
!define PRODUCT_BUILD "build ${BUILDNUM}"
!ifdef FBE_WIN7_BUILD
!define PRODUCT_COMPATIBILITY_SUFFIX " (Windows 7 compatible)"
!else
!define PRODUCT_COMPATIBILITY_SUFFIX ""
!endif
!define PRODUCT_VERSION "${PRODUCT_STAGE} ${PRODUCT_VER_NUM} (${PRODUCT_BUILD})${PRODUCT_COMPATIBILITY_SUFFIX}"
!ifndef PRODUCT_VENDOR
!define PRODUCT_VENDOR "FBE Team"
!endif
!define PRODUCT_NAME_VERSION "${PRODUCT_NAME} ${PRODUCT_VERSION}"
!ifndef OUTPUTFILE
!define OUTPUTFILE "${PRODUCT_NAME_VERSION}.exe"
!endif
!define PRODUCT_URL "https://github.com/sklart/fictionbook-editor-next"
; Отдельное имя ключа не позволяет FBE Next перезаписывать App Paths старого FBE.
!ifndef PRODUCT_DIR_REGKEY
!define PRODUCT_DIR_REGKEY "Software\Microsoft\Windows\CurrentVersion\App Paths\FictionBookEditorNext.exe"
!endif
!define PRODUCT_UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}"
!ifndef PRODUCT_SHORTCUT_NAME
!define PRODUCT_SHORTCUT_NAME "${PRODUCT_NAME}"
!endif
; SHCTX follows SetShellVarContext: current user by default, all users after
; explicit elevation/scope selection.  Do not hard-code HKCU in shared paths.
!define PRODUCT_UNINST_ROOT_KEY "SHCTX"
!define PRODUCT_STARTMENU_REGVAL "NSIS:StartMenuDir"
!define PRODUCT_SYSTEM_INTEGRATION_REGVAL "SystemIntegrationInstalled"
!define FB2_PROPERTY_HANDLER_CLSID "{D4A47F38-1E5A-4F0D-B1C9-6D2A4A6B1F42}"
!define FBE_SEQUENCE_SCHEMA_FILE "FBE.Sequence.propdesc"
!define FBE_SHELL_ROOT "$COMMONFILES\FictionBook Editor Next"
!define FBE_SHELL_SHARED_DIR "${FBE_SHELL_ROOT}\Shell"
!ifndef FBE_SHELL_X86_SHA256
!define FBE_SHELL_X86_SHA256 ""
!endif
!ifndef FBE_SHELL_X64_SHA256
!define FBE_SHELL_X64_SHA256 ""
!endif
!ifndef FBE_SCHEMA_SHA256
!define FBE_SCHEMA_SHA256 ""
!endif
!ifdef FBE_DEPLOYMENT_TEST_SCOPE_PROBE
  !system 'powershell.exe -NoProfile -ExecutionPolicy Bypass -File "..\..\..\tools\build\verify-installer-payload.ps1" -X86Sha256 "${FBE_SHELL_X86_SHA256}" -X64Sha256 "${FBE_SHELL_X64_SHA256}" -SchemaSha256 "${FBE_SCHEMA_SHA256}" -FormatOnly' = 0
!else
  !system 'powershell.exe -NoProfile -ExecutionPolicy Bypass -File "..\..\..\tools\build\verify-installer-payload.ps1" -X86Sha256 "${FBE_SHELL_X86_SHA256}" -X64Sha256 "${FBE_SHELL_X64_SHA256}" -SchemaSha256 "${FBE_SCHEMA_SHA256}" -InputDirectory "${INPUTDIR}"' = 0
!endif
!define FB2_INFOTIP_PROPERTIES "prop:System.ItemTypeText;System.Author;System.Title;System.Language;FBE.Sequence;FBE.DocumentVersion;FBE.DocumentDate;System.Size"
!define FB2_TILEINFO_PROPERTIES "prop:System.Author;System.Title"
!define FB2_DETAILS_PROPERTIES "prop:System.ItemTypeText;System.Author;System.Title;System.Language;FBE.Genre;FBE.Sequence;FBE.DocumentVersion;FBE.DocumentDate;FBE.Keywords;FBE.DocumentId;System.Size"
!define FB2_PREVIEWDETAILS_PROPERTIES "prop:System.ItemTypeText;System.Author;System.Title;System.Language;FBE.Genre;FBE.Sequence;FBE.DocumentVersion;FBE.DocumentDate;FBE.Keywords;FBE.DocumentId;System.Size"
!define FB2_SYSTEM_ASSOC_KEY "Software\Classes\SystemFileAssociations\.fb2"
; Upgrade migration only: remove a registration made by an older FBE Next
; version when, and only when, it still points at the old root DLL.
!macro UnregisterPreviousBundledPluginIfOwned CLSID DLL
  IfFileExists "$INSTDIR\${DLL}" 0 +3
  ReadRegStr $0 HKCU "Software\Classes\CLSID\${CLSID}\InprocServer32" ""
  StrCmp $0 "$INSTDIR\${DLL}" 0 +2
    UnRegDll "$INSTDIR\${DLL}"
!macroend
ManifestDPIAware true
SetCompressor /SOLID lzma

;--------------------------------
;Interface Configuration

  !define MUI_WELCOMEFINISHPAGE_BITMAP "..\\res\\fbe-wizard.bmp"
  !define MUI_UNWELCOMEFINISHPAGE_BITMAP "..\\res\\fbe-wizard.bmp"

RequestExecutionLevel user

; Includes
!include "MUI2.nsh"
!include "UAC.nsh"
!include "LogicLib.nsh"
!include "Sections.nsh"
!include "x64.nsh"
!include "FileFunc.nsh"
!include "nsDialogs.nsh"

!define MUI_ABORTWARNING
!define MUI_ICON "${NSISDIR}\Contrib\Graphics\Icons\modern-install.ico"
!define MUI_UNICON "${NSISDIR}\Contrib\Graphics\Icons\modern-uninstall.ico"
!define MUI_CUSTOMFUNCTION_GUIINIT GuiInit

; Installer pages

; Welcome page
!define MUI_WELCOMEPAGE_TITLE_3LINES
!insertmacro MUI_PAGE_WELCOME

; License page
!insertmacro MUI_PAGE_LICENSE $(License)
LicenseForceSelection radiobuttons

; Deployment choice must happen before components and elevation.  Portable is
; an extraction path: it never reaches optional integration or uninstall code.
Page custom DeploymentModePageCreate DeploymentModePageLeave
Page custom InstallScopePageCreate InstallScopePageLeave

; Components page
!define MUI_PAGE_CUSTOMFUNCTION_SHOW ComponentsPageShow
!define MUI_PAGE_CUSTOMFUNCTION_LEAVE ComponentsPageLeave
!define MUI_PAGE_CUSTOMFUNCTION_PRE ComponentsPagePre
; Компактная разметка оставляет больше ширины для дерева компонентов.
; Высота поля описания увеличивается в ComponentsPageShow.
!define MUI_COMPONENTSPAGE_SMALLDESC
!insertmacro MUI_PAGE_COMPONENTS

; Directory page
!insertmacro MUI_PAGE_DIRECTORY

; Start menu page
var ICONS_GROUP
!define MUI_PAGE_CUSTOMFUNCTION_PRE StartMenuPagePre
!define MUI_STARTMENUPAGE_NODISABLE
!define MUI_STARTMENUPAGE_DEFAULTFOLDER "${PRODUCT_NAME}"
!define MUI_STARTMENUPAGE_REGISTRY_ROOT "${PRODUCT_UNINST_ROOT_KEY}"
!define MUI_STARTMENUPAGE_REGISTRY_KEY "${PRODUCT_UNINST_KEY}"
!define MUI_STARTMENUPAGE_REGISTRY_VALUENAME "${PRODUCT_STARTMENU_REGVAL}"
!insertmacro MUI_PAGE_STARTMENU Application $ICONS_GROUP

; Instfiles page
!insertmacro MUI_PAGE_INSTFILES

; Finish page
!define MUI_FINISHPAGE_RUN
!define MUI_FINISHPAGE_RUN_FUNCTION ExecAppFile
!define MUI_FINISHPAGE_RUN_TEXT "$(FinishPageRunText)"
!define MUI_FINISHPAGE_TITLE "$(FinishPageTitle)"
!define MUI_FINISHPAGE_TEXT "$(FinishPageText)"
!define MUI_FINISHPAGE_TITLE_3LINES
!define MUI_PAGE_CUSTOMFUNCTION_SHOW FinishPageShow
!insertmacro MUI_PAGE_FINISH

; Uninstaller pages

; Uninstaller welcome page
!define MUI_WELCOMEPAGE_TITLE_3LINES
;!insertmacro MUI_UNPAGE_WELCOME

; Uninstaller confirm page
!insertmacro MUI_UNPAGE_CONFIRM

; User data is deliberately opt-in. Keeping this as a page instead of a
; MessageBox also makes a silent uninstall deterministic: it preserves data.
UninstPage custom un.UserDataPageCreate un.UserDataPageLeave

; Uninstaller instfile page
!insertmacro MUI_UNPAGE_INSTFILES

; Uninstaller finish page
!define MUI_FINISHPAGE_TITLE_3LINES
!insertmacro MUI_UNPAGE_FINISH

; MUI end 

!undef BUILDNUM
!define /date DATE "%d_%b"

; Language files
!insertmacro MUI_LANGUAGE "English"
!insertmacro MUI_LANGUAGE "Russian"
!insertmacro MUI_LANGUAGE "Ukrainian"
!insertmacro MUI_LANGUAGE "German"
!insertmacro MUI_LANGUAGE "French"
!insertmacro MUI_LANGUAGE "Spanish"
!insertmacro MUI_LANGUAGE "Italian"
!insertmacro MUI_LANGUAGE "Polish"
!insertmacro MUI_LANGUAGE "Portuguese"
!insertmacro MUI_LANGUAGE "Dutch"
!insertmacro MUI_LANGUAGE "Czech"
!insertmacro MUI_LANGUAGE "Bulgarian"

; Localized strings
!include "Localization\English.nsh"
!include "Localization\Russian.nsh"
!include "Localization\Ukrainian.nsh"
!include "Generated\EuropeanFallback.generated.nsh"

Name "${PRODUCT_NAME_VERSION}"
OutFile "${OUTPUTFILE}"
InstallDir "$LOCALAPPDATA\Programs\${PRODUCT_NAME}"
; InstallDirRegKey accepts only physical hives.  The custom scope page selects
; Program Files for All Users; HKCU preserves the current-user default here.
InstallDirRegKey HKCU "${PRODUCT_DIR_REGKEY}" ""
ShowInstDetails show
ShowUnInstDetails show

Var DeploymentMode
Var InstallScope
Var DeploymentModeInstallRadio
Var DeploymentModePortableRadio
Var InstallScopeCurrentRadio
Var InstallScopeAllUsersRadio
Var ExistingMachineInstall
Var DetectedInstallScope
Var DetectedCurrentInstallLocation
Var DetectedAllUsersInstallLocation
Var SessionCurrentInstallLocation
Var SessionAllUsersInstallLocation
Var ScopeTransitionResult
Var InstalledStateSaved
Var SavedInstallScope
Var SavedInstallDirectory
Var ShellStatusValid
Var ShellSystemInstalled
Var ShellToolsDirectory
Var ShellStageReady
Var ShellStageCreated
Var ShellStageHandle
Var ShellStageStep
Var ShellOperation
Var ShellOperationCode
Var ShellEffectiveInstalled
Var ShellCleanupPending
Var ShellRecoveryPending
Var ShellRetainUninstaller
Var ShellPreviousProperty
Var ShellPreviousSystem
Var ShellPreviousLocation
Var ShellPayloadResult
Var SavedSystemIntegrationFlags
Var SavedFb2AssociationFlags
Var SavedFbdAssociationFlags
Var SavedValidateFlags
Var PreviousAssociationOwned
Var PreviousValidateOwned
Var SavedExplorerFlags
Var SavedShortcutsFlags
Var SavedStartMenuFlags
Var SavedDesktopFlags
Var UninstallUserData
Var UninstallUserDataCheckbox
Var CommandLineCurrentUser
Var CommandLineAllUsers
Var CommandLinePortable
Var ShellRecoveryRequested

!macro FBE_RequireOrdinaryPath path
  Push $9
  System::Call 'kernel32::GetFileAttributesW(w "${path}") i .r9'
  ${If} $9 != -1
    IntOp $9 $9 & 0x400
    ${If} $9 != 0
      MessageBox MB_OK|MB_ICONSTOP "$(UnsafeShellPayloadLocation)"
      SetErrorLevel 5
      Quit
    ${EndIf}
  ${EndIf}
  Pop $9
!macroend

!macro FBE_RestoreElevationVar variable
  ClearErrors
  !insertmacro UAC_AsUser_GetGlobalVar ${variable}
  ${If} ${Errors}
    Goto elevation_state_failed
  ${EndIf}
!macroend

!macro FBE_RestoreElevationSection
  ClearErrors
  !insertmacro UAC_AsUser_GetSection Flags $R8 $R0
  ${If} ${Errors}
    Goto elevation_state_failed
  ${EndIf}
  SectionSetFlags $R8 $R0
  ${If} ${Errors}
    Goto elevation_state_failed
  ${EndIf}
!macroend

Function .onInit
  ClearErrors
  UAC::_ 5
  ${If} ${Errors}
    MessageBox MB_OK|MB_ICONSTOP "$(UacIpcInitializationFailed) $0"
    SetErrorLevel 31
    Quit
  ${EndIf}
  ${If} ${UAC_IsInnerInstance}
  ${AndIfNot} ${UAC_IsAdmin}
    SetErrorLevel 0x666666
    Quit
  ${EndIf}
  !ifndef FBE_DEPLOYMENT_TEST_SCOPE_PROBE
  ${If} ${UAC_IsInnerInstance}
    Call RestoreElevatedInstallerState
    Return
  ${EndIf}
  !endif
  SetShellVarContext current
  StrCpy $DeploymentMode "installed"
  StrCpy $InstallScope "current"
  ; Test-only compile definitions make silent installer smoke tests possible
  ; without interacting with a real FBE Next installation.  They are absent
  ; from normal release builds and never alter the interactive defaults.
  !ifdef FBE_DEPLOYMENT_TEST_PORTABLE
    StrCpy $DeploymentMode "portable"
  !endif
  !ifdef FBE_DEPLOYMENT_TEST_ALLUSERS
    StrCpy $InstallScope "allusers"
    SetShellVarContext all
  !endif
  !ifdef FBE_DEPLOYMENT_TEST_ROOT
    ; The scope smoke redirects only its test probe, never a release install.
    StrCpy $INSTDIR "${FBE_DEPLOYMENT_TEST_ROOT}"
  !endif
  ; A normal GUI upgrade should reopen the scope of the installed copy.  An
  ; explicit command line remains authoritative, and silent setup deliberately
  ; has no implicit registry-based mode selection.
  IfSilent 0 +2
    Goto deployment_scope_detected
  Call DetectExistingInstallScope
deployment_scope_detected:
  Call ApplyDeploymentCommandLine
  ; GUI checks the conflict after the user has chosen scope.  Silent setup has
  ; no page-leave callback, so it must fail before elevation or file writes.
  IfSilent 0 +2
    Call CheckOtherScopeConflict
  ${If} $DeploymentMode == "portable"
    Call DisablePortableIntegration
  ${EndIf}
  !ifdef FBE_DEPLOYMENT_TEST_ROOT
    StrCpy $INSTDIR "${FBE_DEPLOYMENT_TEST_ROOT}"
  !endif
  !ifndef FBE_DEPLOYMENT_TEST_SCOPE_PROBE
    IfSilent 0 +2
      Call EnsureAllUsersElevation
  !endif
  ${IfNot} ${UAC_IsInnerInstance}
    !insertmacro MUI_LANGDLL_DISPLAY
  ${EndIf}
FunctionEnd


Function ApplyDeploymentCommandLine
  ${GetParameters} $0
  StrCpy $CommandLineCurrentUser "0"
  StrCpy $CommandLineAllUsers "0"
  StrCpy $CommandLinePortable "0"
  StrCpy $ShellRecoveryRequested "0"
  ClearErrors
  ${GetOptions} $0 "/RECOVERSHELL" $1
  ${IfNot} ${Errors}
    StrCpy $ShellRecoveryRequested "1"
  ${EndIf}
  ClearErrors
  ${GetOptions} $0 "/CURRENTUSER" $1
  ${IfNot} ${Errors}
    StrCpy $CommandLineCurrentUser "1"
  ${EndIf}
  ClearErrors
  ${GetOptions} $0 "/ALLUSERS" $1
  ${IfNot} ${Errors}
    StrCpy $CommandLineAllUsers "1"
  ${EndIf}
  ClearErrors
  ${GetOptions} $0 "/PORTABLE" $1
  ${IfNot} ${Errors}
    StrCpy $CommandLinePortable "1"
  ${EndIf}
  ${If} $CommandLineCurrentUser == "1"
  ${AndIf} $CommandLineAllUsers == "1"
    SetErrorLevel 87
    Quit
  ${EndIf}
  ${If} $CommandLinePortable == "1"
  ${AndIf} $CommandLineCurrentUser == "1"
    SetErrorLevel 87
    Quit
  ${EndIf}
  ${If} $CommandLinePortable == "1"
  ${AndIf} $CommandLineAllUsers == "1"
    SetErrorLevel 87
    Quit
  ${EndIf}
  ${If} $CommandLinePortable == "1"
    StrCpy $DeploymentMode "portable"
    StrCpy $InstallScope "current"
    SetShellVarContext current
    StrCpy $INSTDIR "$EXEDIR\${PRODUCT_NAME} Portable"
    Return
  ${EndIf}
  ${If} $CommandLineAllUsers == "1"
    StrCpy $DeploymentMode "installed"
    StrCpy $InstallScope "allusers"
    SetShellVarContext all
    StrCpy $INSTDIR "$PROGRAMFILES32\${PRODUCT_NAME}"
    Return
  ${EndIf}
  ${If} $CommandLineCurrentUser == "1"
    StrCpy $DeploymentMode "installed"
    StrCpy $InstallScope "current"
    SetShellVarContext current
    StrCpy $INSTDIR "$LOCALAPPDATA\Programs\${PRODUCT_NAME}"
  ${EndIf}
FunctionEnd

Function DetectExistingInstallScope
  ReadRegStr $DetectedAllUsersInstallLocation HKLM "${PRODUCT_UNINST_KEY}" "InstallLocation"
  ReadRegStr $DetectedCurrentInstallLocation HKCU "${PRODUCT_UNINST_KEY}" "InstallLocation"
  ${If} $DetectedAllUsersInstallLocation != ""
    StrCpy $SessionAllUsersInstallLocation "$DetectedAllUsersInstallLocation"
    StrCpy $DetectedInstallScope "allusers"
    StrCpy $InstallScope "allusers"
    SetShellVarContext all
    StrCpy $INSTDIR "$DetectedAllUsersInstallLocation"
    Return
  ${EndIf}
  ${If} $DetectedCurrentInstallLocation != ""
    StrCpy $SessionCurrentInstallLocation "$DetectedCurrentInstallLocation"
    StrCpy $DetectedInstallScope "current"
    StrCpy $InstallScope "current"
    SetShellVarContext current
    StrCpy $INSTDIR "$DetectedCurrentInstallLocation"
  ${EndIf}
FunctionEnd

Function EnsureAllUsersElevation
  ${If} $InstallScope != "allusers"
    Return
  ${EndIf}
  ${If} ${UAC_IsAdmin}
    Return
  ${EndIf}
  !insertmacro UAC_PageElevation_RunElevated
  ${If} $2 = 0x666666
    MessageBox MB_OK|MB_ICONEXCLAMATION $(UacAbortInstaller)
    Abort
  ${ElseIf} $0 = 1223
    SetErrorLevel 1223
    Abort
  ${ElseIf} $0 = 1062
    MessageBox MB_OK|MB_ICONSTOP $(UacLogonServiceInstaller)
    Abort
  ${ElseIf} $0 <> 0
    MessageBox MB_OK|MB_ICONSTOP "$(UacUnknownError) $0"
    Abort
  ${EndIf}
  Quit
FunctionEnd

Function .OnInstFailed
FunctionEnd
 
Function .OnInstSuccess
FunctionEnd

Function GuiInit
  !insertmacro UAC_PageElevation_OnGuiInit
FunctionEnd

Function DeploymentModePageCreate
  nsDialogs::Create 1018
  Pop $0
  ${If} $0 == error
    Abort
  ${EndIf}
  ${NSD_CreateLabel} 0 0 100% 20u "$(DeploymentModeTitle)"
  Pop $0
  ${NSD_CreateRadioButton} 10u 28u 100% 12u "$(DeploymentModeInstalled)"
  Pop $DeploymentModeInstallRadio
  ${NSD_CreateRadioButton} 10u 48u 100% 12u "$(DeploymentModePortable)"
  Pop $DeploymentModePortableRadio
  ${If} $DeploymentMode == "portable"
    ${NSD_Check} $DeploymentModePortableRadio
  ${Else}
    ${NSD_Check} $DeploymentModeInstallRadio
  ${EndIf}
  nsDialogs::Show
FunctionEnd

Function DeploymentModePageLeave
  ${NSD_GetState} $DeploymentModePortableRadio $0
  ${If} $0 == ${BST_CHECKED}
    ${If} $DeploymentMode == "installed"
      !ifndef FBE_DEPLOYMENT_TEST_SCOPE_PROBE
      Call SaveInstalledModeState
      !endif
    ${EndIf}
    StrCpy $DeploymentMode "portable"
    StrCpy $InstallScope "current"
    SetShellVarContext current
    StrCpy $INSTDIR "$EXEDIR\${PRODUCT_NAME} Portable"
    Call DisablePortableIntegration
  ${Else}
    ${If} $DeploymentMode == "portable"
      !ifndef FBE_DEPLOYMENT_TEST_SCOPE_PROBE
      Call RestoreInstalledModeState
      !endif
    ${EndIf}
    StrCpy $DeploymentMode "installed"
  ${EndIf}
FunctionEnd

Function InstallScopePageCreate
  ${If} $DeploymentMode == "portable"
    Abort
  ${EndIf}
  nsDialogs::Create 1018
  Pop $0
  ${If} $0 == error
    Abort
  ${EndIf}
  ${NSD_CreateLabel} 0 0 100% 20u "$(InstallScopeTitle)"
  Pop $0
  ${NSD_CreateRadioButton} 10u 28u 100% 12u "$(InstallScopeCurrent)"
  Pop $InstallScopeCurrentRadio
  ${NSD_CreateLabel} 24u 40u 100% 10u "$LOCALAPPDATA\Programs\${PRODUCT_NAME}"
  Pop $0
  ${NSD_CreateRadioButton} 10u 54u 100% 12u "$(InstallScopeAllUsers)"
  Pop $InstallScopeAllUsersRadio
  ${NSD_CreateLabel} 24u 66u 100% 10u "$PROGRAMFILES32\${PRODUCT_NAME}"
  Pop $0
  ${If} $InstallScope == "allusers"
    ${NSD_Check} $InstallScopeAllUsersRadio
  ${Else}
    ${NSD_Check} $InstallScopeCurrentRadio
  ${EndIf}
  nsDialogs::Show
FunctionEnd

Function InstallScopePageLeave
  ${NSD_GetState} $InstallScopeAllUsersRadio $0
  ${If} $0 == ${BST_CHECKED}
    StrCpy $0 "allusers"
  ${Else}
    StrCpy $0 "current"
  ${EndIf}
  Call ApplyInstallScopeSelection
  Call CheckOtherScopeConflict
FunctionEnd

; $0 is the selected scope. Preserve the directory typed on the Directory
; page before crossing scopes, then restore that scope's session path when
; returning. Selecting the already active scope deliberately does nothing.
Function ApplyInstallScopeSelection
  ${If} $0 == $InstallScope
    Return
  ${EndIf}
  ${If} $InstallScope == "current"
    StrCpy $SessionCurrentInstallLocation "$INSTDIR"
  ${Else}
    StrCpy $SessionAllUsersInstallLocation "$INSTDIR"
  ${EndIf}
  ${If} $0 == "allusers"
    StrCpy $InstallScope "allusers"
    SetShellVarContext all
    ${If} $SessionAllUsersInstallLocation != ""
      StrCpy $INSTDIR "$SessionAllUsersInstallLocation"
    ${Else}
      StrCpy $INSTDIR "$PROGRAMFILES32\${PRODUCT_NAME}"
    ${EndIf}
  ${Else}
    StrCpy $InstallScope "current"
    SetShellVarContext current
    ${If} $SessionCurrentInstallLocation != ""
      StrCpy $INSTDIR "$SessionCurrentInstallLocation"
    ${Else}
      StrCpy $INSTDIR "$LOCALAPPDATA\Programs\${PRODUCT_NAME}"
    ${EndIf}
  ${EndIf}
FunctionEnd

Function CheckOtherScopeConflict
  ; Installed copies are single-scope by design.  Portable copies never reach
  ; this page and may coexist freely with either installed scope.
  ${If} $DeploymentMode == "portable"
    Return
  ${EndIf}
  ${If} $InstallScope == "allusers"
    ReadRegStr $0 HKCU "${PRODUCT_UNINST_KEY}" "InstallLocation"
  ${Else}
    ReadRegStr $0 HKLM "${PRODUCT_UNINST_KEY}" "InstallLocation"
  ${EndIf}
  ${If} $0 != ""
    IfSilent conflict_silent
    MessageBox MB_OK|MB_ICONEXCLAMATION "$(InstallScopeConflict)$\r$\n$\r$\n$0"
    Abort
conflict_silent:
    SetErrorLevel 183
    Quit
  ${EndIf}
FunctionEnd

Function ComponentsPagePre
  ${If} $DeploymentMode == "portable"
    Abort
  ${EndIf}
FunctionEnd

Function StartMenuPagePre
  ${If} $DeploymentMode == "portable"
    Abort
  ${EndIf}
FunctionEnd

Function CheckMSXMLVersion
  ReadRegStr $0 HKCR "Msxml2.SAXXMLReader.6.0\CLSID" ""
  StrCmp $0 "" noxml
  ReadRegStr $1 HKCR "CLSID\$0\ProgID" ""
  StrCmp $1 "Msxml2.SAXXMLReader.6.0" ok
noxml:
  MessageBox MB_OK|MB_ICONSTOP $(ErrCheckMSXMLVersion)
  Quit
ok:
FunctionEnd

Function CheckIEVersion
  GetDllVersion "shdocvw.dll" $0 $1
  IntCmp $0 327730 ok 0 ok
  MessageBox MB_OK|MB_ICONSTOP $(ErrCheckIEVersion)
  Quit
ok:
FunctionEnd

Function CheckFBERunning
check:
  FindWindow $0 "FictionBookEditorFrame"
  IntCmp $0 0 ok1
  MessageBox MB_OK|MB_ICONSTOP $(ErrCheckFBERunning)
  Goto check
ok1:
  FindWindow $0 "" "FictionBook Validator"
  IntCmp $0 0 ok2
  MessageBox MB_OK|MB_ICONSTOP $(ErrCheckFBVRunning)
  Goto check
ok2:
FunctionEnd

Function RegisterTlb
  Exch $R0 ; save old R0 and get filename as R0
  Push $R1 ; save R1
  Push $R2 ; save R2
  ; now register TLB file R0
  StrCpy $R1 0 ; init R1 with 0 (maybe not necessary)
  System::Call "Oleaut32::LoadTypeLib(w, *i) i (R0, R1R1) .R2"
  ; R2 contains result, 0 if ok
  IntCmp $R2 0 cont
  ; debug MessageBox MB_OK "LoadTypeLib returned $R2 with $R0"
  Goto exit
cont:
  ; now R1 contains pointer to typelib
  System::Call "Oleaut32::RegisterTypeLib(i, w, i) i (R1, R0, 0) .R2"
  ; R2 contains result, 0 if ok
  IntCmp $R2 0 exit
  ; debug MessageBox MB_OK "RegisterTypeLib returned $R2 with $R1, $R0"
exit:
  ;; debug MessageBox MB_OK "RegisterTlb: all ok"
  Pop $R2 ; R2 restore
  Pop $R1 ; R1 restore
  Pop $R0 ; R0 restore
FunctionEnd


Function un.onInit
  ClearErrors
  UAC::_ 5
  ${If} ${Errors}
    MessageBox MB_OK|MB_ICONSTOP "$(UacIpcInitializationFailed) $0"
    SetErrorLevel 31
    Quit
  ${EndIf}
  ${If} ${UAC_IsInnerInstance}
  ${AndIfNot} ${UAC_IsAdmin}
    SetErrorLevel 0x666666
    Quit
  ${EndIf}
  ; Discover scope before reading SHCTX state.  A machine uninstall can be
  ; started by a non-administrator, so HKCU must not shadow its HKLM record.
  ReadRegStr $ExistingMachineInstall HKLM "${PRODUCT_UNINST_KEY}" "InstallLocation"
  StrCmp $ExistingMachineInstall "" un_current_scope
  SetShellVarContext all
  Goto un_scope_ready
un_current_scope:
  SetShellVarContext current
un_scope_ready:
  ${If} ${UAC_IsInnerInstance}
  ${AndIf} $ExistingMachineInstall == ""
    !insertmacro UAC_IsOriginalUser $0
    ${If} $0 != 1
      MessageBox MB_OK|MB_ICONSTOP $(UacDifferentUserUninstaller)
      SetErrorLevel 5
      Quit
    ${EndIf}
  ${EndIf}
  ${IfNot} ${UAC_IsInnerInstance}
    !insertmacro MUI_LANGDLL_DISPLAY
  ${EndIf}

  ${If} $ExistingMachineInstall != ""
  ${AndIfNot} ${UAC_IsAdmin}
    !insertmacro UAC_PageElevation_RunElevated
    ${If} $2 = 0x666666
      MessageBox MB_OK|MB_ICONEXCLAMATION $(UacAbortUninstaller)
      Abort
    ${ElseIf} $0 = 1223
      Abort
    ${ElseIf} $0 = 1062
      MessageBox MB_OK|MB_ICONSTOP $(UacLogonServiceUninstaller)
      Abort
    ${ElseIf} $0 <> 0
      MessageBox MB_OK|MB_ICONSTOP "$(UacUnknownError) $0"
      Abort
    ${EndIf}
    Quit
  ${EndIf}

  ReadRegDWORD $0 ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "${PRODUCT_SYSTEM_INTEGRATION_REGVAL}"
  ReadRegDWORD $1 ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "ShellCleanupPending"
  IntOp $0 $0 | $1
  ReadRegDWORD $1 ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "ShellRecoveryPending"
  IntOp $0 $0 | $1
  ReadINIStr $1 "${FBE_SHELL_SHARED_DIR}\Owner.ini" "Owner" "InstallDirectory"
  ${If} $0 <> 1
  ${AndIf} $1 != $INSTDIR
    Return
  ${EndIf}

  ${If} ${UAC_IsAdmin}
    Return
  ${EndIf}

  !insertmacro UAC_PageElevation_RunElevated

  ${If} $2 = 0x666666
    MessageBox MB_OK|MB_ICONEXCLAMATION $(UacAbortUninstaller)
    Abort
  ${ElseIf} $0 = 1223
    Abort
  ${ElseIf} $0 = 1062
    MessageBox MB_OK|MB_ICONSTOP $(UacLogonServiceUninstaller)
    Abort
  ${ElseIf} $0 <> 0
    MessageBox MB_OK|MB_ICONSTOP "$(UacUnknownError) $0"
    Abort
  ${EndIf}

  Quit
FunctionEnd
Function un.OnUnInstFailed
FunctionEnd
 
Function un.OnUnInstSuccess
FunctionEnd

Function un.CheckFBERunning
check:
  FindWindow $0 "FictionBookEditorFrame"
  IntCmp $0 0 ok1
  MessageBox MB_OK|MB_ICONSTOP $(ErrCheckFBERunning)
  Goto check
ok1:
  FindWindow $0 "" "FictionBook Validator"
  IntCmp $0 0 ok2
  MessageBox MB_OK|MB_ICONSTOP $(ErrCheckFBVRunning)
  Goto check
ok2:
FunctionEnd

; added by SeNS
Function un.GetUserAppData
  Push $1
  Push $2
  Push $3
  Push $4  
 
  StrCpy $1 ""
  StrCpy $2 "0x001C" # CSIDL_LOCAL_APPDATA 0x001c // <user name>\Local Settings\Applicaiton Data (non roaming)
  StrCpy $3 ""
  StrCpy $4 ""
 
  System::Call 'shell32::SHGetSpecialFolderPath(i $HWNDPARENT, t .r1, i r2, i r3) i .r4'
 
  Pop $4
  Pop $3
  Pop $2
  Exch $1
FunctionEnd

Function ExecAppFile
  !insertmacro UAC_AsUser_ExecShell "" "$INSTDIR\FBE.exe" "" "$INSTDIR" SW_SHOWNORMAL
FunctionEnd

Function FinishPageShow
  ; Финальная страница использует короткие локализованные строки, чтобы
  ; не подтягивать длинное имя сборки в заголовок и чекбокс запуска.
  System::Call 'user32::SetWindowPos(p $mui.FinishPage.Run, p 0, i 205, i 241, i 320, i 34, i 0x0004)'
FunctionEnd

!include "ShellMaintenanceStatus.nsh"

!macro FBE_MaintenanceFunctions prefix
Function ${prefix}StageShellMaintenance
  StrCpy $ShellStageReady 0
  StrCpy $ShellStageCreated ""
  StrCpy $ShellStageHandle ""
  StrCpy $ShellToolsDirectory ""
  !ifndef FBE_DEPLOYMENT_TEST_SCOPE_PROBE
  !insertmacro FBE_RequireOrdinaryPath "$PROGRAMFILES32"
  !insertmacro FBE_RequireOrdinaryPath "$COMMONFILES"
  !insertmacro FBE_RequireOrdinaryPath "${FBE_SHELL_ROOT}"
  StrCpy $ShellStageStep "create maintenance root"
  ClearErrors
  CreateDirectory "${FBE_SHELL_ROOT}"
  IfErrors stage_failed
  StrCpy $ShellStageStep "protect maintenance directory"
  nsExec::ExecToStack '"$SYSDIR\icacls.exe" "${FBE_SHELL_ROOT}" /reset'
  Pop $0
  Pop $1
  StrCmp $0 "0" +2
    Goto stage_failed
  !insertmacro FBE_RequireOrdinaryPath "${FBE_SHELL_ROOT}\Maintenance"
  StrCpy $ShellStageStep "create maintenance parent"
  ClearErrors
  CreateDirectory "${FBE_SHELL_ROOT}\Maintenance"
  IfErrors stage_failed
  StrCpy $ShellStageStep "protect maintenance directory"
  nsExec::ExecToStack '"$SYSDIR\icacls.exe" "${FBE_SHELL_ROOT}\Maintenance" /reset'
  Pop $0
  Pop $1
  StrCmp $0 "0" +2
    Goto stage_failed
  StrCpy $ShellStageStep "reserve temporary name"
  ClearErrors
  GetTempFileName $ShellToolsDirectory "${FBE_SHELL_ROOT}\Maintenance"
  IfErrors stage_failed
  StrCmp $ShellToolsDirectory "" stage_failed
  StrCpy $ShellStageCreated "file"
  !insertmacro FBE_RequireOrdinaryPath "$ShellToolsDirectory"
  StrCpy $ShellStageStep "remove reserved temporary file"
  ClearErrors
  Delete "$ShellToolsDirectory"
  IfErrors stage_failed
  StrCpy $ShellStageCreated ""
  StrCpy $ShellStageStep "create session directory"
  System::Call 'kernel32::CreateDirectoryW(w "$ShellToolsDirectory", p 0) i .r0'
  StrCmp $0 "1" +2
    Goto stage_failed
  StrCpy $ShellStageCreated "directory"
  StrCpy $ShellStageStep "protect maintenance directory"
  nsExec::ExecToStack '"$SYSDIR\icacls.exe" "$ShellToolsDirectory" /reset'
  Pop $0
  Pop $1
  StrCmp $0 "0" +2
    Goto stage_failed
  StrCpy $ShellStageStep "identify launcher"
  System::Call 'kernel32::GetCurrentProcess() p .r2'
  System::Call 'kernel32::GetProcessTimes(p r2, *l .r4, *l .r5, *l .r6, *l .r7) i .r3'
  StrCmp $3 "1" +2
    Goto stage_failed
  System::Call 'ole32::CoCreateGuid(g .r5) i .r3'
  StrCmp $3 "0" +2
    Goto stage_failed
  System::Call 'kernel32::GetCurrentProcessId() i .r6'
  IntCmp $6 0 stage_failed stage_failed
  SetOverwrite try
  StrCpy $ShellStageStep "open extraction directory"
  ClearErrors
  SetOutPath "$ShellToolsDirectory"
  IfErrors stage_failed
  StrCpy $ShellStageStep "extract manage-shell-integration.ps1"
  ClearErrors
  File "${INPUTDIR}\InstallerTools\manage-shell-integration.ps1"
  IfErrors stage_failed
  StrCpy $ShellStageStep "extract shell-integration-lifecycle.ps1"
  ClearErrors
  File "${INPUTDIR}\InstallerTools\shell-integration-lifecycle.ps1"
  IfErrors stage_failed
  StrCpy $ShellStageStep "extract update-shell-files.ps1"
  ClearErrors
  File "${INPUTDIR}\InstallerTools\update-shell-files.ps1"
  IfErrors stage_failed
  StrCpy $ShellStageStep "extract register-modern-property-handler.ps1"
  ClearErrors
  File "${INPUTDIR}\InstallerTools\register-modern-property-handler.ps1"
  IfErrors stage_failed
  StrCpy $ShellStageStep "extract register-sequence-property-schema.ps1"
  ClearErrors
  File "${INPUTDIR}\InstallerTools\register-sequence-property-schema.ps1"
  IfErrors stage_failed
  StrCpy $ShellStageStep "extract unregister-modern-property-handler.ps1"
  ClearErrors
  File "${INPUTDIR}\InstallerTools\unregister-modern-property-handler.ps1"
  IfErrors stage_failed
  !if "${prefix}" == ""
  StrCpy $ShellStageStep "open payload directory"
  ClearErrors
  SetOutPath "$ShellToolsDirectory\Payload"
  IfErrors stage_failed
  StrCpy $ShellStageStep "extract FBShell.dll"
  ClearErrors
  File "${INPUTDIR}\FBShell.dll"
  IfErrors stage_failed
  StrCpy $ShellStageStep "extract FBShell64.dll"
  ClearErrors
  File "${INPUTDIR}\FBShell64.dll"
  IfErrors stage_failed
  StrCpy $ShellStageStep "extract FBE.Sequence.propdesc"
  ClearErrors
  File "${INPUTDIR}\FBE.Sequence.propdesc"
  IfErrors stage_failed
  !endif
  StrCpy $ShellStageStep "open previous state"
  ClearErrors
  FileOpen $ShellStageHandle "$ShellToolsDirectory\Previous.ini" w
  IfErrors stage_failed
  ReadRegDWORD $0 ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "ShellRecoveryPending"
  StrCpy $ShellStageStep "write previous state"
  ClearErrors
  FileWriteUTF16LE /BOM $ShellStageHandle "[Markers]$\r$\nRecoveryPending=$0$\r$\nInstallLocation=$ShellPreviousLocation$\r$\nPropertyHandlerInstalled=$ShellPreviousProperty$\r$\nSystemIntegrationInstalled=$ShellPreviousSystem$\r$\n"
  IfErrors stage_failed
  StrCpy $ShellStageStep "close previous state"
  System::Call 'kernel32::FlushFileBuffers(p $ShellStageHandle) i .r0'
  StrCmp $0 "1" +2
    Goto stage_failed
  System::Call 'kernel32::CloseHandle(p $ShellStageHandle) i .r0'
  StrCpy $ShellStageHandle ""
  StrCmp $0 "1" +2
    Goto stage_failed
  StrCpy $ShellStageStep "open session marker"
  ClearErrors
  FileOpen $ShellStageHandle "$ShellToolsDirectory\Session.ini.next" w
  IfErrors stage_failed
  StrCpy $ShellStageStep "write session marker"
  ClearErrors
  FileWriteUTF16LE /BOM $ShellStageHandle "[Maintenance]$\r$\nVersion=2$\r$\nPhase=complete$\r$\nDirectory=$ShellToolsDirectory$\r$\nOwner=FBE Next Shell Maintenance$\r$\nLauncherPid=$6$\r$\nLauncherCreatedUtcFileTime=$4$\r$\nLaunchId=$5$\r$\n"
  IfErrors stage_failed
  StrCpy $ShellStageStep "close session marker"
  System::Call 'kernel32::FlushFileBuffers(p $ShellStageHandle) i .r0'
  StrCmp $0 "1" +2
    Goto stage_failed
  System::Call 'kernel32::CloseHandle(p $ShellStageHandle) i .r0'
  StrCpy $ShellStageHandle ""
  StrCmp $0 "1" +2
    Goto stage_failed
  StrCpy $ShellStageStep "restore installation directory"
  ClearErrors
  SetOutPath "$INSTDIR"
  IfErrors stage_failed
  StrCpy $ShellStageStep "publish session marker"
  ClearErrors
  Rename "$ShellToolsDirectory\Session.ini.next" "$ShellToolsDirectory\Session.ini"
  IfErrors stage_failed
  SetOverwrite on
  StrCpy $ShellStageReady 1
  Return
stage_failed:
  DetailPrint "Maintenance preparation failed at $ShellStageStep; no helper launched; confirmed integration state retained."
  ${If} $ShellStageHandle != ""
    FileClose $ShellStageHandle
    StrCpy $ShellStageHandle ""
  ${EndIf}
  SetOverwrite on
  SetOutPath "$INSTDIR"
  ${If} $ShellStageCreated == "file"
    Delete "$ShellToolsDirectory"
  ${ElseIf} $ShellStageCreated == "directory"
    ; Only this invocation's fixed extraction targets are eligible for cleanup.
    !insertmacro FBE_RequireOrdinaryPath "$ShellToolsDirectory\Payload"
    !insertmacro FBE_RequireOrdinaryPath "$ShellToolsDirectory\manage-shell-integration.ps1"
    Delete "$ShellToolsDirectory\manage-shell-integration.ps1"
    !insertmacro FBE_RequireOrdinaryPath "$ShellToolsDirectory\shell-integration-lifecycle.ps1"
    Delete "$ShellToolsDirectory\shell-integration-lifecycle.ps1"
    !insertmacro FBE_RequireOrdinaryPath "$ShellToolsDirectory\update-shell-files.ps1"
    Delete "$ShellToolsDirectory\update-shell-files.ps1"
    !insertmacro FBE_RequireOrdinaryPath "$ShellToolsDirectory\register-modern-property-handler.ps1"
    Delete "$ShellToolsDirectory\register-modern-property-handler.ps1"
    !insertmacro FBE_RequireOrdinaryPath "$ShellToolsDirectory\register-sequence-property-schema.ps1"
    Delete "$ShellToolsDirectory\register-sequence-property-schema.ps1"
    !insertmacro FBE_RequireOrdinaryPath "$ShellToolsDirectory\unregister-modern-property-handler.ps1"
    Delete "$ShellToolsDirectory\unregister-modern-property-handler.ps1"
    !insertmacro FBE_RequireOrdinaryPath "$ShellToolsDirectory\Previous.ini"
    Delete "$ShellToolsDirectory\Previous.ini"
    !insertmacro FBE_RequireOrdinaryPath "$ShellToolsDirectory\Session.ini.next"
    Delete "$ShellToolsDirectory\Session.ini.next"
    !insertmacro FBE_RequireOrdinaryPath "$ShellToolsDirectory\Session.ini"
    Delete "$ShellToolsDirectory\Session.ini"
    !insertmacro FBE_RequireOrdinaryPath "$ShellToolsDirectory\Payload\FBShell.dll"
    Delete "$ShellToolsDirectory\Payload\FBShell.dll"
    !insertmacro FBE_RequireOrdinaryPath "$ShellToolsDirectory\Payload\FBShell64.dll"
    Delete "$ShellToolsDirectory\Payload\FBShell64.dll"
    !insertmacro FBE_RequireOrdinaryPath "$ShellToolsDirectory\Payload\FBE.Sequence.propdesc"
    Delete "$ShellToolsDirectory\Payload\FBE.Sequence.propdesc"
    RMDir "$ShellToolsDirectory\Payload"
    RMDir "$ShellToolsDirectory"
  ${EndIf}
  ${If} ${FileExists} "$ShellToolsDirectory"
    DetailPrint "Maintenance preparation cleanup incomplete; current path retained: $ShellToolsDirectory"
  ${EndIf}
  StrCpy $ShellRetainUninstaller 1
  StrCpy $ShellPayloadResult "error"
  StrCpy $ShellOperationCode "PREPARATION_FAILED"
  SetErrorLevel 5
  MessageBox MB_OK|MB_ICONEXCLAMATION "$(ShellMaintenancePrepareFailed)" /SD IDOK
  !endif
FunctionEnd

Function ${prefix}ReadShellMaintenanceResult
  StrCpy $9 $0
  !insertmacro FBE_ReadShellMaintenanceStatus
  DetailPrint "Shell operation=$ShellOperation result=$ShellPayloadResult code=$ShellOperationCode exit=$9: $1"
  !insertmacro FBE_ApplyShellMaintenanceStatus
  ${If} $ShellStatusValid == 0
    !insertmacro FBE_RequireOrdinaryPath "${FBE_SHELL_SHARED_DIR}"
    CreateDirectory "${FBE_SHELL_SHARED_DIR}"
    !insertmacro FBE_RequireOrdinaryPath "${FBE_SHELL_SHARED_DIR}\UncertainOperation.ini"
    ${IfNot} ${FileExists} "${FBE_SHELL_SHARED_DIR}\UncertainOperation.ini"
      FileOpen $0 "${FBE_SHELL_SHARED_DIR}\UncertainOperation.ini" w
      FileWriteByte $0 255
      FileWriteByte $0 254
      FileClose $0
      WriteINIStr "${FBE_SHELL_SHARED_DIR}\UncertainOperation.ini" "Recovery" "InstallDirectory" "$INSTDIR"
    ${EndIf}
    DetailPrint "Uncertain helper outcome: confirmed markers retained; explicit recovery required."
  ${EndIf}
  StrCpy $ShellRetainUninstaller 0
  ${If} $ShellPayloadResult == "pending"
    StrCpy $ShellRetainUninstaller 1
    SetErrorLevel 3010
    ${If} $ShellCleanupPending == 1
      MessageBox MB_OK|MB_ICONINFORMATION "$(ShellCleanupRetry)" /SD IDOK
    ${Else}
      MessageBox MB_OK|MB_ICONINFORMATION "$(ShellMaintenanceRetry)" /SD IDOK
    ${EndIf}
  ${ElseIf} $ShellPayloadResult == "error"
    StrCpy $ShellRetainUninstaller 1
    SetErrorLevel 1
    MessageBox MB_OK|MB_ICONEXCLAMATION "$(ShellMaintenanceFailed)" /SD IDOK
  ${ElseIf} $ShellPayloadResult == "skipped"
    SetErrorLevel 2
    MessageBox MB_OK|MB_ICONINFORMATION "$(ShellForeignRetained)" /SD IDOK
  ${EndIf}
  Call ${prefix}CleanupShellMaintenance
FunctionEnd

Function ${prefix}CleanupShellMaintenance
  !ifndef FBE_DEPLOYMENT_TEST_SCOPE_PROBE
  GetErrorLevel $8
  System::Call 'kernel32::SetEnvironmentVariableW(w "FBE_SHELL_CLEANUP_ROOT", w "${FBE_SHELL_ROOT}\Maintenance")'
  System::Call 'kernel32::SetEnvironmentVariableW(w "FBE_SHELL_CLEANUP_LOADER", w "$ShellToolsDirectory\shell-integration-lifecycle.ps1")'
  System::Call 'kernel32::SetEnvironmentVariableW(w "FBE_SHELL_CLEANUP_CURRENT", w "$ShellToolsDirectory")'
  ${If} ${RunningX64}
    nsExec::ExecToStack '"$WINDIR\sysnative\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -Command "& { $$ErrorActionPreference=1; . $$env:FBE_SHELL_CLEANUP_LOADER; Remove-ShellMaintenanceSessions -MaintenanceRoot $$env:FBE_SHELL_CLEANUP_ROOT -CompletedDirectory $$env:FBE_SHELL_CLEANUP_CURRENT }"'
  ${Else}
    nsExec::ExecToStack '"$WINDIR\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -Command "& { $$ErrorActionPreference=1; . $$env:FBE_SHELL_CLEANUP_LOADER; Remove-ShellMaintenanceSessions -MaintenanceRoot $$env:FBE_SHELL_CLEANUP_ROOT -CompletedDirectory $$env:FBE_SHELL_CLEANUP_CURRENT }"'
  ${EndIf}
  Pop $0
  Pop $1
  DetailPrint "Maintenance cleanup exit=$0: $1"
  System::Call 'kernel32::SetEnvironmentVariableW(w "FBE_SHELL_CLEANUP_ROOT", p 0)'
  System::Call 'kernel32::SetEnvironmentVariableW(w "FBE_SHELL_CLEANUP_LOADER", p 0)'
  System::Call 'kernel32::SetEnvironmentVariableW(w "FBE_SHELL_CLEANUP_CURRENT", p 0)'
  SetErrorLevel $8
  !endif
FunctionEnd
!macroend
!insertmacro FBE_MaintenanceFunctions ""
!insertmacro FBE_MaintenanceFunctions "un."

Function CapturePreviousShellState
  StrCpy $ShellPreviousProperty 0
  StrCpy $ShellPreviousSystem 0
  ReadRegStr $ShellPreviousLocation ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "InstallLocation"
  ${If} $ShellPreviousLocation == $INSTDIR
    ReadRegDWORD $ShellPreviousProperty ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "PropertyHandlerInstalled"
    ReadRegDWORD $ShellPreviousSystem ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "${PRODUCT_SYSTEM_INTEGRATION_REGVAL}"
  ${EndIf}
  ${If} $ShellPreviousProperty != 1
    StrCpy $ShellPreviousProperty 0
  ${EndIf}
  ${If} $ShellPreviousSystem != 1
    StrCpy $ShellPreviousSystem 0
  ${EndIf}
FunctionEnd

Function un.UnregisterModernPropertyHandler
  StrCpy $ShellOperation "Remove"
  StrCpy $ShellPreviousProperty $0
  ReadRegDWORD $ShellPreviousSystem ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "${PRODUCT_SYSTEM_INTEGRATION_REGVAL}"
  ReadRegStr $ShellPreviousLocation ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "InstallLocation"
  ${If} $ShellPreviousProperty != 1
    StrCpy $ShellPreviousProperty 0
  ${EndIf}
  ${If} $ShellPreviousSystem != 1
    StrCpy $ShellPreviousSystem 0
  ${EndIf}
  Call un.StageShellMaintenance
  StrCmp $ShellStageReady 1 +2
    Return
  StrCpy $0 -1
  ClearErrors
  ${If} ${RunningX64}
    ExecWait '"$WINDIR\sysnative\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -File "$ShellToolsDirectory\manage-shell-integration.ps1" -Operation Remove -InstallationDirectory "$INSTDIR" -PreviousProperty $ShellPreviousProperty -PreviousSystem $ShellPreviousSystem' $0
  ${Else}
    ExecWait '"$WINDIR\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -File "$ShellToolsDirectory\manage-shell-integration.ps1" -Operation Remove -InstallationDirectory "$INSTDIR" -PreviousProperty $ShellPreviousProperty -PreviousSystem $ShellPreviousSystem' $0
  ${EndIf}
  Call un.ReadShellMaintenanceResult
FunctionEnd

; ArchHandler belonged to older FBE releases.  It registered only its own
; OpenWith entries, ProgIDs and capability root under HKCU; never touch the
; extension default value or UserChoice because those belong to the user.
Function CleanupLegacyArchHandlerRegistration
  ; Old releases placed only ArchHandler here.  Do not remove Utilities itself:
  ; it may contain other current or user-supplied utilities.
  RMDir /r "$INSTDIR\Utilities\ArchHandler"
  ${If} ${UAC_IsInnerInstance}
    !insertmacro UAC_AsUser_Call Function CleanupLegacyArchHandlerUserRegistration ${UAC_SYNCREGISTERS}
  ${Else}
    Call CleanupLegacyArchHandlerUserRegistration
  ${EndIf}
FunctionEnd

Function CleanupLegacyArchHandlerUserRegistration
  DeleteRegValue HKCU "Software\Classes\.zip\OpenWithProgids" "FictionBookEditor.ArchHandler.zip"
  DeleteRegValue HKCU "Software\Classes\.rar\OpenWithProgids" "FictionBookEditor.ArchHandler.rar"
  DeleteRegKey HKCU "Software\Classes\FictionBookEditor.ArchHandler.zip"
  DeleteRegKey HKCU "Software\Classes\FictionBookEditor.ArchHandler.rar"
  DeleteRegValue HKCU "Software\RegisteredApplications" "FictionBook Editor ArchHandler"
  DeleteRegKey HKCU "Software\FictionBook Editor\ArchHandler"
FunctionEnd

Function un.CleanupLegacyArchHandlerRegistration
  ${If} ${UAC_IsInnerInstance}
    !insertmacro UAC_AsUser_Call Function un.CleanupLegacyArchHandlerUserRegistration ${UAC_SYNCREGISTERS}
    Return
  ${EndIf}
  Call un.CleanupLegacyArchHandlerUserRegistration
FunctionEnd

Function un.CleanupLegacyArchHandlerUserRegistration
  DeleteRegValue HKCU "Software\Classes\.zip\OpenWithProgids" "FictionBookEditor.ArchHandler.zip"
  DeleteRegValue HKCU "Software\Classes\.rar\OpenWithProgids" "FictionBookEditor.ArchHandler.rar"
  DeleteRegKey HKCU "Software\Classes\FictionBookEditor.ArchHandler.zip"
  DeleteRegKey HKCU "Software\Classes\FictionBookEditor.ArchHandler.rar"
  DeleteRegValue HKCU "Software\RegisteredApplications" "FictionBook Editor ArchHandler"
  DeleteRegKey HKCU "Software\FictionBook Editor\ArchHandler"
FunctionEnd

Function WriteDeploymentScopeTestProbe
  !ifdef FBE_DEPLOYMENT_TEST_SCOPE_TRANSITIONS
    Call VerifyScopePathTransitions
  !endif
  CreateDirectory "$INSTDIR"
  FileOpen $0 "$INSTDIR\deployment-scope.txt" w
  FileWrite $0 "DeploymentMode=$DeploymentMode$\r$\nInstallScope=$InstallScope$\r$\nInstallPath=$INSTDIR$\r$\nShellRecoveryRequested=$ShellRecoveryRequested$\r$\n"
  !ifdef FBE_DEPLOYMENT_TEST_SCOPE_TRANSITIONS
    FileWrite $0 "ScopeTransitions=$ScopeTransitionResult$\r$\n"
  !endif
  ${If} $InstallScope == "allusers"
    FileWrite $0 "UninstallRegistryRoot=HKLM$\r$\nProductionPath=$PROGRAMFILES32\${PRODUCT_NAME}$\r$\n"
  ${Else}
    FileWrite $0 "UninstallRegistryRoot=HKCU$\r$\nProductionPath=$LOCALAPPDATA\Programs\${PRODUCT_NAME}$\r$\n"
  ${EndIf}
  ${If} $DeploymentMode == "portable"
    FileOpen $1 "$INSTDIR\portable.ini" w
    FileWrite $1 "[Portable]$\r$\nDataPath=Data$\r$\n"
    FileClose $1
    CreateDirectory "$INSTDIR\Data\Settings"
    CreateDirectory "$INSTDIR\Data\Scripts"
    CreateDirectory "$INSTDIR\Data\Dictionaries"
    CreateDirectory "$INSTDIR\Data\Themes"
    CreateDirectory "$INSTDIR\Data\Logs"
    CreateDirectory "$INSTDIR\Data\Diagnostics"
    CreateDirectory "$INSTDIR\Data\Recovery"
    CreateDirectory "$INSTDIR\Data\Cache"
    CreateDirectory "$INSTDIR\Data\Temp"
  ${EndIf}
  FileClose $0
FunctionEnd

!ifdef FBE_DEPLOYMENT_TEST_SCOPE_PROBE
; MUI pages remain declared above; these no-op callbacks let the probe compile
; without parsing production payload and integration sections below.
Function ComponentsPageLeave
FunctionEnd
Function ComponentsPageShow
FunctionEnd
Section "Deployment scope probe"
  Call WriteDeploymentScopeTestProbe
  SetErrorLevel 0
  Quit
SectionEnd
Function DisablePortableIntegration
FunctionEnd

!ifndef FBE_DEPLOYMENT_TEST_SCOPE_PROBE
Function SaveInstalledModeState
  ${If} $InstalledStateSaved == "1"
    Return
  ${EndIf}
  StrCpy $InstalledStateSaved "1"
  StrCpy $SavedInstallScope "$InstallScope"
  StrCpy $SavedInstallDirectory "$INSTDIR"
  SectionGetFlags ${System_Integration_id} $SavedSystemIntegrationFlags
  SectionGetFlags ${FB2_File_Association_id} $SavedFb2AssociationFlags
  SectionGetFlags ${FBD_File_Association_id} $SavedFbdAssociationFlags
  SectionGetFlags ${FB2_Validate_Command_id} $SavedValidateFlags
  SectionGetFlags ${FB2_Explorer_Properties_id} $SavedExplorerFlags
  SectionGetFlags ${ShCutGroup_id} $SavedShortcutsFlags
  SectionGetFlags ${Start_Menu_ShortCuts_id} $SavedStartMenuFlags
  SectionGetFlags ${Desktop_ShortCut_id} $SavedDesktopFlags
FunctionEnd

Function RestoreInstalledModeState
  ${If} $InstalledStateSaved != "1"
    Return
  ${EndIf}
  StrCpy $InstallScope "$SavedInstallScope"
  StrCpy $INSTDIR "$SavedInstallDirectory"
  ${If} $InstallScope == "allusers"
    SetShellVarContext all
  ${Else}
    SetShellVarContext current
  ${EndIf}
  SectionSetFlags ${System_Integration_id} $SavedSystemIntegrationFlags
  SectionSetFlags ${FB2_File_Association_id} $SavedFb2AssociationFlags
  SectionSetFlags ${FBD_File_Association_id} $SavedFbdAssociationFlags
  SectionSetFlags ${FB2_Validate_Command_id} $SavedValidateFlags
  SectionSetFlags ${FB2_Explorer_Properties_id} $SavedExplorerFlags
  SectionSetFlags ${ShCutGroup_id} $SavedShortcutsFlags
  SectionSetFlags ${Start_Menu_ShortCuts_id} $SavedStartMenuFlags
  SectionSetFlags ${Desktop_ShortCut_id} $SavedDesktopFlags
  StrCpy $InstalledStateSaved "0"
FunctionEnd
!endif
Function VerifyScopePathTransitions
  ; Current User custom path -> All Users -> Current User.
  StrCpy $SessionCurrentInstallLocation "C:\NSIS-test\Current-custom"
  StrCpy $SessionAllUsersInstallLocation "D:\NSIS-test\All-custom"
  StrCpy $InstallScope "current"
  StrCpy $INSTDIR "$SessionCurrentInstallLocation"
  StrCpy $0 "allusers"
  Call ApplyInstallScopeSelection
  StrCmp $INSTDIR "D:\NSIS-test\All-custom" 0 transition_failed
  StrCpy $0 "current"
  Call ApplyInstallScopeSelection
  StrCmp $INSTDIR "C:\NSIS-test\Current-custom" 0 transition_failed
  ; Mirror All Users path and a Directory-page manual path round trip.
  StrCpy $InstallScope "allusers"
  StrCpy $INSTDIR "$SessionAllUsersInstallLocation"
  StrCpy $0 "current"
  Call ApplyInstallScopeSelection
  StrCmp $INSTDIR "C:\NSIS-test\Current-custom" 0 transition_failed
  StrCpy $0 "allusers"
  Call ApplyInstallScopeSelection
  StrCmp $INSTDIR "D:\NSIS-test\All-custom" 0 transition_failed
  StrCpy $InstallScope "current"
  StrCpy $INSTDIR "E:\NSIS-test\Manual-current"
  StrCpy $0 "allusers"
  Call ApplyInstallScopeSelection
  StrCpy $0 "current"
  Call ApplyInstallScopeSelection
  StrCmp $INSTDIR "E:\NSIS-test\Manual-current" 0 transition_failed
  StrCpy $ScopeTransitionResult "passed"
  StrCpy $INSTDIR "$EXEDIR"
  Return
transition_failed:
  StrCpy $ScopeTransitionResult "failed"
  StrCpy $INSTDIR "$EXEDIR"
  Return
FunctionEnd
!else
Section !$(Main) MainSection_id
  SectionIn RO
  ReadRegStr $0 HKLM "SOFTWARE\Microsoft\Windows NT\CurrentVersion" CurrentVersion
  StrCmp $0 "" 0 nthere
  MessageBox MB_OK|MB_ICONSTOP $(ErrNTCurrentVersion)
  Quit
nthere:
  Call CheckMSXMLVersion
  Call CheckIEVersion
  Call CheckFBERunning
  Call CapturePreviousShellState
  Call ComponentsPageLeave

  !insertmacro UnregisterPreviousBundledPluginIfOwned "{3C19F5A2-2EC8-4EC7-B7A9-F4910B4CDD82}" "ImportEPUB.dll"
  !insertmacro UnregisterPreviousBundledPluginIfOwned "{C3098839-EF69-4DE5-B27D-1E80051CA843}" "ExportHTML.dll"
  !insertmacro UnregisterPreviousBundledPluginIfOwned "{09B5ABFF-177E-4C03-98D0-9EF4E1C9DB56}" "ExportDOCX.dll"
  !insertmacro UnregisterPreviousBundledPluginIfOwned "{36FCFB2D-C3D8-4B81-ABC1-5A09CA846515}" "ExportEPUB.dll"
  Delete "$INSTDIR\ImportEPUB.dll"
  Delete "$INSTDIR\ExportHTML.dll"
  Delete "$INSTDIR\ExportDOCX.dll"
  Delete "$INSTDIR\ExportEPUB.dll"
  Delete "$INSTDIR\ImportEPUBLunaSVG.dll"

  SetOutPath "$INSTDIR"
  File "${INPUTDIR}\symbols.ini"
  File "${INPUTDIR}\blank.fb2"
  File "${INPUTDIR}\fb2.xsl"
  File "${INPUTDIR}\eng.xsl"
  File "${INPUTDIR}\rus.xsl"
  File "${INPUTDIR}\ukr.xsl"
  File "${INPUTDIR}\html.xsl"
  File "${INPUTDIR}\FBE.exe"
  File "${INPUTDIR}\FictionBook.xsd"
  File "${INPUTDIR}\FictionBookGenres.xsd"
  File "${INPUTDIR}\FictionBookLang.xsd"
  File "${INPUTDIR}\FictionBookLinks.xsd"
  File "${INPUTDIR}\genres.txt"
  File "${INPUTDIR}\genres.rus.txt"
  File "${INPUTDIR}\genres.txt_L"
  File "${INPUTDIR}\genres.rus.txt_L"
  File "${INPUTDIR}\genres.ukr.txt"
  File "${INPUTDIR}\main.css"
  File "${INPUTDIR}\main_fast.css"
  File "${INPUTDIR}\main.html"
  File "${INPUTDIR}\main.js"
  File "${INPUTDIR}\Scintilla.dll"
  File "${INPUTDIR}\Lexilla.dll"
  File "${INPUTDIR}\FBV.exe"
  ; До 3.0.4 MUI host и спутники находились в корне приложения. Очищаем
  ; только свои старые файлы, чтобы обновление не оставляло дубликаты.
  Delete "$INSTDIR\FBVVerbResources.dll"
  Delete "$INSTDIR\en-US\FBVVerbResources.dll.mui"
  Delete "$INSTDIR\ru-RU\FBVVerbResources.dll.mui"
  Delete "$INSTDIR\uk-UA\FBVVerbResources.dll.mui"
  Delete "$INSTDIR\de-DE\FBVVerbResources.dll.mui"
  Delete "$INSTDIR\fr-FR\FBVVerbResources.dll.mui"
  Delete "$INSTDIR\es-ES\FBVVerbResources.dll.mui"
  Delete "$INSTDIR\it-IT\FBVVerbResources.dll.mui"
  Delete "$INSTDIR\pl-PL\FBVVerbResources.dll.mui"
  Delete "$INSTDIR\cs-CZ\FBVVerbResources.dll.mui"
  Delete "$INSTDIR\bg-BG\FBVVerbResources.dll.mui"
  Delete "$INSTDIR\pt-PT\FBVVerbResources.dll.mui"
  Delete "$INSTDIR\nl-NL\FBVVerbResources.dll.mui"
  StrCmp $DeploymentMode "portable" skip_shell_mui
  SetOutPath "$INSTDIR\Lang\Shell"
  File "${INPUTDIR}\Lang\Shell\FBVVerbResources.dll"
skip_shell_mui:
  SetOutPath "$INSTDIR"
  File /nonfatal "${INPUTDIR}\*.reg"
  File "${INPUTDIR}\gpl-3.0.ru.txt"
	File "${INPUTDIR}\gpl-3.0.ua.txt"
	File "${INPUTDIR}\contributors.txt"
	File "${INPUTDIR}\LICENSE"
	File "${INPUTDIR}\NOTICE"
	File "${INPUTDIR}\THIRD-PARTY-NOTICES.md"
	SetOutPath "$INSTDIR\THIRD-PARTY-LICENSES"
	File /r "${INPUTDIR}\THIRD-PARTY-LICENSES\*.*"
	; Resources are immutable program seeds. CSettings copies Words.xml to the
	; active user settings directory on first use instead of writing beside FBE.exe.
	; Remove the pre-3.0.9 location during an upgrade; the executable retains a
	; legacy fallback only for old standalone deployments.
	Delete "$INSTDIR\defaults\Words.xml"
	RMDir "$INSTDIR\defaults"
	SetOutPath "$INSTDIR\Resources"
	File "${INPUTDIR}\Resources\Words.xml"
	SetOutPath "$INSTDIR"
  ; Bundled plug-ins are discovered from this local manifest. The DLLs are
  ; optional components below, but the catalog itself is part of the editor
  ; runtime and must always be installed.
  SetOutPath "$INSTDIR\Plugins"
  File "${INPUTDIR}\Plugins\plugins.json"
  SetOutPath "$INSTDIR"

  StrCmp $DeploymentMode "portable" portable_core_done installed_core_state
portable_core_done:
  FileOpen $0 "$INSTDIR\portable.ini" w
  FileWrite $0 "[Portable]$\r$\nDataPath=Data$\r$\n"
  FileClose $0
  CreateDirectory "$INSTDIR\Data\Settings"
  CreateDirectory "$INSTDIR\Data\Scripts"
  CreateDirectory "$INSTDIR\Data\Dictionaries"
  CreateDirectory "$INSTDIR\Data\Themes"
  CreateDirectory "$INSTDIR\Data\Logs"
  CreateDirectory "$INSTDIR\Data\Diagnostics"
  CreateDirectory "$INSTDIR\Data\Recovery"
  CreateDirectory "$INSTDIR\Data\Cache"
  CreateDirectory "$INSTDIR\Data\Temp"
  Goto main_section_done

installed_core_state:
  Call CleanupLegacyArchHandlerRegistration
  ; Retain ownership when an upgrade leaves an optional component unchecked.
  ; Uninstall still checks each live command/icon before removing it.
  StrCpy $PreviousAssociationOwned 0
  StrCpy $PreviousValidateOwned 0
  ReadRegStr $0 ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "InstallLocation"
  ${If} $0 == $INSTDIR
    ReadRegDWORD $PreviousAssociationOwned ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "AssociationRegistered"
    ReadRegDWORD $PreviousValidateOwned ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "ValidateVerbInstalled"
  ${EndIf}
  ; uninstall info must exist for any successful installation, not only when
  ; optional system integration is selected
  ; Keep the removal command available if later shell maintenance needs a retry.
  Call WriteUninstallDisplayMetadata
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}" "CoreVersion" "${PRODUCT_VERSION}"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}" "InstallScope" "$InstallScope"
  WriteRegExpandStr ${PRODUCT_UNINST_ROOT_KEY} "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}" "InstallLocation" "$INSTDIR"
	WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "SOFTWARE\${PRODUCT_VENDOR}\${PRODUCT_NAME}" "InstallLocation" "$INSTDIR"
	WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "SOFTWARE\${PRODUCT_VENDOR}\${PRODUCT_NAME}" "CoreVersion" "${PRODUCT_VERSION}"
  WriteRegExpandStr ${PRODUCT_UNINST_ROOT_KEY} "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}" "DisplayIcon" "$INSTDIR\FBE.exe,0"
  WriteRegExpandStr ${PRODUCT_UNINST_ROOT_KEY} "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}" "Publisher" "${PRODUCT_VENDOR}"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}" "URLInfoAbout" "${PRODUCT_URL}"
  WriteRegDWORD ${PRODUCT_UNINST_ROOT_KEY} "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}" "NoModify" 1
  WriteRegDWORD ${PRODUCT_UNINST_ROOT_KEY} "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}" "NoRepair" 1
  WriteRegDWORD ${PRODUCT_UNINST_ROOT_KEY} "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}" "${PRODUCT_SYSTEM_INTEGRATION_REGVAL}" $ShellPreviousSystem
  ; Component state is deliberately kept next to the uninstall record.  The
  ; uninstaller must never infer ownership solely from a well-known CLSID or
  ; ProgID: another installed instance may have claimed it after this setup.
  WriteRegDWORD ${PRODUCT_UNINST_ROOT_KEY} "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}" "AssociationRegistered" $PreviousAssociationOwned
  WriteRegDWORD ${PRODUCT_UNINST_ROOT_KEY} "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}" "ValidateVerbInstalled" $PreviousValidateOwned
  WriteRegDWORD ${PRODUCT_UNINST_ROOT_KEY} "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}" "PropertyHandlerInstalled" $ShellPreviousProperty
  WriteUninstaller "$INSTDIR\uninst.exe"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "SOFTWARE\${PRODUCT_VENDOR}\${PRODUCT_NAME}" "InstallDir" "$INSTDIR"

main_section_done:
  SetAutoClose false
SectionEnd

!include "Generated\LanguagePacks.generated.nsh"

SectionGroup /e $(System_Integration) System_Integration_id

Section /o $(FB2_File_Association) FB2_File_Association_id
  ; Регистрируем typelib для автоматизации редактора и исторической совместимости.
  Push "$INSTDIR\FBE.exe"
  Call RegisterTlb

  ; Создаём FB2 ProgID.
  WriteRegStr SHCTX "Software\Classes\FictionBook.2" "" "FictionBook"
  WriteRegStr SHCTX "Software\Classes\FictionBook.2\CurVer" "" "FictionBook.2"

  ; Создаём тип файла FB2.
  WriteRegStr SHCTX "Software\Classes\.fb2" "" "FictionBook.2"
  WriteRegStr SHCTX "Software\Classes\.fb2" "PerceivedType" "Text"
  WriteRegStr SHCTX "Software\Classes\.fb2" "Content Type" "text/xml"
  WriteRegStr SHCTX "Software\Classes\.fb2\DefaultIcon" "" "$INSTDIR\FBE.exe,0"
  WriteRegStr SHCTX "Software\Classes\FictionBook.2\DefaultIcon" "" "$INSTDIR\FBE.exe,0"
  WriteRegStr SHCTX "Software\Classes\FictionBook.2\shell\Edit\Command" "" '"$INSTDIR\FBE.exe" "%1"'
  WriteRegDWORD ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "AssociationRegistered" 1
SectionEnd

Section /o $(FBD_File_Association) FBD_File_Association_id
  ; FBD is metadata-oriented.  Keep it isolated from FB2-only validation and
  ; Explorer extensions by assigning a dedicated ProgID.
  ; Remember the original default once, so uninstall can restore it without
  ; deleting unrelated values from the extension key.
  ReadRegStr $0 SHCTX "Software\Classes\.fbd" ""
  StrCmp $0 "FictionBook.Description" fbd_association_backup_done
  ReadRegStr $1 SHCTX "SOFTWARE\${PRODUCT_VENDOR}\${PRODUCT_NAME}\FbdAssociation" "Captured"
  StrCmp $1 "1" fbd_association_backup_done
  WriteRegStr SHCTX "SOFTWARE\${PRODUCT_VENDOR}\${PRODUCT_NAME}\FbdAssociation" "PreviousProgId" "$0"
  WriteRegStr SHCTX "SOFTWARE\${PRODUCT_VENDOR}\${PRODUCT_NAME}\FbdAssociation" "Captured" "1"
fbd_association_backup_done:
  WriteRegStr SHCTX "Software\Classes\FictionBook.Description" "" "FictionBook Description"
  WriteRegStr SHCTX "Software\Classes\.fbd" "" "FictionBook.Description"
  WriteRegStr SHCTX "Software\Classes\.fbd" "PerceivedType" "Text"
  WriteRegStr SHCTX "Software\Classes\.fbd" "Content Type" "text/xml"
  WriteRegStr SHCTX "Software\Classes\.fbd\DefaultIcon" "" "$INSTDIR\FBE.exe,0"
  WriteRegStr SHCTX "Software\Classes\FictionBook.Description\DefaultIcon" "" "$INSTDIR\FBE.exe,0"
  WriteRegStr SHCTX "Software\Classes\FictionBook.Description\shell\Edit\Command" "" '"$INSTDIR\FBE.exe" "%1"'
  WriteRegDWORD ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "AssociationRegistered" 1
SectionEnd

Section /o $(FB2_Validate_Command) FB2_Validate_Command_id
  ; Добавляем команду проверки, не отбирая .fb2 у другой читалки.
  WriteRegStr SHCTX "${FB2_SYSTEM_ASSOC_KEY}\shell\Validate" "" "Validate"
  WriteRegStr SHCTX "${FB2_SYSTEM_ASSOC_KEY}\shell\Validate" "MUIVerb" '@$INSTDIR\Lang\Shell\FBVVerbResources.dll,-109;v2'
  WriteRegStr SHCTX "${FB2_SYSTEM_ASSOC_KEY}\shell\Validate" "Icon" '"$INSTDIR\FBV.exe",0'
  WriteRegStr SHCTX "${FB2_SYSTEM_ASSOC_KEY}\shell\Validate\Command" "" '"$INSTDIR\FBV.exe" "%1"'

  ; Сохраняем совместимость для систем, где .fb2 явно связан с FictionBook.2.
  WriteRegStr SHCTX "Software\Classes\FictionBook.2\shell\Validate" "" "Validate"
  WriteRegStr SHCTX "Software\Classes\FictionBook.2\shell\Validate" "MUIVerb" '@$INSTDIR\Lang\Shell\FBVVerbResources.dll,-109;v2'
  WriteRegStr SHCTX "Software\Classes\FictionBook.2\shell\Validate" "Icon" '"$INSTDIR\FBV.exe",0'
  WriteRegStr SHCTX "Software\Classes\FictionBook.2\shell\Validate\Command" "" '"$INSTDIR\FBV.exe" "%1"'
  WriteRegDWORD ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "ValidateVerbInstalled" 1
SectionEnd

Section /o $(FB2_Explorer_Properties) FB2_Explorer_Properties_id
  StrCpy $ShellOperation "Update"
  Call MaintainShellIntegration
SectionEnd

SectionGroupEnd

Section "-ReconcileShellIntegration"
  StrCmp $DeploymentMode "portable" reconcile_shell_done
  SectionGetFlags ${FB2_Explorer_Properties_id} $0
  IntOp $0 $0 & ${SF_SELECTED}
  StrCmp $0 0 0 reconcile_shell_done
  ReadINIStr $0 "${FBE_SHELL_SHARED_DIR}\Owner.ini" "Owner" "InstallDirectory"
  ReadRegDWORD $1 ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "ShellRecoveryPending"
  ${If} $ShellPreviousProperty == 1
  ${OrIf} $ShellPreviousSystem == 1
  ${OrIf} $1 == 1
  ${OrIf} $0 == $INSTDIR
    StrCpy $ShellOperation "Remove"
    Call MaintainShellIntegration
  ${EndIf}
reconcile_shell_done:
SectionEnd

Function MaintainShellIntegration
  ${If} $ShellRecoveryRequested == "1"
    StrCpy $ShellOperation "Recover"
  ${EndIf}
  Call StageShellMaintenance
  StrCmp $ShellStageReady 1 +2
    Return
  StrCpy $0 -1
  ClearErrors
  ${If} ${RunningX64}
    ExecWait '"$WINDIR\sysnative\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -File "$ShellToolsDirectory\manage-shell-integration.ps1" -Operation $ShellOperation -InstallationDirectory "$INSTDIR" -ExpectedX86Sha256 "${FBE_SHELL_X86_SHA256}" -ExpectedX64Sha256 "${FBE_SHELL_X64_SHA256}" -ExpectedSchemaSha256 "${FBE_SCHEMA_SHA256}" -PreviousProperty $ShellPreviousProperty -PreviousSystem $ShellPreviousSystem' $0
  ${Else}
    ExecWait '"$WINDIR\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -File "$ShellToolsDirectory\manage-shell-integration.ps1" -Operation $ShellOperation -InstallationDirectory "$INSTDIR" -ExpectedX86Sha256 "${FBE_SHELL_X86_SHA256}" -ExpectedX64Sha256 "${FBE_SHELL_X64_SHA256}" -ExpectedSchemaSha256 "${FBE_SCHEMA_SHA256}" -PreviousProperty $ShellPreviousProperty -PreviousSystem $ShellPreviousSystem' $0
  ${EndIf}
  Call ReadShellMaintenanceResult
FunctionEnd

Function ComponentsPageLeave
  ${If} $DeploymentMode == "portable"
    Return
  ${EndIf}
  Call EnsureAllUsersElevation

  SectionGetFlags ${FB2_Explorer_Properties_id} $0
  IntOp $0 $0 & ${SF_SELECTED}
  ${If} $0 = 0
    ReadRegDWORD $0 ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "${PRODUCT_SYSTEM_INTEGRATION_REGVAL}"
    ReadRegDWORD $1 ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "ShellRecoveryPending"
    IntOp $0 $0 | $1
    ReadINIStr $1 "${FBE_SHELL_SHARED_DIR}\Owner.ini" "Owner" "InstallDirectory"
    ${If} $0 != 1
    ${AndIf} $1 != $INSTDIR
      Return
    ${EndIf}
  ${EndIf}

  ${If} ${UAC_IsAdmin}
    Return
  ${EndIf}

  GetDlgItem $9 $HWNDParent 1
  System::Call 'user32::GetFocus() i .s'
  EnableWindow $9 0
  !insertmacro UAC_PageElevation_RunElevated
  EnableWindow $9 1
  System::Call 'user32::SetFocus(is)'

  ${If} $2 = 0x666666
    MessageBox MB_OK|MB_ICONEXCLAMATION $(UacAbortInstaller)
    Abort
  ${ElseIf} $0 = 1223
    Abort
  ${ElseIf} $0 = 1062
    MessageBox MB_OK|MB_ICONSTOP $(UacLogonServiceInstaller)
    Abort
  ${ElseIf} $0 <> 0
    MessageBox MB_OK|MB_ICONSTOP "$(UacUnknownError) $0"
    Abort
  ${EndIf}

  Quit
FunctionEnd

Function ComponentsPageShow
  ; Стандартный modern_smalldesc оставляет описанию лишь две строки. Увеличиваем
  ; поле вниз на 16 пикселей: место есть до нижнего разделителя, а длинные
  ; локализованные пояснения теперь читаются без обрезания.
  GetDlgItem $0 $HWNDPARENT 1043
  System::Call 'user32::GetWindowRect(p r0, *i .r1, *i .r2, *i .r3, *i .r4)'
  IntOp $3 $3 - $1
  IntOp $4 $4 - $2
  IntOp $4 $4 + 16
  System::Call 'user32::SetWindowPos(p r0, p 0, i 0, i 0, i r3, i r4, i 0x0006)'
FunctionEnd

Function CreateStartMenuShortcuts
  ${If} $DeploymentMode == "portable"
    Return
  ${EndIf}
  !insertmacro MUI_STARTMENU_WRITE_BEGIN Application
  CreateDirectory "$SMPROGRAMS\$ICONS_GROUP"
  CreateShortCut "$SMPROGRAMS\$ICONS_GROUP\${PRODUCT_SHORTCUT_NAME}.lnk" "$INSTDIR\FBE.exe"
  CreateShortCut "$SMPROGRAMS\$ICONS_GROUP\Uninstall ${PRODUCT_SHORTCUT_NAME}.lnk" "$INSTDIR\uninst.exe"
  !insertmacro MUI_STARTMENU_WRITE_END
FunctionEnd

Function CreateDesktopShortcut
  ${If} $DeploymentMode == "portable"
    Return
  ${EndIf}
  !insertmacro MUI_STARTMENU_WRITE_BEGIN Application
  CreateShortCut "$DESKTOP\${PRODUCT_SHORTCUT_NAME}.lnk" "$INSTDIR\FBE.exe"
  !insertmacro MUI_STARTMENU_WRITE_END
FunctionEnd

; UAC copies registers, not arbitrary globals. The elevated page can change
; the menu group after elevation; pass that current selection on every call.
Function CreateStartMenuShortcutsAsUser
  StrCpy $ICONS_GROUP $R7
  Call CreateStartMenuShortcuts
FunctionEnd
Function CreateDesktopShortcutAsUser
  StrCpy $ICONS_GROUP $R7
  Call CreateDesktopShortcut
FunctionEnd

SectionGroup /e !$(ShCutGroup) ShCutGroup_id
; Shortcuts
	Section $(Start_Menu_ShortCuts) Start_Menu_ShortCuts_id
		${If} $InstallScope == "allusers"
			Call CreateStartMenuShortcuts
		${Else}
			StrCpy $R7 $ICONS_GROUP
			!insertmacro UAC_AsUser_Call Function CreateStartMenuShortcutsAsUser ${UAC_SYNCREGISTERS}|${UAC_SYNCOUTDIR}|${UAC_SYNCINSTDIR}
		${EndIf}
	SectionEnd	
	Section /o $(Desktop_ShortCut) Desktop_ShortCut_id
		${If} $InstallScope == "allusers"
			Call CreateDesktopShortcut
		${Else}
			StrCpy $R7 $ICONS_GROUP
			!insertmacro UAC_AsUser_Call Function CreateDesktopShortcutAsUser ${UAC_SYNCREGISTERS}|${UAC_SYNCOUTDIR}|${UAC_SYNCINSTDIR}
		${EndIf}
	SectionEnd
SectionGroupEnd

SectionGroup /e !$(PluginsGroup) PluginsGroup_id
; Plugins
	SectionGroup /e !$(ImportPluginsGroup) ImportPluginsGroup_id
		Section $(Plugin_ImportEPUB) ImportEPUB_Plugin_id
			SetOutPath "$INSTDIR\Plugins"
			File "${INPUTDIR}\Plugins\ImportEPUB.dll"
		SectionEnd
		Section /o $(Plugin_ImportEPUB_SVG) ImportEPUB_SVG_id
			SetOutPath "$INSTDIR\Plugins"
			File "${INPUTDIR}\Plugins\ImportEPUBLunaSVG.dll"
		SectionEnd
	SectionGroupEnd

	SectionGroup /e !$(ExportPluginsGroup) ExportPluginsGroup_id
		Section $(Plugin_ExportHTML) ExportHTML_Plugin_id
			SetOutPath "$INSTDIR\Plugins"
			File "${INPUTDIR}\Plugins\ExportHTML.dll"
		SectionEnd
		Section $(Plugin_ExportDOCX) ExportDOCX_Plugin_id
			SetOutPath "$INSTDIR\Plugins"
			File "${INPUTDIR}\Plugins\ExportDOCX.dll"
		SectionEnd
		Section $(Plugin_ExportEPUB) ExportEPUB_Plugin_id
			SetOutPath "$INSTDIR\Plugins"
			File "${INPUTDIR}\Plugins\ExportEPUB.dll"
		SectionEnd
	SectionGroupEnd

	Section /o $(Plugin_BatchConverters) BatchConverters_id
		SetOutPath "$INSTDIR"
		File "${INPUTDIR}\ExportDOCXBatch.exe"
		File "${INPUTDIR}\ExportEPUBBatch.exe"
		File "${INPUTDIR}\ImportEPUBBatch.exe"

		; Консольные batch-конвертеры используют те же библиотеки, что и GUI-плагины.
		; Копируем зависимости здесь тоже, чтобы секция была самодостаточной, даже если
		; пользователь не выбрал соответствующие GUI-плагины.
		SetOutPath "$INSTDIR\Plugins"
		File "${INPUTDIR}\Plugins\ExportDOCX.dll"
		File "${INPUTDIR}\Plugins\ExportEPUB.dll"
		File "${INPUTDIR}\Plugins\ImportEPUB.dll"
	SectionEnd
SectionGroupEnd

Section !$(Scripts) Scripts_id
;Scripts and dependances
	SetOutPath "$INSTDIR\Scripts"
	File /r ${INPUTDIR}\Scripts\*.*
	SetOutPath "$INSTDIR\TreeCmd"
	File /r ${INPUTDIR}\TreeCmd\*.*
	SetOutPath "$INSTDIR\HTML"
	File /r ${INPUTDIR}\HTML\*.*
        SetOutPath "$INSTDIR\Help"
	File /r ${INPUTDIR}\Help\*.*
	SetOutPath "$INSTDIR\Utilities"
	File /r ${INPUTDIR}\Utilities\*.*
	SetOutPath "$INSTDIR\Themes"
	File /r ${INPUTDIR}\Themes\*.*
	SetOutPath "$INSTDIR\Backgrounds"
	File /r ${INPUTDIR}\Backgrounds\*.*
SectionEnd

SubSection !$(Dictionaries) Dictionaries_id

        Section $(EnglishDict) Dict01
      SectionIn RO
 	  SetOutPath "$INSTDIR\Dict"
	  File "${INPUTDIR}\Dict\en_US.dic"
	  File "${INPUTDIR}\Dict\en_US.aff"
        SectionEnd

        Section $(RussianDict) Dict02
      SectionIn RO
 	  SetOutPath "$INSTDIR\Dict"
	  File "${INPUTDIR}\Dict\ru_RU.dic"
	  File "${INPUTDIR}\Dict\ru_RU.aff"
        SectionEnd

        Section /o $(UkrainianDict) Dict03
	  SetOutPath "$INSTDIR\Dict"
	  File "${INPUTDIR}\Dict\uk_UA.dic"
	  File "${INPUTDIR}\Dict\uk_UA.aff"
        SectionEnd

        Section /o $(GermanDict) Dict08
	  SetOutPath "$INSTDIR\Dict"
	  File "${INPUTDIR}\Dict\de_DE.dic"
	  File "${INPUTDIR}\Dict\de_DE.aff"
        SectionEnd

SubSectionEnd

Function WriteUninstallDisplayMetadata
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayName" "${PRODUCT_NAME}"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayVersion" "${PRODUCT_VER_NUM}"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "UninstallString" '$\"$INSTDIR\uninst.exe$\"'
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "QuietUninstallString" '$\"$INSTDIR\uninst.exe$\" /S'
  ; GetTime returns zero-padded day/month and the local installation date.
  ${GetTime} "" "L" $0 $1 $2 $3 $4 $5 $6
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "InstallDate" "$2$1$0"
FunctionEnd

Function FinalizeUninstallMetadata
  ; All selected language packs, plug-ins, scripts and dictionaries are now on disk.
  ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
  WriteRegDWORD ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "EstimatedSize" $0
FunctionEnd

Section "-FinalizeUninstallMetadata"
  ${If} $DeploymentMode != "portable"
    Call FinalizeUninstallMetadata
  ${EndIf}
SectionEnd

!ifndef FBE_DEPLOYMENT_TEST_SCOPE_PROBE
Function RestoreElevatedInstallerState
  ; Read from the original NSIS process before using HKCU, shell folders or
  ; any defaults belonging to the administrator account entered at UAC.
  !insertmacro FBE_RestoreElevationVar $LANGUAGE
  !insertmacro FBE_RestoreElevationVar $DeploymentMode
  !insertmacro FBE_RestoreElevationVar $InstallScope
  !insertmacro FBE_RestoreElevationVar $SessionCurrentInstallLocation
  !insertmacro FBE_RestoreElevationVar $SessionAllUsersInstallLocation
  !insertmacro FBE_RestoreElevationVar $InstalledStateSaved
  !insertmacro FBE_RestoreElevationVar $SavedInstallScope
  !insertmacro FBE_RestoreElevationVar $SavedInstallDirectory
  !insertmacro FBE_RestoreElevationVar $SavedSystemIntegrationFlags
  !insertmacro FBE_RestoreElevationVar $SavedFb2AssociationFlags
  !insertmacro FBE_RestoreElevationVar $SavedFbdAssociationFlags
  !insertmacro FBE_RestoreElevationVar $SavedValidateFlags
  !insertmacro FBE_RestoreElevationVar $SavedExplorerFlags
  !insertmacro FBE_RestoreElevationVar $SavedShortcutsFlags
  !insertmacro FBE_RestoreElevationVar $SavedStartMenuFlags
  !insertmacro FBE_RestoreElevationVar $SavedDesktopFlags
  !insertmacro FBE_RestoreElevationVar $CommandLineCurrentUser
  !insertmacro FBE_RestoreElevationVar $CommandLineAllUsers
  !insertmacro FBE_RestoreElevationVar $CommandLinePortable
  !insertmacro FBE_RestoreElevationVar $ShellRecoveryRequested
  !insertmacro FBE_RestoreElevationVar $ICONS_GROUP
  !insertmacro FBE_RestoreElevationVar $INSTDIR

  ${If} $InstallScope == "allusers"
    SetShellVarContext all
  ${Else}
    SetShellVarContext current
    !insertmacro UAC_IsOriginalUser $0
    ${If} $0 != 1
      MessageBox MB_OK|MB_ICONSTOP $(UacDifferentUserInstaller)
      SetErrorLevel 5
      Quit
    ${EndIf}
  ${EndIf}

  ; Section indexes in this installer are contiguous, including groups and
  ; generated language packs. Copy every selection made before elevation.
  StrCpy $R8 ${MainSection_id}
elevation_sections_next:
  !insertmacro FBE_RestoreElevationSection
  IntOp $R8 $R8 + 1
  ${If} $R8 <= ${Dict08}
    Goto elevation_sections_next
  ${EndIf}
  Return
elevation_state_failed:
  MessageBox MB_OK|MB_ICONSTOP $(UacStateRestoreFailed)
  SetErrorLevel 13
  Quit
FunctionEnd
!endif

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${MainSection_id} $(DESC_Main)
  !insertmacro MUI_DESCRIPTION_TEXT ${System_Integration_id} $(DESC_System_Integration)
  !insertmacro MUI_DESCRIPTION_TEXT ${FB2_File_Association_id} $(DESC_FB2_File_Association)
  !insertmacro MUI_DESCRIPTION_TEXT ${FBD_File_Association_id} $(DESC_FBD_File_Association)
  !insertmacro MUI_DESCRIPTION_TEXT ${FB2_Validate_Command_id} $(DESC_FB2_Validate_Command)
  !insertmacro MUI_DESCRIPTION_TEXT ${FB2_Explorer_Properties_id} $(DESC_FB2_Explorer_Properties)
  !insertmacro MUI_DESCRIPTION_TEXT ${ShCutGroup_id} $(DESC_ShCutGroup)
  !insertmacro MUI_DESCRIPTION_TEXT ${Desktop_ShortCut_id} $(DESC_Desktop_ShortCut)
  !insertmacro MUI_DESCRIPTION_TEXT ${Start_Menu_ShortCuts_id} $(DESC_Start_Menu_ShortCuts)
  !insertmacro MUI_DESCRIPTION_TEXT ${PluginsGroup_id} $(DESC_PluginsGroup)
  !insertmacro MUI_DESCRIPTION_TEXT ${ImportPluginsGroup_id} $(DESC_ImportPluginsGroup)
  !insertmacro MUI_DESCRIPTION_TEXT ${ImportEPUB_Plugin_id} $(DESC_Plugin_ImportEPUB)
  !insertmacro MUI_DESCRIPTION_TEXT ${ImportEPUB_SVG_id} $(DESC_Plugin_ImportEPUB_SVG)
  !insertmacro MUI_DESCRIPTION_TEXT ${ExportPluginsGroup_id} $(DESC_ExportPluginsGroup)
  !insertmacro MUI_DESCRIPTION_TEXT ${ExportHTML_Plugin_id} $(DESC_Plugin_ExportHTML)
  !insertmacro MUI_DESCRIPTION_TEXT ${ExportDOCX_Plugin_id} $(DESC_Plugin_ExportDOCX)
  !insertmacro MUI_DESCRIPTION_TEXT ${ExportEPUB_Plugin_id} $(DESC_Plugin_ExportEPUB)
  !insertmacro MUI_DESCRIPTION_TEXT ${BatchConverters_id} $(DESC_Plugin_BatchConverters)
  !insertmacro MUI_DESCRIPTION_TEXT ${Scripts_id} $(DESC_Scripts)
  !insertmacro MUI_DESCRIPTION_TEXT ${Dictionaries_id} $(DESC_Dictionaries)
  !insertmacro MUI_DESCRIPTION_TEXT ${LanguagePacksGroup_id} $(DESC_LanguagePacksGroup)
  !insertmacro MUI_DESCRIPTION_TEXT ${LanguagePack_en_US} $(DESC_LanguagePack)
  !insertmacro MUI_DESCRIPTION_TEXT ${LanguagePack_ru_RU} $(DESC_LanguagePack)
  !insertmacro MUI_DESCRIPTION_TEXT ${LanguagePack_uk_UA} $(DESC_LanguagePack)
  !insertmacro MUI_DESCRIPTION_TEXT ${LanguagePack_de_DE} $(DESC_LanguagePack)
  !insertmacro MUI_DESCRIPTION_TEXT ${LanguagePack_fr_FR} $(DESC_LanguagePack)
  !insertmacro MUI_DESCRIPTION_TEXT ${LanguagePack_es_ES} $(DESC_LanguagePack)
  !insertmacro MUI_DESCRIPTION_TEXT ${LanguagePack_it_IT} $(DESC_LanguagePack)
  !insertmacro MUI_DESCRIPTION_TEXT ${LanguagePack_pl_PL} $(DESC_LanguagePack)
  !insertmacro MUI_DESCRIPTION_TEXT ${LanguagePack_pt_PT} $(DESC_LanguagePack)
  !insertmacro MUI_DESCRIPTION_TEXT ${LanguagePack_nl_NL} $(DESC_LanguagePack)
  !insertmacro MUI_DESCRIPTION_TEXT ${LanguagePack_cs_CZ} $(DESC_LanguagePack)
  !insertmacro MUI_DESCRIPTION_TEXT ${LanguagePack_bg_BG} $(DESC_LanguagePack)
!insertmacro MUI_FUNCTION_DESCRIPTION_END

Function DisablePortableIntegration
  ; Hiding the Components and Start Menu pages is not enough: NSIS still runs
  ; selected sections in silent mode. A portable copy must never create shell
  ; registration or shortcuts.
  !ifdef FBE_DEPLOYMENT_TEST_SCOPE_PROBE
    Return
  !else
  SectionSetFlags ${System_Integration_id} 0
  SectionSetFlags ${FB2_File_Association_id} 0
  SectionSetFlags ${FBD_File_Association_id} 0
  SectionSetFlags ${FB2_Validate_Command_id} 0
  SectionSetFlags ${FB2_Explorer_Properties_id} 0
  SectionSetFlags ${ShCutGroup_id} 0
  SectionSetFlags ${Start_Menu_ShortCuts_id} 0
  SectionSetFlags ${Desktop_ShortCut_id} 0
  !endif
FunctionEnd

Function SaveInstalledModeState
  ${If} $InstalledStateSaved == "1"
    Return
  ${EndIf}
  StrCpy $InstalledStateSaved "1"
  StrCpy $SavedInstallScope "$InstallScope"
  StrCpy $SavedInstallDirectory "$INSTDIR"
  SectionGetFlags ${System_Integration_id} $SavedSystemIntegrationFlags
  SectionGetFlags ${FB2_File_Association_id} $SavedFb2AssociationFlags
  SectionGetFlags ${FBD_File_Association_id} $SavedFbdAssociationFlags
  SectionGetFlags ${FB2_Validate_Command_id} $SavedValidateFlags
  SectionGetFlags ${FB2_Explorer_Properties_id} $SavedExplorerFlags
  SectionGetFlags ${ShCutGroup_id} $SavedShortcutsFlags
  SectionGetFlags ${Start_Menu_ShortCuts_id} $SavedStartMenuFlags
  SectionGetFlags ${Desktop_ShortCut_id} $SavedDesktopFlags
FunctionEnd

Function RestoreInstalledModeState
  ${If} $InstalledStateSaved != "1"
    Return
  ${EndIf}
  StrCpy $InstallScope "$SavedInstallScope"
  StrCpy $INSTDIR "$SavedInstallDirectory"
  ${If} $InstallScope == "allusers"
    SetShellVarContext all
  ${Else}
    SetShellVarContext current
  ${EndIf}
  SectionSetFlags ${System_Integration_id} $SavedSystemIntegrationFlags
  SectionSetFlags ${FB2_File_Association_id} $SavedFb2AssociationFlags
  SectionSetFlags ${FBD_File_Association_id} $SavedFbdAssociationFlags
  SectionSetFlags ${FB2_Validate_Command_id} $SavedValidateFlags
  SectionSetFlags ${FB2_Explorer_Properties_id} $SavedExplorerFlags
  SectionSetFlags ${ShCutGroup_id} $SavedShortcutsFlags
  SectionSetFlags ${Start_Menu_ShortCuts_id} $SavedStartMenuFlags
  SectionSetFlags ${Desktop_ShortCut_id} $SavedDesktopFlags
  StrCpy $InstalledStateSaved "0"
FunctionEnd

Function un.DeleteShortcuts
  !insertmacro MUI_STARTMENU_GETFOLDER Application $ICONS_GROUP
  Delete "$DESKTOP\${PRODUCT_SHORTCUT_NAME}.lnk"
  Delete "$SMPROGRAMS\$ICONS_GROUP\Uninstall ${PRODUCT_SHORTCUT_NAME}.lnk"
  Delete "$SMPROGRAMS\$ICONS_GROUP\${PRODUCT_SHORTCUT_NAME}.lnk"
  RMDir "$SMPROGRAMS\$ICONS_GROUP"
FunctionEnd

Function un.UserDataPageCreate
  StrCpy $UninstallUserData "0"
  nsDialogs::Create 1018
  Pop $0
  ${If} $0 == error
    Abort
  ${EndIf}
  ${NSD_CreateLabel} 0 0 100% 20u "$(UninstUserDataTitle)"
  Pop $0
  ${NSD_CreateCheckbox} 10u 28u 100% 18u "$(UninstUserDataCheckbox)"
  Pop $UninstallUserDataCheckbox
  nsDialogs::Show
FunctionEnd

Function un.UserDataPageLeave
  ${NSD_GetState} $UninstallUserDataCheckbox $0
  ${If} $0 == ${BST_CHECKED}
    StrCpy $UninstallUserData "1"
  ${Else}
    StrCpy $UninstallUserData "0"
  ${EndIf}
FunctionEnd

Section Uninstall

  Call un.CheckFBERunning
  Call un.CleanupLegacyArchHandlerRegistration

  ; Shared shell registration is removed only when this setup installed it.
  ; The helper additionally verifies every CLSID and DLL path before changing
  ; the registry, which protects a newer installation from an old uninstaller.
  StrCpy $ShellRetainUninstaller 0
  ReadRegDWORD $0 ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "PropertyHandlerInstalled"
  ReadRegDWORD $1 ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "ShellCleanupPending"
  IntOp $1 $1 | $0
  ReadRegDWORD $2 ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "ShellRecoveryPending"
  IntOp $1 $1 | $2
  ReadINIStr $2 "${FBE_SHELL_SHARED_DIR}\Owner.ini" "Owner" "InstallDirectory"
  ${If} $1 = 1
  ${OrIf} $2 == $INSTDIR
    Call un.UnregisterModernPropertyHandler
  ${EndIf}

  ; Remove only associations and verbs that still belong to this instance.
  ReadRegDWORD $1 ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "AssociationRegistered"
  ${If} $1 = 1
    ReadRegStr $0 SHCTX "Software\Classes\FictionBook.2\shell\Edit\Command" ""
    StrCmp $0 '"$INSTDIR\FBE.exe" "%1"' 0 +2
      DeleteRegKey SHCTX "Software\Classes\FictionBook.2\shell\Edit"
    ReadRegStr $0 SHCTX "Software\Classes\.fb2\DefaultIcon" ""
    StrCmp $0 "$INSTDIR\FBE.exe,0" 0 integration_cleanup_done
    DeleteRegKey SHCTX "Software\Classes\.fb2\DefaultIcon"
    ${If} $ShellRetainUninstaller != 1
    ${AndIf} $ShellPayloadResult != "skipped"
      DeleteRegValue SHCTX "Software\Classes\FictionBook.2" "InfoTip"
      DeleteRegValue SHCTX "Software\Classes\FictionBook.2" "TileInfo"
      DeleteRegValue SHCTX "Software\Classes\FictionBook.2" "Details"
      DeleteRegValue SHCTX "Software\Classes\FictionBook.2" "PreviewDetails"
    ${EndIf}
  ${EndIf}
integration_cleanup_done:

  ReadRegDWORD $1 ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "ValidateVerbInstalled"
  ${If} $1 = 1
    ReadRegStr $0 SHCTX "Software\Classes\FictionBook.2\shell\Validate\Command" ""
    StrCmp $0 '"$INSTDIR\FBV.exe" "%1"' 0 +2
      DeleteRegKey SHCTX "Software\Classes\FictionBook.2\shell\Validate"
    ReadRegStr $0 SHCTX "${FB2_SYSTEM_ASSOC_KEY}\shell\Validate\Command" ""
    StrCmp $0 '"$INSTDIR\FBV.exe" "%1"' 0 +2
      DeleteRegKey SHCTX "${FB2_SYSTEM_ASSOC_KEY}\shell\Validate"
  ${EndIf}

  ReadRegDWORD $1 ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "AssociationRegistered"
  ${If} $1 <> 1
    Goto fbd_uninstall_done
  ${EndIf}

  ; Restore the handler selected before FBE, but never remove an extension key
  ; that was changed after installation.
  ReadRegStr $0 SHCTX "Software\Classes\.fbd" ""
  StrCmp $0 "FictionBook.Description" 0 fbd_uninstall_done
  ReadRegStr $1 SHCTX "SOFTWARE\${PRODUCT_VENDOR}\${PRODUCT_NAME}\FbdAssociation" "Captured"
  StrCmp $1 "1" 0 fbd_uninstall_clear_default
  ReadRegStr $1 SHCTX "SOFTWARE\${PRODUCT_VENDOR}\${PRODUCT_NAME}\FbdAssociation" "PreviousProgId"
  StrCmp $1 "" fbd_uninstall_clear_default
  WriteRegStr SHCTX "Software\Classes\.fbd" "" "$1"
  Goto fbd_uninstall_remove_owned
fbd_uninstall_clear_default:
  DeleteRegValue SHCTX "Software\Classes\.fbd" ""
fbd_uninstall_remove_owned:
  ReadRegStr $0 SHCTX "Software\Classes\.fbd\DefaultIcon" ""
  StrCmp $0 "$INSTDIR\FBE.exe,0" 0 +2
    DeleteRegKey SHCTX "Software\Classes\.fbd\DefaultIcon"
  DeleteRegValue SHCTX "Software\Classes\.fbd" "PerceivedType"
  DeleteRegValue SHCTX "Software\Classes\.fbd" "Content Type"
  ; This ProgID is removed only after .fbd no longer points to it.
  DeleteRegKey SHCTX "Software\Classes\FictionBook.Description"
fbd_uninstall_done:
  
  ${If} $ShellRetainUninstaller != 1
    Delete "$INSTDIR\uninst.exe"
  ${EndIf}
  Delete "$INSTDIR\Scintilla.dll"
  Delete "$INSTDIR\Lexilla.dll"
  Delete "$INSTDIR\SciLexer.dll"
  Delete "$INSTDIR\main.js"
  Delete "$INSTDIR\main.html"
  Delete "$INSTDIR\main.css"
  Delete "$INSTDIR\main_fast.css"
  Delete "$INSTDIR\imgprev.html"
  Delete "$INSTDIR\genres.txt"
  Delete "$INSTDIR\genres.rus.txt"
  Delete "$INSTDIR\genres.ukr.txt"
  Delete "$INSTDIR\genres.txt_L"
  Delete "$INSTDIR\genres.rus.txt_L"
  Delete "$INSTDIR\FictionBookLinks.xsd"
  Delete "$INSTDIR\FictionBookLang.xsd"
  Delete "$INSTDIR\FictionBookGenres.xsd"
  Delete "$INSTDIR\FictionBook.xsd"
  Delete "$INSTDIR\fb2.xsl"
  Delete "$INSTDIR\eng.xsl"
  Delete "$INSTDIR\rus.xsl"
  Delete "$INSTDIR\ukr.xsl"
  Delete "$INSTDIR\html.xsl"
  Delete "$INSTDIR\ImportEPUB.dll"
  Delete "$INSTDIR\ExportHTML.dll"
  Delete "$INSTDIR\ExportDOCX.dll"
  Delete "$INSTDIR\ExportEPUB.dll"
  Delete "$INSTDIR\ExportDOCXBatch.exe"
  Delete "$INSTDIR\ExportEPUBBatch.exe"
  Delete "$INSTDIR\ImportEPUBBatch.exe"
  Delete "$INSTDIR\ImportEPUBLunaSVG.dll"
  Delete "$INSTDIR\Plugins\ImportEPUB.dll"
  Delete "$INSTDIR\Plugins\ExportHTML.dll"
  Delete "$INSTDIR\Plugins\ExportDOCX.dll"
  Delete "$INSTDIR\Plugins\ExportEPUB.dll"
  Delete "$INSTDIR\Plugins\ImportEPUBLunaSVG.dll"
  Delete "$INSTDIR\Plugins\plugins.json"
  RMDir "$INSTDIR\Plugins"
  Delete "$INSTDIR\gdiplus.manifest"
  Delete "$INSTDIR\gdiplus.dll"
  Delete "$INSTDIR\gdiplus.cat"
  Delete "$INSTDIR\*.reg"
  
  Delete "$INSTDIR\gpl-3.0.ru.txt"
	Delete "$INSTDIR\gpl-3.0.ua.txt"

	Delete "$INSTDIR\contributors.txt"
	Delete "$INSTDIR\LICENSE"
  Delete "$INSTDIR\NOTICE"
  Delete "$INSTDIR\THIRD-PARTY-NOTICES.md"
  RMDir /r "$INSTDIR\THIRD-PARTY-LICENSES"
	Delete "$INSTDIR\Resources\Words.xml"
	RMDir "$INSTDIR\Resources"
	; Clean up the obsolete seed location left by an older installation.
	Delete "$INSTDIR\defaults\Words.xml"
	RMDir "$INSTDIR\defaults"
  RMDir /r "$INSTDIR\Themes"
  Delete "$INSTDIR\InstallerTools\register-sequence-property-schema.ps1"
  Delete "$INSTDIR\InstallerTools\register-modern-property-handler.ps1"
  Delete "$INSTDIR\InstallerTools\unregister-modern-property-handler.ps1"
  RMDir "$INSTDIR\InstallerTools"

  Delete "$INSTDIR\symbols.ini"

  ;Scripts
  RMDir /r "$INSTDIR\Dict"
  RMDir /r "$INSTDIR\Backgrounds"
  RMDir /r "$INSTDIR\Scripts"
  RMDir /r "$INSTDIR\TreeCmd"
  RMDir /r "$INSTDIR\HTML"
  RMDir /r "$INSTDIR\Help"
  RMDir /r "$INSTDIR\Utilities"
  RMDir /r "$INSTDIR\img"
  RMDir /r "$INSTDIR\Lang"

  ; Удаляем MUI-раскладку из ранних выпусков FictionBook Editor Next.
  Delete "$INSTDIR\FBVVerbResources.dll"
  Delete "$INSTDIR\en-US\FBVVerbResources.dll.mui"
  Delete "$INSTDIR\ru-RU\FBVVerbResources.dll.mui"
  Delete "$INSTDIR\uk-UA\FBVVerbResources.dll.mui"
  Delete "$INSTDIR\de-DE\FBVVerbResources.dll.mui"
  Delete "$INSTDIR\fr-FR\FBVVerbResources.dll.mui"
  Delete "$INSTDIR\es-ES\FBVVerbResources.dll.mui"
  Delete "$INSTDIR\it-IT\FBVVerbResources.dll.mui"
  Delete "$INSTDIR\pl-PL\FBVVerbResources.dll.mui"
  Delete "$INSTDIR\cs-CZ\FBVVerbResources.dll.mui"
  Delete "$INSTDIR\bg-BG\FBVVerbResources.dll.mui"
  Delete "$INSTDIR\pt-PT\FBVVerbResources.dll.mui"
  Delete "$INSTDIR\nl-NL\FBVVerbResources.dll.mui"
  RMDir "$INSTDIR\en-US"
  RMDir "$INSTDIR\ru-RU"
  RMDir "$INSTDIR\uk-UA"
  RMDir "$INSTDIR\de-DE"
  RMDir "$INSTDIR\fr-FR"
  RMDir "$INSTDIR\es-ES"
  RMDir "$INSTDIR\it-IT"
  RMDir "$INSTDIR\pl-PL"
  RMDir "$INSTDIR\cs-CZ"
  RMDir "$INSTDIR\bg-BG"
  RMDir "$INSTDIR\pt-PT"
  RMDir "$INSTDIR\nl-NL"

  Delete "$INSTDIR\blank.fb2"
  Delete "$INSTDIR\FBV.exe"
  Delete "$INSTDIR\res_rus.dll"
  Delete "$INSTDIR\res_ukr.dll"
  Delete "$INSTDIR\FBE.exe"

; Delete program settings only after the explicit, unchecked opt-in page.
  ${If} $UninstallUserData == "1"
    ${If} ${UAC_IsInnerInstance}
      !insertmacro UAC_AsUser_Call Function un.DeleteOriginalUserData ${UAC_SYNCREGISTERS}
    ${Else}
      Call un.DeleteOriginalUserData
    ${EndIf}
  ${EndIf}

  Call un.DeleteShortcuts

  RMDir "$INSTDIR"

  ${If} $ShellRetainUninstaller != 1
    DeleteRegKey ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}"
  ${Else}
    ${If} $ShellPayloadResult == "pending"
      SetErrorLevel 3010
    ${Else}
      SetErrorLevel 1
    ${EndIf}
  ${EndIf}
  DeleteRegKey ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_DIR_REGKEY}"
  DeleteRegKey ${PRODUCT_UNINST_ROOT_KEY} "SOFTWARE\${PRODUCT_VENDOR}\${PRODUCT_NAME}"
  SetAutoClose false
SectionEnd
!endif

Function un.DeleteOriginalUserData
  Call un.GetUserAppData
  Pop $0
  Delete "$0\FBE Next\Words.xml"
  RMDir /r "$0\FBE Next"
  DeleteRegKey HKEY_CURRENT_USER "SOFTWARE\FBETeam\FictionBook Editor Next"
FunctionEnd
