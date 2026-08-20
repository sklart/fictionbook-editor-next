!define /date BUILDNUM "%d %b"
!include "version.nsh"
!ifndef INPUTDIR
!define INPUTDIR "..\..\..\out\package\FictionBookEditor"
!endif
!define PRODUCT_NAME "FictionBook Editor Next"
!define PRODUCT_STAGE "Release"
!define PRODUCT_BUILD "build ${BUILDNUM}"
!ifdef FBE_WIN7_BUILD
!define PRODUCT_COMPATIBILITY_SUFFIX " (Windows 7 compatible)"
!else
!define PRODUCT_COMPATIBILITY_SUFFIX ""
!endif
!define PRODUCT_VERSION "${PRODUCT_STAGE} ${PRODUCT_VER_NUM} (${PRODUCT_BUILD})${PRODUCT_COMPATIBILITY_SUFFIX}"
!define PRODUCT_VENDOR "FBE Team"
!define PRODUCT_NAME_VERSION "${PRODUCT_NAME} ${PRODUCT_VERSION}"
!ifndef OUTPUTFILE
!define OUTPUTFILE "${PRODUCT_NAME_VERSION}.exe"
!endif
!define PRODUCT_URL "https://github.com/sklart/fictionbook-editor-next"
; Отдельное имя ключа не позволяет FBE Next перезаписывать App Paths старого FBE.
!define PRODUCT_DIR_REGKEY "Software\Microsoft\Windows\CurrentVersion\App Paths\FictionBookEditorNext.exe"
!define PRODUCT_UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}"
; SHCTX follows SetShellVarContext: current user by default, all users after
; explicit elevation/scope selection.  Do not hard-code HKCU in shared paths.
!define PRODUCT_UNINST_ROOT_KEY "SHCTX"
!define PRODUCT_STARTMENU_REGVAL "NSIS:StartMenuDir"
!define PRODUCT_SYSTEM_INTEGRATION_REGVAL "SystemIntegrationInstalled"
!define FB2_PROPERTY_HANDLER_CLSID "{D4A47F38-1E5A-4F0D-B1C9-6D2A4A6B1F42}"
!define FBE_SEQUENCE_SCHEMA_FILE "FBE.Sequence.propdesc"
!define FBE_SHELL_SHARED_DIR "$%ProgramData%\FictionBook Editor Next\Shell"
!define FB2_INFOTIP_PROPERTIES "prop:System.ItemTypeText;System.Author;System.Title;System.Language;FBE.Sequence;FBE.DocumentVersion;FBE.DocumentDate;System.Size"
!define FB2_TILEINFO_PROPERTIES "prop:System.Author;System.Title"
!define FB2_DETAILS_PROPERTIES "prop:System.ItemTypeText;System.Author;System.Title;System.Language;FBE.Genre;FBE.Sequence;FBE.DocumentVersion;FBE.DocumentDate;FBE.Keywords;FBE.DocumentId;System.Size"
!define FB2_PREVIEWDETAILS_PROPERTIES "prop:System.ItemTypeText;System.Author;System.Title;System.Language;FBE.Genre;FBE.Sequence;FBE.DocumentVersion;FBE.DocumentDate;FBE.Keywords;FBE.DocumentId;System.Size"
!define FB2_SYSTEM_ASSOC_KEY "Software\Classes\SystemFileAssociations\.fb2"
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

Function .onInit
  !insertmacro UAC_PageElevation_OnInit
  ${If} ${UAC_IsInnerInstance}
  ${AndIfNot} ${UAC_IsAdmin}
    SetErrorLevel 0x666666
    Quit
  ${EndIf}
  SetShellVarContext current
  StrCpy $DeploymentMode "installed"
  StrCpy $InstallScope "current"
  ${IfNot} ${UAC_IsInnerInstance}
    !insertmacro MUI_LANGDLL_DISPLAY
  ${EndIf}
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
    StrCpy $DeploymentMode "portable"
    StrCpy $InstallScope "current"
    SetShellVarContext current
    StrCpy $INSTDIR "$EXEDIR\${PRODUCT_NAME} Portable"
  ${Else}
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
  ${NSD_CreateRadioButton} 10u 48u 100% 12u "$(InstallScopeAllUsers)"
  Pop $InstallScopeAllUsersRadio
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
    StrCpy $InstallScope "allusers"
    SetShellVarContext all
    StrCpy $INSTDIR "$PROGRAMFILES32\${PRODUCT_NAME}"
  ${Else}
    StrCpy $InstallScope "current"
    SetShellVarContext current
    StrCpy $INSTDIR "$LOCALAPPDATA\Programs\${PRODUCT_NAME}"
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
  !insertmacro UAC_PageElevation_OnInit
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
  ${If} $0 <> 1
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

