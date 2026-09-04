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

foreach ($state in @('InstallScope', 'InstallLocation', 'CoreVersion', 'AssociationRegistered', 'ValidateVerbInstalled', 'PropertyHandlerInstalled')) {
    Require $nsis ('"' + $state + '"') "NSIS installer state"
}

Require $nsis 'ReadRegDWORD $0 ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "PropertyHandlerInstalled"' 'Property-handler uninstall state gate'
Require $nsis 'ReadRegDWORD $1 ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "AssociationRegistered"' 'Association uninstall state gate'
Require $nsis 'ReadRegDWORD $1 ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "ValidateVerbInstalled"' 'Validate-verb uninstall state gate'
if ($nsis -match 'LegacyComInstalled|UnregisterLegacyPluginIfOwned|\bRegDll\b') { throw 'Dead bundled Legacy COM installer state remains.' }
$upgradeStart = $nsis.IndexOf('Call CheckFBERunning', [StringComparison]::Ordinal)
$upgradeEnd = $nsis.IndexOf('SetOutPath "$INSTDIR"', $upgradeStart, [StringComparison]::Ordinal)
if ($upgradeStart -lt 0 -or $upgradeEnd -le $upgradeStart) { throw 'Не найден upgrade cleanup до копирования Core.' }
$upgrade = $nsis.Substring($upgradeStart, $upgradeEnd - $upgradeStart)
foreach ($dll in 'ImportEPUB.dll','ExportHTML.dll','ExportDOCX.dll','ExportEPUB.dll') { Require $upgrade ('Delete "$INSTDIR\' + $dll + '"') "Legacy plugin root cleanup: $dll" }
Require $upgrade 'Delete "$INSTDIR\ImportEPUBLunaSVG.dll"' 'Legacy SVG helper root cleanup'
Require $nsis 'Delete "$INSTDIR\Plugins\plugins.json"' 'Plugin manifest uninstall cleanup'
Require $nsis 'RMDir "$INSTDIR\Plugins"' 'Empty Plugins directory uninstall cleanup'
Require $nsis 'Function MigrateLegacySharedShell' 'Legacy ProgramData shell migration'
Require $nsis 'FBE_LEGACY_SHELL_SHARED_DIR' 'Legacy ProgramData shell path'
Require $nsis 'FBE-migrate-legacy-shell-status.ini' 'Legacy migration ownership status'
Require $unregister 'function Test-ModernRegistrationOwned' 'Modern handler ownership check'
Require $unregister 'FOREIGN_REGISTRATION' 'Foreign handler safe-skip result'
Require $unregister 'Test-ComClassRegistrationOwned' 'Modern handler COM path check'

Write-Host 'NSIS ownership-safe uninstall contract passed.'
