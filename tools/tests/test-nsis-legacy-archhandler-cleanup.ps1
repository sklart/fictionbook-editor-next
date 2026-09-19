<# Verifies safe removal of registrations left by pre-FBE Next ArchHandler. #>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$script = Get-Content -Raw -LiteralPath (Join-Path $root 'packaging\nsis\Installer\MakeInstaller.nsi')

function Get-FunctionBody([string]$name) {
    $match = [regex]::Match($script, '(?ms)^Function\s+' + [regex]::Escape($name) + '\s*\r?\n(?<body>.*?)^FunctionEnd\s*$')
    if (-not $match.Success) { throw "Не найдена NSIS-функция $name." }
    return $match.Groups['body'].Value
}
function Require([string]$text, [string]$needle, [string]$message) {
    if (-not $text.Contains($needle)) { throw $message }
}
function Forbid([string]$text, [string]$needle, [string]$message) {
    if ($text.Contains($needle)) { throw $message }
}

$cleanup = Get-FunctionBody 'CleanupLegacyArchHandlerRegistration'
$uninstallCleanup = Get-FunctionBody 'un.CleanupLegacyArchHandlerRegistration'
$expected = @(
    'DeleteRegValue HKCU "Software\Classes\.zip\OpenWithProgids" "FictionBookEditor.ArchHandler.zip"',
    'DeleteRegValue HKCU "Software\Classes\.rar\OpenWithProgids" "FictionBookEditor.ArchHandler.rar"',
    'DeleteRegKey HKCU "Software\Classes\FictionBookEditor.ArchHandler.zip"',
    'DeleteRegKey HKCU "Software\Classes\FictionBookEditor.ArchHandler.rar"',
    'DeleteRegValue HKCU "Software\RegisteredApplications" "FictionBook Editor ArchHandler"',
    'DeleteRegKey HKCU "Software\FictionBook Editor\ArchHandler"'
)
foreach ($command in $expected) {
    Require $cleanup $command "Upgrade cleanup must contain: $command"
    Require $uninstallCleanup $command "Uninstall cleanup must contain: $command"
}
foreach ($body in @($cleanup, $uninstallCleanup)) {
    foreach ($unsafe in @('UserChoice', 'DeleteRegValue HKCU "Software\Classes\.zip" ""', 'DeleteRegValue HKCU "Software\Classes\.rar" ""', 'DeleteRegKey HKCU "Software\Classes\.zip"', 'DeleteRegKey HKCU "Software\Classes\.rar"')) {
        Forbid $body $unsafe "Legacy cleanup must not change the user's ZIP/RAR default association: $unsafe"
    }
}

$outsideCleanup = $script.Replace($cleanup, '').Replace($uninstallCleanup, '')
foreach ($legacyRegistration in @('FictionBookEditor.ArchHandler', 'FictionBook Editor ArchHandler', 'Software\FictionBook Editor\ArchHandler')) {
    Forbid $outsideCleanup $legacyRegistration "Installer must not register ArchHandler: $legacyRegistration"
}
Require $script 'Call CleanupLegacyArchHandlerRegistration' 'Successful installed upgrade must remove old ArchHandler registration.'
Require $script 'Call un.CleanupLegacyArchHandlerRegistration' 'Uninstall must remove any remaining old ArchHandler registration.'

Write-Host 'NSIS legacy ArchHandler cleanup contract passed.'
