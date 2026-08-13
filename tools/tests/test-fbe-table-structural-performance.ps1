<#
.SYNOPSIS
Measures production CFBEView structural table commands on a 356 x 14 table.
#>
[CmdletBinding()]
param([string]$FbeExe = (Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe'), [int]$TimeoutSeconds = 180)

$ErrorActionPreference='Stop'
$FbeExe=$ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($FbeExe)
if(-not (Test-Path -LiteralPath $FbeExe -PathType Leaf)) { throw "Не найден FBE: $FbeExe" }
$directory=Join-Path ([IO.Path]::GetTempPath()) ('fbe-table-structural-performance-'+[guid]::NewGuid().ToString('N'))
[void](New-Item -ItemType Directory -Path $directory)
try {
    $fixture=Join-Path $directory 'huge-structural.fb2'; $report=Join-Path $directory 'huge-structural.tsv'
    $table=New-Object Text.StringBuilder
    for($row=0;$row -lt 356;$row++) { [void]$table.Append('<tr>'); for($column=0;$column -lt 14;$column++) { [void]$table.Append('<td>cell</td>') }; [void]$table.Append('</tr>') }
    ('<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>huge structural table</book-title><lang>en</lang></title-info><document-info><program-used>test</program-used><id>huge-structural-table</id><version>1.0</version></document-info></description><body><section><table>' + $table + '</table></section></body></FictionBook>') | Set-Content -LiteralPath $fixture -Encoding utf8
    $oldMode=$env:FBE_NEXT_TEST_MODE; $oldScenario=$env:FBE_NEXT_TEST_SCENARIO
    try {
        $env:FBE_NEXT_TEST_MODE='1'; $env:FBE_NEXT_TEST_SCENARIO='table-structural'
        $process=Start-Process -FilePath $FbeExe -ArgumentList @('-b',$report,$fixture) -PassThru
        if(-not $process.WaitForExit($TimeoutSeconds*1000)) { Stop-Process -Id $process.Id -Force; throw 'FBE не завершил huge structural benchmark.' }
        if($process.ExitCode -ne 0) { throw "FBE вернул код $($process.ExitCode) для huge structural benchmark." }
    } finally {
        if($null -eq $oldMode){Remove-Item Env:FBE_NEXT_TEST_MODE -ErrorAction SilentlyContinue}else{$env:FBE_NEXT_TEST_MODE=$oldMode}
        if($null -eq $oldScenario){Remove-Item Env:FBE_NEXT_TEST_SCENARIO -ErrorAction SilentlyContinue}else{$env:FBE_NEXT_TEST_SCENARIO=$oldScenario}
    }
    $rows=Import-Csv -LiteralPath $report -Delimiter "`t"
    $measurements=@(foreach($operation in @('toggle-header','insert-row-above','insert-row-below','delete-row','insert-column-left','insert-column-right','delete-column','make-header','make-normal')) {
        $before=$rows|Where-Object phase -eq "$operation-before"; $after=$rows|Where-Object phase -eq "$operation-after"; $undo=$rows|Where-Object phase -eq "$operation-undo"; $redo=$rows|Where-Object phase -eq "$operation-redo"
        if(@($before,$after,$undo,$redo).Count -ne 4) { throw "Неполный benchmark snapshot: $operation." }
        if([int64]$undo.elapsed_ms -lt [int64]$after.elapsed_ms -or [int64]$redo.elapsed_ms -lt [int64]$undo.elapsed_ms) { throw "Некорректная временная последовательность $operation." }
        [pscustomobject]@{ Operation=$operation; Handler_ms=([int64]$after.elapsed_ms-[int64]$before.elapsed_ms); Undo_ms=([int64]$undo.elapsed_ms-[int64]$after.elapsed_ms); Redo_ms=([int64]$redo.elapsed_ms-[int64]$undo.elapsed_ms) }
    })
    $measurements | Format-Table -AutoSize | Out-Host
    $complete=$rows|Where-Object phase -eq 'save-complete'
    if(@($complete).Count -ne 1 -or [int]$complete.table_count -ne 1) { throw 'Huge structural benchmark не сохранил таблицу.' }
    Write-Host 'Huge structural table benchmark passed (356 x 14, 4984 initial TD).'
} finally { Remove-Item -LiteralPath $directory -Recurse -Force -ErrorAction SilentlyContinue }