Function HasAdminRights
  ${If} ${UAC_IsAdmin}
    StrCpy $0 "Admin"
  ${Else}
    StrCpy $0 "User"
  ${EndIf}
FunctionEnd

Function RegisterFbePropertySchema
  Call HasAdminRights
  StrCmp $0 "Admin" +2
  Goto rfps_skip_no_admin

  IfFileExists "${FBE_SHELL_SHARED_DIR}\${FBE_SEQUENCE_SCHEMA_FILE}" 0 rfps_done
  IfFileExists "$INSTDIR\InstallerTools\register-sequence-property-schema.ps1" 0 rfps_no_helper

  Delete "$TEMP\FBE-register-schema-status.ini"

  ${If} ${RunningX64}
    ExecWait '"$WINDIR\sysnative\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -File "$INSTDIR\InstallerTools\register-sequence-property-schema.ps1" -SchemaPath "${FBE_SHELL_SHARED_DIR}\${FBE_SEQUENCE_SCHEMA_FILE}" -StatusFilePath "$TEMP\FBE-register-schema-status.ini" -NoRestartExplorer' $0
  ${Else}
    ExecWait '"$WINDIR\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -File "$INSTDIR\InstallerTools\register-sequence-property-schema.ps1" -SchemaPath "${FBE_SHELL_SHARED_DIR}\${FBE_SEQUENCE_SCHEMA_FILE}" -StatusFilePath "$TEMP\FBE-register-schema-status.ini" -NoRestartExplorer' $0
  ${EndIf}

  ${If} $0 != 0
    ReadINIStr $1 "$TEMP\FBE-register-schema-status.ini" "Schema" "Step"
    ReadINIStr $2 "$TEMP\FBE-register-schema-status.ini" "Schema" "Code"
    ${If} $1 == ""
      StrCpy $1 "register-sequence-property-schema.ps1"
    ${EndIf}
    ${If} $2 == ""
      StrCpy $2 "exit=$0"
    ${EndIf}
    DetailPrint "Schema registration helper failed at step $1 with $2."
    MessageBox MB_OK|MB_ICONEXCLAMATION "$(WarnModernPropertyHandlerInstall)$\r$\n$\r$\nШаг: $1$\r$\nКод: $2"
    Return
  ${EndIf}

  DetailPrint "Schema registration helper completed successfully."

rfps_done:
  Return
rfps_no_helper:
  DetailPrint "Skip property schema registration: helper script is missing."
  MessageBox MB_OK|MB_ICONEXCLAMATION "$(WarnModernPropertyHandlerInstall)$\r$\n$\r$\nШаг: register-sequence-property-schema.ps1$\r$\nДетали: helper-скрипт отсутствует в составе установки."
  Return
rfps_skip_no_admin:
  DetailPrint "Skip property schema registration: administrative rights are required."
  Return
FunctionEnd

Function un.UnregisterFbePropertySchema
  IfFileExists "${FBE_SHELL_SHARED_DIR}\${FBE_SEQUENCE_SCHEMA_FILE}" 0 done

  ClearErrors
  System::Call 'propsys::PSUnregisterPropertySchema(w "${FBE_SHELL_SHARED_DIR}\${FBE_SEQUENCE_SCHEMA_FILE}") i .r0'
  ${If} ${Errors}
    DetailPrint "Не удалось вызвать PSUnregisterPropertySchema для ${FBE_SEQUENCE_SCHEMA_FILE}."
    Return
  ${EndIf}

  IntCmp $0 0 refresh unregister_failed unregister_status
unregister_failed:
  DetailPrint "PSUnregisterPropertySchema вернул ошибку $0 для ${FBE_SEQUENCE_SCHEMA_FILE}."
  Return
unregister_status:
  DetailPrint "PSUnregisterPropertySchema вернул дополнительный статус $0 для ${FBE_SEQUENCE_SCHEMA_FILE}."

refresh:
  ClearErrors
  System::Call 'propsys::PSRefreshPropertySchema() i .r0'
  ${If} ${Errors}
    DetailPrint "Не удалось вызвать PSRefreshPropertySchema после снятия schema."
    Return
  ${EndIf}

  IntCmp $0 0 done refresh_failed refresh_status
refresh_failed:
  DetailPrint "PSRefreshPropertySchema вернул ошибку $0 после снятия schema."
  Return
refresh_status:
  DetailPrint "PSRefreshPropertySchema вернул дополнительный статус $0 после снятия schema."

