<#
.SYNOPSIS
Exercises a structural table fault after the handler has opened its markup
undo unit, then proves that a subsequent normal command still saves and
round-trips through FictionBook.xsd.
#>
[CmdletBinding()]
param([string]$FbeExe, [int]$TimeoutSeconds = 180)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$editor = Join-Path $root 'src\fbe\table\TableStructuralEditor.cpp'
$production = Join-Path $PSScriptRoot 'test-fbe-table-structural-production.ps1'
$source = Get-Content -Raw -LiteralPath $editor

foreach($required in @('FBE_NEXT_TEST_MODE', 'table-structural-after-mutation', 'InjectTestFaultAfterMutation')) {
    if(-not $source.Contains($required)) { throw "Нет test-only structural fault hook: $required" }
}

$arguments = @{ FixtureId = 'plain'; Operation = 'insert-row-below'; TimeoutSeconds = $TimeoutSeconds }
if($FbeExe) { $arguments.FbeExe = $FbeExe }

& $production @arguments -Fault 'table-structural-after-mutation'
& $production @arguments
Write-Host 'Table structural undo failure safety passed.'
