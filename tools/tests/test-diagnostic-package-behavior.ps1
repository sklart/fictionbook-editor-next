[CmdletBinding()]
param([string]$Configuration='Release')
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
& (Join-Path $PSScriptRoot 'test-diagnostic-sessions.ps1') -Executable (Join-Path $root "out/$Configuration/FBE.exe") -Scenario Active
