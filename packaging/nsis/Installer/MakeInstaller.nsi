!define /date BUILDNUM "%d %b"
!include "version.nsh"
!ifndef INPUTDIR
!define INPUTDIR "..\..\..\out\package\FictionBookEditor"
!endif
!define PRODUCT_NAME "FictionBook Editor Next"
!define PRODUCT_STAGE "Release"
!define PRODUCT_BUILD "build ${BUILDNUM}"
!define PRODUCT_VERSION "${PRODUCT_STAGE} ${PRODUCT_VER_NUM} (${PRODUCT_BUILD})"
!define PRODUCT_VENDOR "FBE Team"
!define PRODUCT_NAME_VERSION "${PRODUCT_NAME} ${PRODUCT_VERSION}"
!ifndef OUTPUTFILE
!define OUTPUTFILE "${PRODUCT_NAME_VERSION}.exe"
!endif
!define PRODUCT_URL "https://github.com/sklart/fictionbook-editor-next"
!define PRODUCT_DIR_REGKEY "Software\Microsoft\Windows\CurrentVersion\App Paths\FBE.exe"
!define PRODUCT_UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}"
!define PRODUCT_UNINST_ROOT_KEY "HKCU"
!define PRODUCT_STARTMENU_REGVAL "NSIS:StartMenuDir"
!define PRODUCT_SYSTEM_INTEGRATION_REGVAL "SystemIntegrationInstalled"
!define FB2_PROPERTY_HANDLER_CLSID "{D4A47F38-1E5A-4F0D-B1C9-6D2A4A6B1F42}"
!define FBE_SEQUENCE_SCHEMA_FILE "FBE.Sequence.propdesc"
!define FBE_SHELL_SHARED_DIR "$%ProgramData%\FictionBook Editor\Shell"
!define FB2_INFOTIP_PROPERTIES "prop:System.ItemTypeText;System.Author;System.Title;System.Language;FBE.Sequence;FBE.DocumentVersion;FBE.DocumentDate;System.Size"
!define FB2_TILEINFO_PROPERTIES "prop:System.Author;System.Title"
!define FB2_DETAILS_PROPERTIES "prop:System.ItemTypeText;System.Author;System.Title;System.Language;FBE.Genre;FBE.Sequence;FBE.DocumentVersion;FBE.DocumentDate;FBE.Keywords;FBE.DocumentId;System.Size"
!define FB2_PREVIEWDETAILS_PROPERTIES "prop:System.ItemTypeText;System.Author;System.Title;System.Language;FBE.Genre;FBE.Sequence;FBE.DocumentVersion;FBE.DocumentDate;FBE.Keywords;FBE.DocumentId;System.Size"
ManifestDPIAware true
SetCompressor /SOLID lzma

;--------------------------------
;Interface Configuration

  !define MUI_WELCOMEFINISHPAGE_BITMAP "..\\res\\fbe.bmp"
  !define MUI_UNWELCOMEFINISHPAGE_BITMAP "..\\res\\fbe.bmp"

RequestExecutionLevel user

; Includes
!include "MUI2.nsh"
!include "UAC.nsh"
!include "LogicLib.nsh"
!include "Sections.nsh"
!include "x64.nsh"
!include "FileFunc.nsh"

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

; Components page
!define MUI_PAGE_CUSTOMFUNCTION_LEAVE ComponentsPageLeave
!insertmacro MUI_PAGE_COMPONENTS

; Directory page
!insertmacro MUI_PAGE_DIRECTORY

; Start menu page
var ICONS_GROUP
!define MUI_STARTMENUPAGE_NODISABLE
!define MUI_STARTMENUPAGE_DEFAULTFOLDER "${PRODUCT_NAME} ${PRODUCT_VER_NUM}"
!define MUI_STARTMENUPAGE_REGISTRY_ROOT "${PRODUCT_UNINST_ROOT_KEY}"
!define MUI_STARTMENUPAGE_REGISTRY_KEY "${PRODUCT_UNINST_KEY}"
!define MUI_STARTMENUPAGE_REGISTRY_VALUENAME "${PRODUCT_STARTMENU_REGVAL}"
!insertmacro MUI_PAGE_STARTMENU Application $ICONS_GROUP