done:
FunctionEnd

Function RegisterModernPropertyHandler
  Call HasAdminRights
  StrCmp $0 "Admin" +2
  Goto rmph_skip_no_admin

  ${If} ${RunningX64}
    IfFileExists "${FBE_SHELL_SHARED_DIR}\FBShell64.dll" 0 rmph_done
    IfFileExists "$INSTDIR\InstallerTools\register-modern-property-handler.ps1" 0 rmph_no_helper

    Delete "$TEMP\FBE-register-shell-status.ini"
    ExecWait '"$WINDIR\sysnative\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -File "$INSTDIR\InstallerTools\register-modern-property-handler.ps1" -DllPath "${FBE_SHELL_SHARED_DIR}\FBShell64.dll" -Platform x64 -PropertyHandlerClsid "${FB2_PROPERTY_HANDLER_CLSID}" -StatusFilePath "$TEMP\FBE-register-shell-status.ini"' $0
    ${If} $0 != 0
      ReadINIStr $1 "$TEMP\FBE-register-shell-status.ini" "Shell" "Step"
      ReadINIStr $2 "$TEMP\FBE-register-shell-status.ini" "Shell" "Code"
      ReadINIStr $3 "$TEMP\FBE-register-shell-status.ini" "Shell" "Message"
      ${If} $1 == ""
        StrCpy $1 "register-modern-property-handler.ps1"
      ${EndIf}
      ${If} $2 == ""
        StrCpy $2 "exit=$0"
      ${EndIf}
      DetailPrint "Modern property handler helper failed at step $1 with $2."
      ${If} $3 != ""
        DetailPrint "Details: $3"
        MessageBox MB_OK|MB_ICONEXCLAMATION "$(WarnModernPropertyHandlerInstall)$\r$\n$\r$\nШаг: $1$\r$\nКод: $2$\r$\nДетали: $3"
      ${Else}
        MessageBox MB_OK|MB_ICONEXCLAMATION "$(WarnModernPropertyHandlerInstall)$\r$\n$\r$\nШаг: $1$\r$\nКод: $2"
      ${EndIf}
      Return
    ${EndIf}
    DetailPrint "Registered modern property handler for 64-bit Explorer."
    Goto rmph_done
  ${EndIf}

  IfFileExists "${FBE_SHELL_SHARED_DIR}\FBShell.dll" 0 rmph_done
  IfFileExists "$INSTDIR\InstallerTools\register-modern-property-handler.ps1" 0 rmph_no_helper

  Delete "$TEMP\FBE-register-shell-status.ini"
  ExecWait '"$WINDIR\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -File "$INSTDIR\InstallerTools\register-modern-property-handler.ps1" -DllPath "${FBE_SHELL_SHARED_DIR}\FBShell.dll" -Platform Win32 -PropertyHandlerClsid "${FB2_PROPERTY_HANDLER_CLSID}" -StatusFilePath "$TEMP\FBE-register-shell-status.ini"' $0
  ${If} $0 != 0
    ReadINIStr $1 "$TEMP\FBE-register-shell-status.ini" "Shell" "Step"
    ReadINIStr $2 "$TEMP\FBE-register-shell-status.ini" "Shell" "Code"
    ReadINIStr $3 "$TEMP\FBE-register-shell-status.ini" "Shell" "Message"
    ${If} $1 == ""
      StrCpy $1 "register-modern-property-handler.ps1"
    ${EndIf}
    ${If} $2 == ""
      StrCpy $2 "exit=$0"
    ${EndIf}
    DetailPrint "Modern property handler helper failed at step $1 with $2."
    ${If} $3 != ""
      DetailPrint "Details: $3"
      MessageBox MB_OK|MB_ICONEXCLAMATION "$(WarnModernPropertyHandlerInstall)$\r$\n$\r$\nШаг: $1$\r$\nКод: $2$\r$\nДетали: $3"
    ${Else}
      MessageBox MB_OK|MB_ICONEXCLAMATION "$(WarnModernPropertyHandlerInstall)$\r$\n$\r$\nШаг: $1$\r$\nКод: $2"
    ${EndIf}
    Return
  ${EndIf}
  DetailPrint "Registered modern property handler for Win32 Explorer."
rmph_done:
  Return
rmph_no_helper:
  DetailPrint "Skip modern property handler registration: helper script is missing."
  MessageBox MB_OK|MB_ICONEXCLAMATION "$(WarnModernPropertyHandlerInstall)$\r$\n$\r$\nШаг: register-modern-property-handler.ps1$\r$\nДетали: helper-скрипт отсутствует в составе установки."
  Return
