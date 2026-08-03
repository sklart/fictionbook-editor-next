<#
.SYNOPSIS
Запускает полный набор контрактов диагностической инфраструктуры FBE.
#>
[CmdletBinding()]
param(
    [switch]$SkipStartupSmoke
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path $PSScriptRoot)
$tests = @(
    'test-fbe-main-menu-connected-resource.ps1',
    'test-fbe-main-menu-generated-resource.ps1',
    'test-fbe-main-menu-catalog.ps1',
    'test-diagnostic-build-artifacts.ps1',
    'test-resource-id-safety.ps1',
    'test-diagnostic-localization.ps1',
    'test-diagnostic-log-segments.ps1',
    'test-diagnostic-trace-privacy.ps1',
    'test-fbe-trace-bridge.ps1',
    'test-fbe-typelib-diagnostics.ps1'
)

foreach($test in $tests)
{
    $path = Join-Path $PSScriptRoot $test
    if(-not (Test-Path -LiteralPath $path)) { throw "Diagnostic test not found: $test" }
    Write-Host "== $test =="
    & $path
}

if(-not $SkipStartupSmoke)
{
    Write-Host '== test-fbe-startup.ps1 =='
    & (Join-Path $PSScriptRoot 'test-fbe-startup.ps1')
}

Write-Host 'Diagnostic test suite passed.'