; Instfiles page
!insertmacro MUI_PAGE_INSTFILES

; Finish page
!define MUI_FINISHPAGE_RUN
!define MUI_FINISHPAGE_RUN_FUNCTION ExecAppFile
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

; Localized strings
!include "Localization\English.nsh"
!include "Localization\Russian.nsh"
!include "Localization\Ukrainian.nsh"

Name "${PRODUCT_NAME_VERSION}"
OutFile "${OUTPUTFILE}"
InstallDir "$LOCALAPPDATA\Programs\${PRODUCT_NAME}"
InstallDirRegKey ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_DIR_REGKEY}" ""
ShowInstDetails show
ShowUnInstDetails show

Function .onInit
  !insertmacro UAC_PageElevation_OnInit
  ${If} ${UAC_IsInnerInstance}
  ${AndIfNot} ${UAC_IsAdmin}
    SetErrorLevel 0x666666
    Quit
  ${EndIf}
  SetShellVarContext current
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
  SetShellVarContext current
  ${IfNot} ${UAC_IsInnerInstance}
    !insertmacro MUI_LANGDLL_DISPLAY
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
  ; Даем длинной подписи чекбокса больше места, чтобы она не обрезалась
  ; на современных системах с более крупными системными шрифтами.
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
  File "${INPUTDIR}\FBVVerbResources.dll"
  File "${INPUTDIR}\res_rus.dll"
  File "${INPUTDIR}\res_ukr.dll"
  SetOutPath "$INSTDIR\en-US"
  File /nonfatal "${INPUTDIR}\en-US\FBVVerbResources.dll.mui"
  SetOutPath "$INSTDIR\ru-RU"
  File /nonfatal "${INPUTDIR}\ru-RU\FBVVerbResources.dll.mui"
  SetOutPath "$INSTDIR\uk-UA"
  File /nonfatal "${INPUTDIR}\uk-UA\FBVVerbResources.dll.mui"
  SetOutPath "$INSTDIR\de-DE"
  File /nonfatal "${INPUTDIR}\de-DE\FBVVerbResources.dll.mui"
  SetOutPath "$INSTDIR\fr-FR"
  File /nonfatal "${INPUTDIR}\fr-FR\FBVVerbResources.dll.mui"
  SetOutPath "$INSTDIR\es-ES"
  File /nonfatal "${INPUTDIR}\es-ES\FBVVerbResources.dll.mui"
  SetOutPath "$INSTDIR\it-IT"
  File /nonfatal "${INPUTDIR}\it-IT\FBVVerbResources.dll.mui"
  SetOutPath "$INSTDIR\pl-PL"
  File /nonfatal "${INPUTDIR}\pl-PL\FBVVerbResources.dll.mui"
  SetOutPath "$INSTDIR\cs-CZ"
  File /nonfatal "${INPUTDIR}\cs-CZ\FBVVerbResources.dll.mui"
  SetOutPath "$INSTDIR\bg-BG"
  File /nonfatal "${INPUTDIR}\bg-BG\FBVVerbResources.dll.mui"
  SetOutPath "$INSTDIR\pt-PT"
  File /nonfatal "${INPUTDIR}\pt-PT\FBVVerbResources.dll.mui"
  SetOutPath "$INSTDIR\nl-NL"
  File /nonfatal "${INPUTDIR}\nl-NL\FBVVerbResources.dll.mui"
  SetOutPath "$INSTDIR"
  File /nonfatal "${INPUTDIR}\*.reg"
  File "${INPUTDIR}\gpl-3.0.txt"
  File "${INPUTDIR}\gpl-3.0.ru.txt"
  File "${INPUTDIR}\gpl-3.0.ua.txt"
  File "${INPUTDIR}\contributors.txt"
  File "${INPUTDIR}\copying.txt"

  ; uninstall info must exist for any successful installation, not only when
  ; optional system integration is selected
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}" "DisplayName" "${PRODUCT_NAME_VERSION}"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}" "DisplayVersion" "${PRODUCT_VERSION}"
  WriteRegExpandStr ${PRODUCT_UNINST_ROOT_KEY} "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}" "InstallLocation" "$INSTDIR"
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

  SetAutoClose false