rmph_skip_no_admin:
  DetailPrint "Skip modern shell registration: administrative rights are required."
  Return
FunctionEnd

Function un.UnregisterModernPropertyHandler
  ${If} ${RunningX64}
    IfFileExists "${FBE_SHELL_SHARED_DIR}\FBShell64.dll" 0 remove_key_x64
    IfFileExists "$INSTDIR\InstallerTools\unregister-modern-property-handler.ps1" 0 remove_key_x64

    Delete "$TEMP\FBE-unregister-shell-status.ini"
    ExecWait '"$WINDIR\sysnative\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -File "$INSTDIR\InstallerTools\unregister-modern-property-handler.ps1" -DllPath "${FBE_SHELL_SHARED_DIR}\FBShell64.dll" -Platform x64 -StatusFilePath "$TEMP\FBE-unregister-shell-status.ini"' $0
    ${If} $0 != 0
      DetailPrint "Не удалось снять регистрацию FBShell64.dll helper-скриптом, код $0."
    ${EndIf}

remove_key_x64:
    Return
  ${EndIf}

  IfFileExists "${FBE_SHELL_SHARED_DIR}\FBShell.dll" 0 remove_key_win32
  IfFileExists "$INSTDIR\InstallerTools\unregister-modern-property-handler.ps1" 0 remove_key_win32

  Delete "$TEMP\FBE-unregister-shell-status.ini"
  ExecWait '"$WINDIR\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -File "$INSTDIR\InstallerTools\unregister-modern-property-handler.ps1" -DllPath "${FBE_SHELL_SHARED_DIR}\FBShell.dll" -Platform Win32 -StatusFilePath "$TEMP\FBE-unregister-shell-status.ini"' $0
  ${If} $0 != 0
    DetailPrint "Не удалось снять регистрацию FBShell.dll helper-скриптом, код $0."
  ${EndIf}

remove_key_win32:
FunctionEnd

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
  SetOutPath "$INSTDIR"

  StrCmp $DeploymentMode "portable" portable_core_done installed_core_state
portable_core_done:
  FileOpen $0 "$INSTDIR\portable.ini" w
  FileWrite $0 "[Portable]$\r$\nDataPath=Data$\r$\n"
  FileClose $0
  CreateDirectory "$INSTDIR\Data\Settings"
  CreateDirectory "$INSTDIR\Data\Logs"
  CreateDirectory "$INSTDIR\Data\Diagnostics"
  CreateDirectory "$INSTDIR\Data\Recovery"
  CreateDirectory "$INSTDIR\Data\Cache"
  CreateDirectory "$INSTDIR\Data\Temp"
  Goto main_section_done

installed_core_state:
  ; uninstall info must exist for any successful installation, not only when
  ; optional system integration is selected
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}" "DisplayName" "${PRODUCT_NAME_VERSION}"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}" "DisplayVersion" "${PRODUCT_VERSION}"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}" "InstallScope" "$InstallScope"
  WriteRegExpandStr ${PRODUCT_UNINST_ROOT_KEY} "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}" "InstallLocation" "$INSTDIR"
	WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "SOFTWARE\${PRODUCT_VENDOR}\${PRODUCT_NAME}" "InstallLocation" "$INSTDIR"
	WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "SOFTWARE\${PRODUCT_VENDOR}\${PRODUCT_NAME}" "CoreVersion" "${PRODUCT_VERSION}"
  WriteRegExpandStr ${PRODUCT_UNINST_ROOT_KEY} "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}" "DisplayIcon" "$INSTDIR\FBE.exe,0"
  WriteRegExpandStr ${PRODUCT_UNINST_ROOT_KEY} "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}" "Publisher" "${PRODUCT_VENDOR}"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}" "URLInfoAbout" "${PRODUCT_URL}"
  WriteRegDWORD ${PRODUCT_UNINST_ROOT_KEY} "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}" "NoModify" 1
  WriteRegDWORD ${PRODUCT_UNINST_ROOT_KEY} "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}" "NoRepair" 1
  WriteRegDWORD ${PRODUCT_UNINST_ROOT_KEY} "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}" "${PRODUCT_SYSTEM_INTEGRATION_REGVAL}" 0
  WriteRegExpandStr ${PRODUCT_UNINST_ROOT_KEY} "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}" "UninstallString" "$INSTDIR\uninst.exe"
  WriteUninstaller "$INSTDIR\uninst.exe"
  ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
  WriteRegDWORD ${PRODUCT_UNINST_ROOT_KEY} "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}" "EstimatedSize" $0
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
SectionEnd

