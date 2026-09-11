[CmdletBinding()]
param([string]$FbeExe = (Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe'), [int]$TimeoutSeconds = 180)
$ErrorActionPreference='Stop'
$contract=Join-Path $PSScriptRoot 'test-fbe-image-document-inserter-contract.ps1'
& $contract
$test=Join-Path $PSScriptRoot 'test-fbe-image-import-generated-id-production.ps1'
& $test -FbeExe $FbeExe -TimeoutSeconds $TimeoutSeconds -Inline '0'
& $test -FbeExe $FbeExe -TimeoutSeconds $TimeoutSeconds -Inline '1'
Write-Host 'Image document inserter block/inline runtime passed.'
