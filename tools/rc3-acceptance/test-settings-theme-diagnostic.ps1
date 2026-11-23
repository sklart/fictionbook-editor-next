[CmdletBinding()]
param([Parameter(Mandatory)][string]$FbeExe)
$ErrorActionPreference='Stop'
if($env:GITHUB_ACTIONS -ne 'true' -or $env:RUNNER_ENVIRONMENT -ne 'github-hosted'){throw 'Disposable hosted runner required.'}
& (Join-Path $PSScriptRoot '../tests/test-interface-theme-policy-runtime.ps1') -FbeExe $FbeExe