Section /o $(FB2_Explorer_Properties) FB2_Explorer_Properties_id
  ; Создаём FB2 ProgID только для shell-строк метаданных; .fb2 здесь не ассоциируем.
  WriteRegStr SHCTX "Software\Classes\FictionBook.2" "" "FictionBook"
  WriteRegStr SHCTX "Software\Classes\FictionBook.2\CurVer" "" "FictionBook.2"
  WriteRegStr SHCTX "Software\Classes\FictionBook.2" "InfoTip" "${FB2_INFOTIP_PROPERTIES}"
  WriteRegStr SHCTX "Software\Classes\FictionBook.2" "TileInfo" "${FB2_TILEINFO_PROPERTIES}"
  WriteRegStr SHCTX "Software\Classes\FictionBook.2" "Details" "${FB2_DETAILS_PROPERTIES}"
  WriteRegStr SHCTX "Software\Classes\FictionBook.2" "PreviewDetails" "${FB2_PREVIEWDETAILS_PROPERTIES}"

  ; Современный обработчик свойств для Win32/x64 Проводника.
  SetOutPath "${FBE_SHELL_SHARED_DIR}"
  File /nonfatal /oname=FBShell.dll.new "${INPUTDIR}\FBShell.dll"
  IfFileExists "${FBE_SHELL_SHARED_DIR}\FBShell.dll.new" 0 fbshell32_done
  IfFileExists "${FBE_SHELL_SHARED_DIR}\FBShell.dll" 0 fbshell32_install
  ClearErrors
  Delete "${FBE_SHELL_SHARED_DIR}\FBShell.dll"
  IfErrors 0 fbshell32_install
    DetailPrint "FBShell.dll занята Проводником Windows; обновление будет завершено после перезагрузки."
    Delete /REBOOTOK "${FBE_SHELL_SHARED_DIR}\FBShell.dll"
    Rename /REBOOTOK "${FBE_SHELL_SHARED_DIR}\FBShell.dll.new" "${FBE_SHELL_SHARED_DIR}\FBShell.dll"
    SetRebootFlag true
    Goto fbshell32_done
fbshell32_install:
  ClearErrors
  Rename "${FBE_SHELL_SHARED_DIR}\FBShell.dll.new" "${FBE_SHELL_SHARED_DIR}\FBShell.dll"
  IfErrors 0 fbshell32_done
    DetailPrint "FBShell.dll не удалось заменить сразу; обновление будет завершено после перезагрузки."
    Rename /REBOOTOK "${FBE_SHELL_SHARED_DIR}\FBShell.dll.new" "${FBE_SHELL_SHARED_DIR}\FBShell.dll"
    SetRebootFlag true
fbshell32_done:
  File /nonfatal /oname=FBShell64.dll.new "${INPUTDIR}\FBShell64.dll"
  IfFileExists "${FBE_SHELL_SHARED_DIR}\FBShell64.dll.new" 0 fbshell64_done
  IfFileExists "${FBE_SHELL_SHARED_DIR}\FBShell64.dll" 0 fbshell64_install
  ClearErrors
  Delete "${FBE_SHELL_SHARED_DIR}\FBShell64.dll"
  IfErrors 0 fbshell64_install
    DetailPrint "FBShell64.dll занята Проводником Windows; обновление будет завершено после перезагрузки."
    Delete /REBOOTOK "${FBE_SHELL_SHARED_DIR}\FBShell64.dll"
    Rename /REBOOTOK "${FBE_SHELL_SHARED_DIR}\FBShell64.dll.new" "${FBE_SHELL_SHARED_DIR}\FBShell64.dll"
    SetRebootFlag true
    Goto fbshell64_done
fbshell64_install:
  ClearErrors
  Rename "${FBE_SHELL_SHARED_DIR}\FBShell64.dll.new" "${FBE_SHELL_SHARED_DIR}\FBShell64.dll"
  IfErrors 0 fbshell64_done
    DetailPrint "FBShell64.dll не удалось заменить сразу; обновление будет завершено после перезагрузки."
    Rename /REBOOTOK "${FBE_SHELL_SHARED_DIR}\FBShell64.dll.new" "${FBE_SHELL_SHARED_DIR}\FBShell64.dll"
    SetRebootFlag true
