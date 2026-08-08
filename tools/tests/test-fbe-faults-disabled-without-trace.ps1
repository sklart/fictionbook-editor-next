<# Proves test-only fault switches have no effect unless a diagnostic trace is active. #>
[CmdletBinding()]
param([string]$Configuration = 'Release')
$ErrorActionPreference = 'Stop'
$previousTrace = $env:FBE_NEXT_TRACE
$previousTest = $env:FBE_NEXT_TEST_MODE
$previousFault = $env:FBE_NEXT_FAULT_INJECT
try {
    $env:FBE_NEXT_TRACE = '0'
    $env:FBE_NEXT_TEST_MODE = '1'
    foreach($fault in @('api-load-exception', 'first-set-external', 'second-set-external', 'inflate-paragraphs')) {
        $env:FBE_NEXT_FAULT_INJECT = $fault
        & (Join-Path $PSScriptRoot 'test-fbe-startup.ps1') -Configuration $Configuration -TimeoutSeconds 90
        Write-Host "Fault '$fault' was ignored without diagnostic trace."
    }
}
finally {
    $env:FBE_NEXT_TRACE = $previousTrace
    $env:FBE_NEXT_TEST_MODE = $previousTest
    $env:FBE_NEXT_FAULT_INJECT = $previousFault
}
