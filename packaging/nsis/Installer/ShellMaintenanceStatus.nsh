; Only a complete protocol response from a normally exited worker can change confirmed state.
!macro FBE_ReadShellMaintenanceStatus
  StrCpy $ShellStatusValid 1
  ReadINIStr $ShellPayloadResult "$ShellToolsDirectory\Status.ini" "Shell" "Result"
  ReadINIStr $ShellOperationCode "$ShellToolsDirectory\Status.ini" "Shell" "Code"
  ReadINIStr $ShellEffectiveInstalled "$ShellToolsDirectory\Status.ini" "Shell" "EffectiveInstalled"
  ReadINIStr $ShellCleanupPending "$ShellToolsDirectory\Status.ini" "Shell" "CleanupPending"
  ReadINIStr $ShellRecoveryPending "$ShellToolsDirectory\Status.ini" "Shell" "RecoveryPending"
  ReadINIStr $ShellSystemInstalled "$ShellToolsDirectory\Status.ini" "Shell" "SystemIntegrationInstalled"
  ReadINIStr $1 "$ShellToolsDirectory\Status.ini" "Shell" "Message"
  ReadINIStr $2 "$ShellToolsDirectory\Status.ini" "Shell" "Protocol"
  ReadINIStr $3 "$ShellToolsDirectory\Status.ini" "Shell" "Phase"
  ReadINIStr $4 "$ShellToolsDirectory\Status.ini" "Shell" "ProcessExitCode"
  ${If} $9 != 0
  ${OrIf} $2 != "1"
  ${OrIf} $3 != "complete"
  ${OrIf} $4 != "0"
  ${OrIf} $ShellOperationCode == ""
  ${OrIf} $ShellOperationCode == "STARTING"
    StrCpy $ShellStatusValid 0
  ${EndIf}
  ${If} $ShellPayloadResult != "ok"
  ${AndIf} $ShellPayloadResult != "pending"
  ${AndIf} $ShellPayloadResult != "skipped"
  ${AndIf} $ShellPayloadResult != "error"
    StrCpy $ShellStatusValid 0
  ${EndIf}
  !insertmacro FBE_RequireShellBoolean $ShellEffectiveInstalled
  !insertmacro FBE_RequireShellBoolean $ShellCleanupPending
  !insertmacro FBE_RequireShellBoolean $ShellRecoveryPending
  !insertmacro FBE_RequireShellBoolean $ShellSystemInstalled
  IntOp $2 $ShellEffectiveInstalled | $ShellCleanupPending
  IntOp $2 $2 | $ShellRecoveryPending
  ${If} $2 != $ShellSystemInstalled
    StrCpy $ShellStatusValid 0
  ${EndIf}
  ${If} $ShellPayloadResult == "ok"
  ${OrIf} $ShellPayloadResult == "skipped"
    ${If} $ShellCleanupPending != "0"
    ${OrIf} $ShellRecoveryPending != "0"
      StrCpy $ShellStatusValid 0
    ${EndIf}
  ${EndIf}
  ${If} $ShellCleanupPending == "1"
  ${AndIf} $ShellPayloadResult != "error"
    ${If} $ShellPayloadResult != "pending"
    ${OrIf} $ShellEffectiveInstalled != "0"
    ${OrIf} $ShellOperationCode != "CLEANUP_PENDING"
      StrCpy $ShellStatusValid 0
    ${EndIf}
  ${EndIf}
  ${If} $ShellPayloadResult == "pending"
    ${If} $ShellOperationCode != "CLEANUP_PENDING"
    ${AndIf} $ShellOperationCode != "FILES_IN_USE"
    ${AndIf} $ShellOperationCode != "BACKUP_CLEANUP_PENDING"
      StrCpy $ShellStatusValid 0
    ${EndIf}
  ${EndIf}
  ${If} $ShellOperationCode == "RECOVERY_REQUIRED"
  ${AndIf} $ShellRecoveryPending != "1"
    StrCpy $ShellStatusValid 0
  ${EndIf}
  ${If} $ShellPayloadResult == "ok"
    ${If} $ShellOperationCode != "INSTALLED"
    ${AndIf} $ShellOperationCode != "ALREADY_CURRENT"
    ${AndIf} $ShellOperationCode != "INSPECTED"
    ${AndIf} $ShellOperationCode != "RECOVERED"
    ${AndIf} $ShellOperationCode != "NO_RECOVERY_NEEDED"
    ${AndIf} $ShellOperationCode != "NOT_INSTALLED"
    ${AndIf} $ShellOperationCode != "REMOVED"
      StrCpy $ShellStatusValid 0
    ${EndIf}
  ${EndIf}
  ${If} $ShellPayloadResult == "skipped"
    ${If} $ShellOperationCode != "FOREIGN_PACKAGE"
    ${AndIf} $ShellOperationCode != "FOREIGN_REGISTRATION"
    ${AndIf} $ShellOperationCode != "REGISTRATION_CHANGED"
      StrCpy $ShellStatusValid 0
    ${EndIf}
  ${EndIf}
  ${If} $ShellPayloadResult == "error"
    ${If} $ShellOperationCode != "INITIALIZATION_FAILED"
    ${AndIf} $ShellOperationCode != "RECOVERY_REQUIRED"
    ${AndIf} $ShellOperationCode != "UPDATE_FAILED"
      StrCpy $ShellStatusValid 0
    ${EndIf}
  ${EndIf}
  ${If} $ShellOperationCode == "CLEANUP_PENDING"
  ${AndIf} $ShellCleanupPending != "1"
    StrCpy $ShellStatusValid 0
  ${EndIf}
  ${If} $ShellPayloadResult == "pending"
  ${AndIf} $ShellRecoveryPending != "0"
    StrCpy $ShellStatusValid 0
  ${EndIf}
  ${If} $ShellOperationCode == "INSTALLED"
  ${OrIf} $ShellOperationCode == "ALREADY_CURRENT"
    ${If} $ShellEffectiveInstalled != "1"
      StrCpy $ShellStatusValid 0
    ${EndIf}
  ${EndIf}
  ${If} $ShellOperationCode == "REMOVED"
  ${OrIf} $ShellOperationCode == "NOT_INSTALLED"
  ${OrIf} $ShellPayloadResult == "skipped"
    ${If} $ShellEffectiveInstalled != "0"
      StrCpy $ShellStatusValid 0
    ${EndIf}
  ${EndIf}
!macroend

!macro FBE_RequireShellBoolean value
  ${If} ${value} != "0"
  ${AndIf} ${value} != "1"
    StrCpy $ShellStatusValid 0
  ${EndIf}
!macroend

!macro FBE_ApplyShellMaintenanceStatus
  ${If} $ShellStatusValid == 1
    WriteRegDWORD ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "PropertyHandlerInstalled" $ShellEffectiveInstalled
    WriteRegDWORD ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "ShellCleanupPending" $ShellCleanupPending
    WriteRegDWORD ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "ShellRecoveryPending" $ShellRecoveryPending
    WriteRegDWORD ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "${PRODUCT_SYSTEM_INTEGRATION_REGVAL}" $ShellSystemInstalled
  ${Else}
    ; Keep the three confirmed fields exactly as stored; add recovery only.
    WriteRegDWORD ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "ShellRecoveryPending" 1
    StrCpy $ShellPayloadResult "error"
    StrCpy $ShellOperationCode "INDETERMINATE_RESULT"
    StrCpy $ShellRecoveryPending 1
  ${EndIf}
!macroend