fbshell64_done:
  File /nonfatal "${INPUTDIR}\${FBE_SEQUENCE_SCHEMA_FILE}"
  SetOutPath "$INSTDIR\InstallerTools"
  File /nonfatal "${INPUTDIR}\InstallerTools\register-sequence-property-schema.ps1"
  File /nonfatal "${INPUTDIR}\InstallerTools\register-modern-property-handler.ps1"
  File /nonfatal "${INPUTDIR}\InstallerTools\unregister-modern-property-handler.ps1"
  SetOutPath "$INSTDIR"
  Call RegisterFbePropertySchema
  Call RegisterModernPropertyHandler

  WriteRegDWORD ${PRODUCT_UNINST_ROOT_KEY} "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}" "${PRODUCT_SYSTEM_INTEGRATION_REGVAL}" 1
SectionEnd

SectionGroupEnd


Function ComponentsPageLeave
  ${If} $InstallScope == "allusers"
  ${AndIfNot} ${UAC_IsAdmin}
    !insertmacro UAC_PageElevation_RunElevated
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
  ${EndIf}

  SectionGetFlags ${FB2_Explorer_Properties_id} $0
  IntOp $0 $0 & ${SF_SELECTED}
  ${If} $0 = 0
    Return
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
  CreateShortCut "$SMPROGRAMS\$ICONS_GROUP\FictionBook Editor Next.lnk" "$INSTDIR\FBE.exe"
  CreateShortCut "$SMPROGRAMS\$ICONS_GROUP\Uninstall FictionBook Editor Next.lnk" "$INSTDIR\uninst.exe"
  !insertmacro MUI_STARTMENU_WRITE_END
FunctionEnd

Function CreateDesktopShortcut
  ${If} $DeploymentMode == "portable"
    Return
  ${EndIf}
  !insertmacro MUI_STARTMENU_WRITE_BEGIN Application
  CreateShortCut "$DESKTOP\FictionBook Editor Next.lnk" "$INSTDIR\FBE.exe"
  !insertmacro MUI_STARTMENU_WRITE_END
FunctionEnd

SectionGroup /e !$(ShCutGroup) ShCutGroup_id
; Shortcuts
	Section $(Start_Menu_ShortCuts) Start_Menu_ShortCuts_id
		!insertmacro UAC_AsUser_Call Function CreateStartMenuShortcuts ${UAC_SYNCREGISTERS}|${UAC_SYNCOUTDIR}|${UAC_SYNCINSTDIR}
	SectionEnd	
	Section /o $(Desktop_ShortCut) Desktop_ShortCut_id
		!insertmacro UAC_AsUser_Call Function CreateDesktopShortcut ${UAC_SYNCREGISTERS}|${UAC_SYNCOUTDIR}|${UAC_SYNCINSTDIR}
	SectionEnd
SectionGroupEnd

SectionGroup /e !$(PluginsGroup) PluginsGroup_id
; Plugins
	SectionGroup /e !$(ImportPluginsGroup) ImportPluginsGroup_id
		Section $(Plugin_ImportEPUB) ImportEPUB_Plugin_id
			SetOutPath "$INSTDIR"
			File "${INPUTDIR}\ImportEPUB.dll"
		SectionEnd
		Section /o $(Plugin_ImportEPUB_SVG) ImportEPUB_SVG_id
			SetOutPath "$INSTDIR"
			File "${INPUTDIR}\ImportEPUBLunaSVG.dll"
		SectionEnd
	SectionGroupEnd

	SectionGroup /e !$(ExportPluginsGroup) ExportPluginsGroup_id
		Section $(Plugin_ExportHTML) ExportHTML_Plugin_id
			SetOutPath "$INSTDIR"
			File "${INPUTDIR}\ExportHTML.dll"
		SectionEnd
		Section $(Plugin_ExportDOCX) ExportDOCX_Plugin_id
			SetOutPath "$INSTDIR"
			File "${INPUTDIR}\ExportDOCX.dll"
		SectionEnd
		Section $(Plugin_ExportEPUB) ExportEPUB_Plugin_id
			SetOutPath "$INSTDIR"
			File "${INPUTDIR}\ExportEPUB.dll"
		SectionEnd
	SectionGroupEnd

	; The editor activates bundled plug-ins via their local class factories.
	; Registry COM remains available only for external legacy consumers.
	Section /o "Legacy COM compatibility" LegacyComCompatibility_id
		RegDll "$INSTDIR\ImportEPUB.dll"
		RegDll "$INSTDIR\ExportHTML.dll"
		RegDll "$INSTDIR\ExportDOCX.dll"
		RegDll "$INSTDIR\ExportEPUB.dll"
		WriteRegDWORD ${PRODUCT_UNINST_ROOT_KEY} "SOFTWARE\${PRODUCT_VENDOR}\${PRODUCT_NAME}" "LegacyComInstalled" 1
	SectionEnd

	Section /o $(Plugin_BatchConverters) BatchConverters_id
		SetOutPath "$INSTDIR"
		File "${INPUTDIR}\ExportDOCXBatch.exe"
		File "${INPUTDIR}\ExportEPUBBatch.exe"
		File "${INPUTDIR}\ImportEPUBBatch.exe"

		; Консольные batch-конвертеры используют те же библиотеки, что и GUI-плагины.
		; Копируем зависимости здесь тоже, чтобы секция была самодостаточной, даже если
		; пользователь не выбрал соответствующие GUI-плагины.
		File "${INPUTDIR}\ExportDOCX.dll"
		File "${INPUTDIR}\ExportEPUB.dll"
		File "${INPUTDIR}\ImportEPUB.dll"
	SectionEnd
