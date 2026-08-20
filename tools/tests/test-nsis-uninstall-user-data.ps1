<# Verifies that uninstall keeps user data by default and offers an explicit opt-in. #>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$nsis = Get-Content -Raw -LiteralPath (Join-Path $root 'packaging\nsis\Installer\MakeInstaller.nsi')

function Require([string]$Needle, [string]$Description) {
    if ($nsis.IndexOf($Needle, [StringComparison]::Ordinal) -lt 0) {
        throw "NSIS uninstall user-data contract is missing: $Description ($Needle)."
    }
}

foreach ($entry in @(
    @('UninstPage custom un.UserDataPageCreate un.UserDataPageLeave', 'explicit user-data page'),
    @('Var UninstallUserData', 'user-data state'),
    @('StrCpy $UninstallUserData "0"', 'unchecked default'),
    @('${NSD_CreateCheckbox}', 'opt-in checkbox'),
    @('${If} $UninstallUserData == "1"', 'conditional deletion'),
    @('DeleteRegKey HKEY_CURRENT_USER "SOFTWARE\FBETeam\FictionBook Editor Next"', 'user-data deletion only inside opt-in')
)) { Require $entry[0] $entry[1] }

if ($nsis.IndexOf('MessageBox MB_YESNO $(UninstAskSettings)', [StringComparison]::Ordinal) -ge 0) {
    throw 'The legacy interactive MessageBox must not control user-data deletion.'
}

Write-Host 'NSIS explicit user-data uninstall contract passed.'
