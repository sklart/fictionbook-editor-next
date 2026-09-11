<# Uses the production FBE MSHTML load/save route.  The structural-table
fixture exercises Normalize() with loose section text, native TABLE, TD/P and
the save serializer; the delegated test asserts live-DOM snapshots and XSD. #>
[CmdletBinding()]
param([string]$FbeExe = (Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe'), [int]$TimeoutSeconds = 180)
$ErrorActionPreference='Stop'
& (Join-Path $PSScriptRoot 'test-fbe-table-structural-production.ps1') -FbeExe $FbeExe -FixtureId preserve -Operation insert-row-below -TimeoutSeconds $TimeoutSeconds
Write-Host 'Visual DOM normalizer production runtime passed.'