SectionGroupEnd

Function VerifyPluginRegistration
  SectionGetFlags ${ExportHTML_Plugin_id} $1
  IntOp $1 $1 & ${SF_SELECTED}
  ${If} $1 = 0
    Goto verify_export_docx
  ${EndIf}
  ClearErrors
  ReadRegStr $0 HKCU "Software\FBETeam\FictionBook Editor Next\Plugins\{C3098839-EF69-4DE5-B27D-1E80051CA843}" "Type"
  ${If} $0 != "Export"
    DetailPrint "ExportHTML plugin registration was not found after RegDll."
    MessageBox MB_OK|MB_ICONEXCLAMATION "Плагин ExportHTML не зарегистрировался. Установка продолжена, но экспорт в HTML будет недоступен. Проверьте журнал установки и повторите установку."
  ${EndIf}

verify_export_docx:
  SectionGetFlags ${ExportDOCX_Plugin_id} $1
  IntOp $1 $1 & ${SF_SELECTED}
  ${If} $1 = 0
    Goto verify_export_epub
  ${EndIf}
  ClearErrors
  ReadRegStr $0 HKCU "Software\FBETeam\FictionBook Editor Next\Plugins\{09B5ABFF-177E-4C03-98D0-9EF4E1C9DB56}" "Type"
  ${If} $0 != "Export"
    DetailPrint "ExportDOCX plugin registration was not found after RegDll."
    MessageBox MB_OK|MB_ICONEXCLAMATION "Плагин ExportDOCX не зарегистрировался. Установка продолжена, но экспорт в DOCX будет недоступен. Проверьте журнал установки и повторите установку."
  ${EndIf}

verify_export_epub:
  SectionGetFlags ${ExportEPUB_Plugin_id} $1
  IntOp $1 $1 & ${SF_SELECTED}
  ${If} $1 = 0
    Return
  ${EndIf}
  ClearErrors
  ReadRegStr $0 HKCU "Software\FBETeam\FictionBook Editor Next\Plugins\{36FCFB2D-C3D8-4B81-ABC1-5A09CA846515}" "Type"
  ${If} $0 != "Export"
    DetailPrint "ExportEPUB plugin registration was not found after RegDll."
    MessageBox MB_OK|MB_ICONEXCLAMATION "Плагин ExportEPUB не зарегистрировался. Установка продолжена, но экспорт в EPUB будет недоступен. Проверьте журнал установки и повторите установку."
  ${EndIf}
FunctionEnd

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
SectionEnd

SubSection !$(Dictionaries) Dictionaries_id

        Section $(EnglishDict) Dict01
      SectionIn RO
 	  SetOutPath "$INSTDIR\Dict"
	  File "${INPUTDIR}\dict\en_US.dic"
	  File "${INPUTDIR}\dict\en_US.aff"
        SectionEnd

        Section $(RussianDict) Dict02
      SectionIn RO
 	  SetOutPath "$INSTDIR\Dict"
	  File "${INPUTDIR}\dict\ru_RU.dic"
	  File "${INPUTDIR}\dict\ru_RU.aff"
        SectionEnd

        Section /o $(UkrainianDict) Dict03
	  SetOutPath "$INSTDIR\Dict"
	  File "${INPUTDIR}\dict\uk_UA.dic"
	  File "${INPUTDIR}\dict\uk_UA.aff"
        SectionEnd

        Section /o $(GermanDict) Dict08
	  SetOutPath "$INSTDIR\Dict"
	  File "${INPUTDIR}\dict\de_DE.dic"
	  File "${INPUTDIR}\dict\de_DE.aff"
        SectionEnd

SubSectionEnd

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

