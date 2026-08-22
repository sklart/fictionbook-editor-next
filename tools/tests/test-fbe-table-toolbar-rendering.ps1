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
$directory = Join-Path ([IO.Path]::GetTempPath()) ('fbe-table-toolbar-' + [guid]::NewGuid().ToString('N'))
[void](New-Item -ItemType Directory -Path $directory)
try {
    $fixture = Join-Path $directory 'toolbar.fb2'; $report = Join-Path $directory 'toolbar.tsv'
    @'
<?xml version="1.0" encoding="utf-8"?>
<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>toolbar</book-title><lang>en</lang></title-info><document-info><program-used>test</program-used><id>toolbar-test</id><version>1.0</version></document-info></description><body><section><p>Outside table.</p><table><tr><th>head</th><td>one</td></tr><tr><td>two</td><td>three</td></tr></table></section></body></FictionBook>
'@ | Set-Content -LiteralPath $fixture -Encoding utf8
    $oldMode, $oldScenario = $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO
    try {
        $env:FBE_NEXT_TEST_MODE = '1'; $env:FBE_NEXT_TEST_SCENARIO = 'table-toolbar-rendering'
        $process = Start-Process -FilePath $FbeExe -ArgumentList @('-b', $report, $fixture) -PassThru
        if (-not $process.WaitForExit($TimeoutSeconds * 1000)) { Stop-Process -Id $process.Id -Force; throw 'FBE не завершил toolbar rendering scenario.' }
        if ($process.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $report)) { throw "FBE toolbar scenario failed: exit $($process.ExitCode)." }
    } finally { $env:FBE_NEXT_TEST_MODE=$oldMode; $env:FBE_NEXT_TEST_SCENARIO=$oldScenario }
    $rows = @(Import-Csv -LiteralPath $report -Delimiter "`t")
    $commands = @($rows.command_id | Select-Object -Unique)
    if ($commands.Count -ne 8) { throw "Expected 8 table commands, got $($commands.Count)." }
    foreach ($command in $commands) {
        $outside = @($rows | Where-Object { $_.command_id -eq $command -and $_.phase -like 'outside-*' })
        $inside = @($rows | Where-Object { $_.command_id -eq $command -and $_.phase -like 'inside-*' })
        if ($outside.Count -ne 2 -or $inside.Count -ne 3) { throw "Incomplete transition matrix for command $command." }
        if (@($outside | Where-Object enabled -ne 0).Count) { throw "Command $command remained enabled outside a table." }
        if (@($inside | Where-Object enabled -ne 1).Count) { throw "Command $command was disabled inside a table." }
        if (@($inside | Where-Object image_index -lt 0).Count) { throw "Command $command lost its toolbar image." }
        $disabledChroma = ($outside | Measure-Object -Property chroma_pixels -Maximum).Maximum
        $enabledChroma = ($inside | Measure-Object -Property chroma_pixels -Minimum).Minimum
        if ($enabledChroma -le $disabledChroma) { throw "Command $command enabled rendering is not more chromatic than disabled rendering ($enabledChroma <= $disabledChroma)." }
    }
    Write-Host 'FBE table toolbar state and rendering transitions passed.'
} finally {
    if ($KeepArtifacts) { Write-Host "Toolbar artifacts: $directory" } else { Remove-Item -LiteralPath $directory -Recurse -Force -ErrorAction SilentlyContinue }
}
