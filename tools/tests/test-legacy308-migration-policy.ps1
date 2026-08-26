<# Keeps the temporary updater bridge scoped to the published 3.0.8 line. #>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
. (Join-Path $root 'tools\build\UpdateVersion.ps1')
if (-not (Test-FbeLegacy308MigrationRequired '3.0.8-rc.2') -or
    -not (Test-FbeLegacy308MigrationRequired '3.0.8') -or
    (Test-FbeLegacy308MigrationRequired '3.0.9') -or
    (Test-FbeLegacy308MigrationRequired '3.1.0-rc.1') -or
    (Test-FbeLegacy308MigrationRequired '4.0.0-rc.1')) {
    throw 'Legacy migration policy must be true only for the 3.0.8 release line.'
}
Write-Host 'Legacy 3.0.8 migration policy passed.'
