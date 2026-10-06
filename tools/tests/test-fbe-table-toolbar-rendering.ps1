<#
.SYNOPSIS
Exercises table toolbar state transitions in a real FBE process and compares
the painted button chroma for disabled and enabled states.
#>
[CmdletBinding()]
param([string]$FbeExe = (Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe'), [int]$TimeoutSeconds = 90, [switch]$KeepArtifacts)
$ErrorActionPreference = 'Stop'
$FbeExe = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($FbeExe)
if (-not (Test-Path -LiteralPath $FbeExe -PathType Leaf)) { throw "Не найден FBE: $FbeExe" }
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$isolation = New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'table-toolbar'
$directory = $isolation.Root
$passed = $false
try {
    $fixture = Join-Path $directory 'toolbar.fb2'; $report = Join-Path $directory 'toolbar.tsv'
    @'
<?xml version="1.0" encoding="utf-8"?>
<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>toolbar</book-title><lang>en</lang></title-info><document-info><program-used>test</program-used><id>toolbar-test</id><version>1.0</version></document-info></description><body><section><p id="outside">Outside table.</p><table><tr><th id="h0">head1</th><th id="h1">head2</th></tr><tr><td id="d0">one</td><td id="d1">two</td></tr></table><table><tr><td id="one1">single</td></tr></table><table><tr><td id="row0">one</td><td id="row1">two</td><td id="row2">three</td></tr></table><table><tr><td id="col0">one</td></tr><tr><td id="col1">two</td></tr><tr><td id="col2">three</td></tr></table></section></body></FictionBook>
'@ | Set-Content -LiteralPath $fixture -Encoding utf8
    $oldMode, $oldScenario = $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO
    try {
        $env:FBE_NEXT_TEST_MODE = '1'; $env:FBE_NEXT_TEST_SCENARIO = 'table-toolbar-rendering'
        $process = Start-Process -FilePath $isolation.Exe -WorkingDirectory $isolation.Runtime -ArgumentList @('-b', $report, '--portable', $fixture) -PassThru
        $exitCode = Wait-IsolatedFbeProcess -Process $process -Isolation $isolation -Scenario 'table-toolbar-rendering' -Report $report -TimeoutSeconds $TimeoutSeconds
        if ($exitCode -ne 0 -or -not (Test-Path -LiteralPath $report)) { throw "FBE toolbar scenario failed: exit $exitCode." }
    } finally { $env:FBE_NEXT_TEST_MODE=$oldMode; $env:FBE_NEXT_TEST_SCENARIO=$oldScenario }
    $rows = @(Import-Csv -LiteralPath $report -Delimiter "`t")
    $standard = @($rows | Where-Object { $_.phase -notlike 'scaled-*' })
    $commands = @($standard.command_id | Select-Object -Unique)
    if ($commands.Count -ne 8) { throw "Expected 8 table commands, got $($commands.Count)." }
    $phases = @('outside', 'td', 'th', 'td-td', 'th-th', 'td-th', 'one-by-one', 'one-by-n', 'n-by-one', 'n-by-m')
    foreach ($command in $commands) {
        $commandRows = @($standard | Where-Object command_id -eq $command)
        if ($commandRows.Count -ne $phases.Count) { throw "Incomplete selection matrix for command $command." }
        foreach($phase in $phases) { if(@($commandRows | Where-Object phase -eq $phase).Count -ne 1) { throw "Missing $phase state for command $command." } }
        if (@($commandRows | Where-Object { $_.enabled -ne $_.ui_enabled }).Count) { throw "Toolbar and UpdateUI state differ for command $command." }
        if (@($commandRows | Where-Object { $_.image_index -lt 0 -or $_.image_list_has_mask -ne 1 }).Count) { throw "Command $command lost its masked toolbar image." }
    }
    $byPhase = { param($phase) @($standard | Where-Object phase -eq $phase) }
    if (@(& $byPhase 'outside' | Where-Object enabled -ne 0).Count) { throw 'Table actions remained enabled outside a table.' }
    foreach($phase in @('td','th','td-td','th-th','td-th','one-by-one','one-by-n','n-by-one','n-by-m')) {
        $rowsForPhase = & $byPhase $phase
        foreach($command in @($commands[0], $commands[1], $commands[3], $commands[4])) { if(@($rowsForPhase | Where-Object { $_.command_id -eq $command -and $_.enabled -ne 1 }).Count) { throw "$phase disabled a structural insert command." } }
    }
    $deleteRow, $deleteColumn, $makeHeader, $makeNormal = $commands[2], $commands[5], $commands[6], $commands[7]
    foreach($phase in @('one-by-one','one-by-n')) { if(@(& $byPhase $phase | Where-Object { $_.command_id -eq $deleteRow -and $_.enabled -ne 0 }).Count) { throw "$phase allowed deletion of its only row." } }
    foreach($phase in @('one-by-one','n-by-one')) { if(@(& $byPhase $phase | Where-Object { $_.command_id -eq $deleteColumn -and $_.enabled -ne 0 }).Count) { throw "$phase allowed deletion of its only column." } }
    foreach($phase in @('td','td-td','one-by-one','one-by-n','n-by-one')) {
        if(@(& $byPhase $phase | Where-Object { $_.command_id -eq $makeHeader -and $_.enabled -ne 1 }).Count -or @(& $byPhase $phase | Where-Object { $_.command_id -eq $makeNormal -and $_.enabled -ne 0 }).Count) { throw "$phase must enable only Make Header." }
    }
    foreach($phase in @('th','th-th')) {
        if(@(& $byPhase $phase | Where-Object { $_.command_id -eq $makeHeader -and $_.enabled -ne 0 }).Count -or @(& $byPhase $phase | Where-Object { $_.command_id -eq $makeNormal -and $_.enabled -ne 1 }).Count) { throw "$phase must enable only Make Normal." }
    }
    if(@(& $byPhase 'td-th' | Where-Object { ($_.command_id -eq $makeHeader -or $_.command_id -eq $makeNormal) -and $_.enabled -ne 1 }).Count -or @(& $byPhase 'n-by-m' | Where-Object { ($_.command_id -eq $makeHeader -or $_.command_id -eq $makeNormal -or $_.command_id -eq $deleteRow -or $_.command_id -eq $deleteColumn) -and $_.enabled -ne 1 }).Count) { throw 'Mixed TD+TH selections, including rectangular N×M, must enable both conversions and both deletions.' }
    $dpiSizes = @{ 'scaled-96' = 24; 'scaled-120' = 30; 'scaled-144' = 36; 'scaled-168' = 42; 'scaled-192' = 48 }
    foreach($phase in $dpiSizes.Keys) {
        $scaled = @($rows | Where-Object phase -eq $phase)
        if($scaled.Count -ne 8) { throw "Expected eight scaled bitmaps for $phase." }
        if(@($scaled | Where-Object { $_.image_index -ne $dpiSizes[$phase] -or $_.image_list_has_mask -ne 1 -or $_.chroma_pixels -le 0 -or $_.image_black_pixels -ne 0 -or $_.checked -ne 0 -or $_.hidden -ne 1 }).Count) { throw "$phase produced a transparent-background, magenta, or black-fringe artifact." }
    }    $passed = $true
    Write-Host 'FBE table toolbar state and rendering transitions passed.'
} finally {
    Complete-IsolatedFbeRuntime -Isolation $isolation -Passed ($passed -and -not $KeepArtifacts)
}