Function un.DeleteShortcuts
  !insertmacro MUI_STARTMENU_GETFOLDER Application $ICONS_GROUP
  Delete "$DESKTOP\FictionBook Editor Next.lnk"
  Delete "$SMPROGRAMS\$ICONS_GROUP\Uninstall FictionBook Editor Next.lnk"
  Delete "$SMPROGRAMS\$ICONS_GROUP\FictionBook Editor Next.lnk"
  RMDir "$SMPROGRAMS\$ICONS_GROUP"
FunctionEnd

Section Uninstall

  Call un.CheckFBERunning

  ; remove typelib entry
  DeleteRegKey HKCR "Interface\{7269066E-2089-4408-B3F3-E8D75984D5A6}"
  DeleteRegKey HKCR "TypeLib\{37B16C7D-4400-4D7D-AA35-14C74E265EA4}"


  Call un.UnregisterModernPropertyHandler
  Call un.UnregisterFbePropertySchema
  Delete "${FBE_SHELL_SHARED_DIR}\FBShell.dll"
  Delete "${FBE_SHELL_SHARED_DIR}\FBShell64.dll"
  Delete "${FBE_SHELL_SHARED_DIR}\${FBE_SEQUENCE_SCHEMA_FILE}"
  RMDir "${FBE_SHELL_SHARED_DIR}"

  ; Only the explicitly selected legacy component performs COM registration.
  ReadRegDWORD $0 ${PRODUCT_UNINST_ROOT_KEY} "SOFTWARE\${PRODUCT_VENDOR}\${PRODUCT_NAME}" "LegacyComInstalled"
  ${If} $0 = 1
    UnRegDll "$INSTDIR\ImportEPUB.dll"
    UnRegDll "$INSTDIR\ExportHTML.dll"
    UnRegDll "$INSTDIR\ExportDOCX.dll"
    UnRegDll "$INSTDIR\ExportEPUB.dll"
  ${EndIf}

  ; Remove only associations and verbs that still belong to this instance.
  ReadRegStr $0 SHCTX "Software\Classes\FictionBook.2\shell\Edit\Command" ""
  StrCmp $0 '"$INSTDIR\FBE.exe" "%1"' 0 +2
    DeleteRegKey SHCTX "Software\Classes\FictionBook.2\shell\Edit"
  ReadRegStr $0 SHCTX "Software\Classes\FictionBook.2\shell\Validate\Command" ""
  StrCmp $0 '"$INSTDIR\FBV.exe" "%1"' 0 +2
    DeleteRegKey SHCTX "Software\Classes\FictionBook.2\shell\Validate"
  ReadRegStr $0 SHCTX "${FB2_SYSTEM_ASSOC_KEY}\shell\Validate\Command" ""
  StrCmp $0 '"$INSTDIR\FBV.exe" "%1"' 0 +2
    DeleteRegKey SHCTX "${FB2_SYSTEM_ASSOC_KEY}\shell\Validate"
  ReadRegStr $0 SHCTX "Software\Classes\.fb2\DefaultIcon" ""
  StrCmp $0 "$INSTDIR\FBE.exe,0" 0 integration_cleanup_done
  DeleteRegKey SHCTX "Software\Classes\.fb2\DefaultIcon"
  DeleteRegValue SHCTX "Software\Classes\FictionBook.2" "InfoTip"
  DeleteRegValue SHCTX "Software\Classes\FictionBook.2" "TileInfo"
  DeleteRegValue SHCTX "Software\Classes\FictionBook.2" "Details"
  DeleteRegValue SHCTX "Software\Classes\FictionBook.2" "PreviewDetails"
integration_cleanup_done:

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
  
  Delete "$INSTDIR\uninst.exe"
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
	RMDir /r "$INSTDIR\Themes\licenses"
  Delete "$INSTDIR\InstallerTools\register-sequence-property-schema.ps1"
  Delete "$INSTDIR\InstallerTools\register-modern-property-handler.ps1"
  Delete "$INSTDIR\InstallerTools\unregister-modern-property-handler.ps1"
  RMDir "$INSTDIR\InstallerTools"

  Delete "$INSTDIR\symbols.ini"

  ;Scripts
  RMDir /r "$INSTDIR\Dict"
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

; Delete program settings
  MessageBox MB_YESNO $(UninstAskSettings) IDNO DoNotDelete

    Call un.GetUserAppData
    Pop $0
    Delete "$0\FBE Next\Words.xml"
	RMDir /r "$0\FBE Next"

	DeleteRegKey HKEY_CURRENT_USER "SOFTWARE\FBETeam\FictionBook Editor Next"

DoNotDelete:

  Call un.DeleteShortcuts

  RMDir "$INSTDIR"

  DeleteRegKey ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}"
  DeleteRegKey ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_DIR_REGKEY}"
  SetAutoClose false
SectionEnd
