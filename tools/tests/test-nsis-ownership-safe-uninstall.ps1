<# Verifies that the installer records component ownership before uninstalling. #>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$nsis = Get-Content -Raw -LiteralPath (Join-Path $root 'packaging\nsis\Installer\MakeInstaller.nsi')
$unregister = Get-Content -Raw -LiteralPath (Join-Path $root 'tools\build\unregister-modern-property-handler.ps1')

function Require([string]$Text, [string]$Needle, [string]$Description) {
    if ($Text.IndexOf($Needle, [StringComparison]::Ordinal) -lt 0) {
        throw "${Description}: missing '$Needle'."
    }
}

foreach ($state in @('InstallScope', 'InstallLocation', 'CoreVersion', 'AssociationRegistered', 'ValidateVerbInstalled', 'PropertyHandlerInstalled', 'LegacyComInstalled')) {
    Require $nsis ('"' + $state + '"') "NSIS installer state"
}

Require $nsis 'ReadRegDWORD $0 ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "PropertyHandlerInstalled"' 'Property-handler uninstall state gate'
Require $nsis 'ReadRegDWORD $1 ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "AssociationRegistered"' 'Association uninstall state gate'
Require $nsis 'ReadRegDWORD $1 ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "ValidateVerbInstalled"' 'Validate-verb uninstall state gate'
Require $nsis '!macro UnregisterLegacyPluginIfOwned CLSID DLL' 'Legacy COM ownership helper'
Require $nsis 'ReadRegStr $0 HKCU "Software\Classes\CLSID\${CLSID}\InprocServer32" ""' 'Legacy COM path check'
Require $nsis 'Function MigrateLegacySharedShell' 'Legacy ProgramData shell migration'
Require $nsis 'FBE_LEGACY_SHELL_SHARED_DIR' 'Legacy ProgramData shell path'
Require $nsis 'FBE-migrate-legacy-shell-status.ini' 'Legacy migration ownership status'
Require $unregister 'function Test-ModernRegistrationOwned' 'Modern handler ownership check'
Require $unregister 'FOREIGN_REGISTRATION' 'Foreign handler safe-skip result'
Require $unregister 'Test-ComClassRegistrationOwned' 'Modern handler COM path check'

Write-Host 'NSIS ownership-safe uninstall contract passed.'
