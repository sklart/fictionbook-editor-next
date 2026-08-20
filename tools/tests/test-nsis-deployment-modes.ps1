<# Verifies that the NSIS script keeps the three deployment branches explicit. #>
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$text = Get-Content -LiteralPath (Join-Path $root 'packaging\nsis\Installer\MakeInstaller.nsi') -Raw
function Require([string]$fragment) { if ($text.IndexOf($fragment, [StringComparison]::Ordinal) -lt 0) { throw "NSIS deployment mode contract is missing: $fragment" } }

foreach ($fragment in @(
    '!include "nsDialogs.nsh"',
    'Page custom DeploymentModePageCreate DeploymentModePageLeave',
    'Page custom InstallScopePageCreate InstallScopePageLeave',
    'StrCpy $INSTDIR "$LOCALAPPDATA\Programs\${PRODUCT_NAME}"',
    'StrCpy $INSTDIR "$PROGRAMFILES32\${PRODUCT_NAME}"',
    'SetShellVarContext all',
    'WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}" "InstallScope" "$InstallScope"',
    'ReadRegStr $ExistingMachineInstall HKLM "${PRODUCT_UNINST_KEY}" "InstallLocation"',
    'Function CheckOtherScopeConflict',
    'ReadRegStr $0 HKCU "${PRODUCT_UNINST_KEY}" "InstallLocation"',
    'ReadRegStr $0 HKLM "${PRODUCT_UNINST_KEY}" "InstallLocation"',
    '$(InstallScopeConflict)',
    'ReadRegStr $0 SHCTX "Software\Classes\FictionBook.2\shell\Edit\Command" ""',
    'DeleteRegKey SHCTX "Software\Classes\FictionBook.2\shell\Edit"',
    'StrCmp $DeploymentMode "portable" portable_core_done installed_core_state',
    'FileOpen $0 "$INSTDIR\portable.ini" w',
    'CreateDirectory "$INSTDIR\Data\Settings"',
    'Goto main_section_done',
    'Function ComponentsPagePre',
    'Function StartMenuPagePre'
)) { Require $fragment }
Write-Host 'NSIS deployment modes contract passed.'
