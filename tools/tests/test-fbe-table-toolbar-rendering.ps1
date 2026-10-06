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
<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>toolbar</book-title><lang>en</lang></title-info><document-info><program-used>test</program-used><id>toolbar-test</id><version>1.0</version></document-info></description><body><section><p>Outside table.</p><table><tr><th>head</th><td>one</td></tr><tr><td>two</td><td>three</td></tr></table></section></body></FictionBook>
'@ | Set-Content -LiteralPath $fixture -Encoding utf8
    $oldMode, $oldScenario = $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO
    try {
        $env:FBE_NEXT_TEST_MODE = '1'; $env:FBE_NEXT_TEST_SCENARIO = 'table-toolbar-rendering'
        $process = Start-Process -FilePath $isolation.Exe -WorkingDirectory $isolation.Runtime -ArgumentList @('-b', $report, '--portable', $fixture) -PassThru
        $exitCode = Wait-IsolatedFbeProcess -Process $process -Isolation $isolation -Scenario 'table-toolbar-rendering' -Report $report -TimeoutSeconds $TimeoutSeconds
        if ($exitCode -ne 0 -or -not (Test-Path -LiteralPath $report)) { throw "FBE toolbar scenario failed: exit $exitCode." }
    } finally { $env:FBE_NEXT_TEST_MODE=$oldMode; $env:FBE_NEXT_TEST_SCENARIO=$oldScenario }
    $rows = @(Import-Csv -LiteralPath $report -Delimiter "`t")
    $commands = @($rows.command_id | Select-Object -Unique)
    if ($commands.Count -ne 8) { throw "Expected 8 table commands, got $($commands.Count)." }
    foreach ($command in $commands) {
        $outside = @($rows | Where-Object { $_.command_id -eq $command -and $_.phase -like 'outside-*' })
        $inside = @($rows | Where-Object { $_.command_id -eq $command -and $_.phase -like 'inside-*' })
        if ($outside.Count -ne 2 -or $inside.Count -ne 3) { throw "Incomplete transition matrix for command $command." }
        if (@($outside | Where-Object enabled -ne 0).Count) { throw "Command $command remained enabled outside a table." }
        if (@($inside | Where-Object image_index -lt 0).Count) { throw "Command $command lost its toolbar image." }
        if (@($outside + $inside | Where-Object image_list_has_mask -ne 1).Count) { throw "Command $command rendered without an image-list mask plane." }
        $disabledChroma = ($outside | Measure-Object -Property chroma_pixels -Maximum).Maximum
        $enabledRows = @($inside | Where-Object enabled -eq 1)
        if ($enabledRows.Count -eq 0) { continue }
        $enabledChroma = ($enabledRows | Measure-Object -Property chroma_pixels -Minimum).Minimum
        if ($enabledChroma -le $disabledChroma) { throw "Command $command enabled rendering is not more chromatic than disabled rendering ($enabledChroma <= $disabledChroma)." }
    }
    foreach($command in $commands[0..5]) { if(@($rows | Where-Object { $_.command_id -eq $command -and $_.phase -like 'inside-*' -and $_.enabled -ne 1 }).Count) { throw "Structural command $command was disabled in a valid 2x2 table." } }
    $makeHeader, $makeNormal = $commands[6], $commands[7]
    if(@($rows | Where-Object { $_.command_id -eq $makeHeader -and $_.phase -like 'inside-1' -and $_.enabled -ne 1 }).Count -or
       @($rows | Where-Object { $_.command_id -eq $makeHeader -and $_.phase -eq 'inside-2' -and $_.enabled -ne 0 }).Count) { throw 'Make header must be enabled for TD and disabled for TH.' }
    if(@($rows | Where-Object { $_.command_id -eq $makeNormal -and $_.phase -like 'inside-1' -and $_.enabled -ne 0 }).Count -or
       @($rows | Where-Object { $_.command_id -eq $makeNormal -and $_.phase -eq 'inside-2' -and $_.enabled -ne 1 }).Count) { throw 'Make normal must be disabled for TD and enabled for TH.' }
    $passed = $true
    Write-Host 'FBE table toolbar state and rendering transitions passed.'
} finally {
    Complete-IsolatedFbeRuntime -Isolation $isolation -Passed ($passed -and -not $KeepArtifacts)
}