SectionEnd

Section $(System_Integration) System_Integration_id
  SectionIn 1

  ; register typelib
  Push "$INSTDIR\FBE.exe"
  Call RegisterTlb

  ; create an FB2 progid
  WriteRegStr HKCU "Software\Classes\FictionBook.2" "" "FictionBook"
  WriteRegStr HKCU "Software\Classes\FictionBook.2\CurVer" "" "FictionBook.2"

  ; create an FB2 filetype
  WriteRegStr HKCU "Software\Classes\.fb2" "" "FictionBook.2"
  WriteRegStr HKCU "Software\Classes\.fb2" "PerceivedType" "Text"
  WriteRegStr HKCU "Software\Classes\.fb2" "Content Type" "text/xml"
  WriteRegStr HKCU "Software\Classes\.fb2\DefaultIcon" "" "$INSTDIR\FBE.exe,0"
  WriteRegStr HKCU "Software\Classes\FictionBook.2\DefaultIcon" "" "$INSTDIR\FBE.exe,0"
  WriteRegStr HKCU "Software\Classes\FictionBook.2" "InfoTip" "${FB2_INFOTIP_PROPERTIES}"
  WriteRegStr HKCU "Software\Classes\FictionBook.2" "TileInfo" "${FB2_TILEINFO_PROPERTIES}"
  WriteRegStr HKCU "Software\Classes\FictionBook.2" "Details" "${FB2_DETAILS_PROPERTIES}"
  WriteRegStr HKCU "Software\Classes\FictionBook.2" "PreviewDetails" "${FB2_PREVIEWDETAILS_PROPERTIES}"

  ; register verb
  ;WriteRegStr HKCR "FictionBook.2\shell\Open\Command" "" '"$INSTDIR\FBE.exe" "%1"'
  WriteRegStr HKCU "Software\Classes\FictionBook.2\shell\Edit\Command" "" '"$INSTDIR\FBE.exe" "%1"'
  WriteRegStr HKCU "Software\Classes\FictionBook.2\shell\Validate" "" "Validate"
  WriteRegStr HKCU "Software\Classes\FictionBook.2\shell\Validate" "MUIVerb" '@$INSTDIR\FBVVerbResources.dll,-109;v2'
  WriteRegStr HKCU "Software\Classes\FictionBook.2\shell\Validate" "Icon" '"$INSTDIR\FBV.exe",0'
  WriteRegStr HKCU "Software\Classes\FictionBook.2\shell\Validate\Command" "" '"$INSTDIR\FBV.exe" "%1"'

  ; write the installation path into the registry
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "SOFTWARE\${PRODUCT_VENDOR}\${PRODUCT_NAME}" "InstallDir" "$INSTDIR"

  ; modern property handler for Win32/x64 Explorer
  SetOutPath "${FBE_SHELL_SHARED_DIR}"
  File /nonfatal "${INPUTDIR}\FBShell.dll"
  File /nonfatal "${INPUTDIR}\FBShell64.dll"
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

Function ComponentsPageLeave
  SectionGetFlags ${System_Integration_id} $0
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

Function CreateStartMenuShortcuts
  !insertmacro MUI_STARTMENU_WRITE_BEGIN Application
  CreateDirectory "$SMPROGRAMS\$ICONS_GROUP"
  CreateShortCut "$SMPROGRAMS\$ICONS_GROUP\FictionBook Editor Next.lnk" "$INSTDIR\FBE.exe"
  CreateShortCut "$SMPROGRAMS\$ICONS_GROUP\Uninstall FictionBook Editor Next.lnk" "$INSTDIR\uninst.exe"
  !insertmacro MUI_STARTMENU_WRITE_END
FunctionEnd

Function CreateDesktopShortcut
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
			RegDll "$INSTDIR\ImportEPUB.dll"
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
			RegDll "$INSTDIR\ExportHTML.dll"
		SectionEnd
		Section $(Plugin_ExportDOCX) ExportDOCX_Plugin_id
			SetOutPath "$INSTDIR"
			File "${INPUTDIR}\ExportDOCX.dll"
			RegDll "$INSTDIR\ExportDOCX.dll"
		SectionEnd
		Section $(Plugin_ExportEPUB) ExportEPUB_Plugin_id
			SetOutPath "$INSTDIR"
			File "${INPUTDIR}\ExportEPUB.dll"
			RegDll "$INSTDIR\ExportEPUB.dll"
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
		File "${INPUTDIR}\ExportDOCX.dll"
		File "${INPUTDIR}\ExportEPUB.dll"
		File "${INPUTDIR}\ImportEPUB.dll"
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

  ; remove registry keys
  DeleteRegKey ${PRODUCT_UNINST_ROOT_KEY} "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}"
  DeleteRegKey ${PRODUCT_UNINST_ROOT_KEY} "SOFTWARE\${PRODUCT_VENDOR}\${PRODUCT_NAME}"
  ; remove typelib entry
  DeleteRegKey HKCR "Interface\{7269066E-2089-4408-B3F3-E8D75984D5A6}"
  DeleteRegKey HKCR "TypeLib\{37B16C7D-4400-4D7D-AA35-14C74E265EA4}"


  Call un.UnregisterModernPropertyHandler
  Call un.UnregisterFbePropertySchema
  Delete "${FBE_SHELL_SHARED_DIR}\FBShell.dll"
  Delete "${FBE_SHELL_SHARED_DIR}\FBShell64.dll"
  Delete "${FBE_SHELL_SHARED_DIR}\${FBE_SEQUENCE_SCHEMA_FILE}"
  RMDir "${FBE_SHELL_SHARED_DIR}"

  ; remove plugin
  UnRegDll "$INSTDIR\ImportEPUB.dll"
  UnRegDll "$INSTDIR\ExportHTML.dll"
  UnRegDll "$INSTDIR\ExportDOCX.dll"
  UnRegDll "$INSTDIR\ExportEPUB.dll"

  ; remove verbs; TODO: check if these really point to FBE
  DeleteRegKey HKCU "Software\Classes\FictionBook.2\shell\Edit"
  DeleteRegKey HKCU "Software\Classes\FictionBook.2\shell\Validate"
  DeleteRegKey HKCU "Software\Classes\.fb2\DefaultIcon"
  DeleteRegValue HKCU "Software\Classes\FictionBook.2" "InfoTip"
  DeleteRegValue HKCU "Software\Classes\FictionBook.2" "TileInfo"
  DeleteRegValue HKCU "Software\Classes\FictionBook.2" "Details"
  DeleteRegValue HKCU "Software\Classes\FictionBook.2" "PreviewDetails"
  
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
  
  Delete "$INSTDIR\gpl-3.0.txt"
  Delete "$INSTDIR\gpl-3.0.ru.txt"
  Delete "$INSTDIR\gpl-3.0.ua.txt"

  Delete "$INSTDIR\contributors.txt"
  Delete "$INSTDIR\copying.txt"
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

  Delete "$INSTDIR\blank.fb2"
  Delete "$INSTDIR\FBV.exe"
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
  Delete "$INSTDIR\res_rus.dll"
  Delete "$INSTDIR\res_ukr.dll"
  Delete "$INSTDIR\FBE.exe"

; Delete program settings
  MessageBox MB_YESNO $(UninstAskSettings) IDNO DoNotDelete

    Call un.GetUserAppData
    Pop $0
    Delete "$0\FBE\Hotkeys.xml"
    Delete "$0\FBE\Settings.xml"
    Delete "$0\FBE\Words.xml"
    RMDir /r "$0\FBE"

    DeleteRegKey HKEY_CURRENT_USER "SOFTWARE\FBETeam"

DoNotDelete:

  Call un.DeleteShortcuts

  RMDir "$INSTDIR"

  DeleteRegKey ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}"
  DeleteRegKey ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_DIR_REGKEY}"
  SetAutoClose false
SectionEnd
