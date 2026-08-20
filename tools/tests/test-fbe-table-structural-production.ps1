<#
.SYNOPSIS
Exercises production CFBEView table handlers with live DOM snapshots and Undo/Redo.
#>
[CmdletBinding()]
param([string]$FbeExe = (Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe'), [int]$TimeoutSeconds = 180, [switch]$KeepArtifacts, [string]$Target, [string]$Operation, [string]$SecondOperation, [string]$FixtureId, [string]$RuntimeStyle)

$ErrorActionPreference = 'Stop'
$requestedOperation = $Operation
$FbeExe = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($FbeExe)
if(-not (Test-Path -LiteralPath $FbeExe -PathType Leaf)) { throw "Не найден FBE: $FbeExe" }
$schemaPath = Join-Path $PSScriptRoot '..\..\runtime\FictionBook.xsd'
function Assert-Schema([string]$Path) {
    $cache = New-Object -ComObject Msxml2.XMLSchemaCache.6.0; $cache.add('http://www.gribuser.ru/xml/fictionbook/2.0', $schemaPath)
    $doc = New-Object -ComObject Msxml2.DOMDocument.6.0; $doc.async = $false
    if(-not $doc.load($Path)) { throw "MSXML не прочитал FB2: $($doc.parseError.reason)" }; $doc.schemas = $cache
    if($doc.validate().errorCode -ne 0) { throw 'FictionBook.xsd validation failed.' }
}
function Invoke-Fbe([string[]]$Arguments, [string]$Name) {
    $process=Start-Process -FilePath $FbeExe -ArgumentList $Arguments -PassThru
    if(-not $process.WaitForExit($TimeoutSeconds*1000)) { Stop-Process -Id $process.Id -Force; throw "FBE не завершил $Name." }
    if($process.ExitCode -ne 0) { throw "FBE вернул код $($process.ExitCode): $Name." }
    # MSHTML/common controls may finish tearing down their UI just after the
    # batch process exits.  The structural suite starts many GUI instances;
    # leave a small process-lifecycle gap before the next one to avoid a
    # cross-process teardown race without retrying or weakening any scenario.
    Start-Sleep -Seconds 1
}
$directory=Join-Path ([IO.Path]::GetTempPath()) ('fbe-table-structural-'+[guid]::NewGuid().ToString('N'))
[void](New-Item -ItemType Directory -Path $directory)
try {
    $bulkTable = New-Object Text.StringBuilder
    for($row=0;$row -lt 10;$row++) { [void]$bulkTable.Append('<tr>'); for($column=0;$column -lt 10;$column++) { [void]$bulkTable.Append("<td id=`"bulk-$row-$column`">$row-$column</td>") }; [void]$bulkTable.Append('</tr>') }
    $bulkHeaderTable = $bulkTable.ToString().Replace('<td ', '<th ').Replace('</td>', '</th>')
    $preserveTable = '<tr><td id="preserve" style="color: red" fbstyle="preserve-style" colspan="2" fbcolspan="2" rowspan="2" fbrowspan="2" align="center" fbalign="center" valign="top" fbvalign="top"><strong>Foo Bar</strong> <emphasis>MiXeD</emphasis> <a href="#note">Link</a></td><td>companion</td></tr><tr><td>tail</td></tr>'
    $preserveHeaderTable = $preserveTable.Replace('<td id="preserve"', '<th id="preserve"').Replace('</td><td>companion</td>', '</th><td>companion</td>')
    $fixtures = @(
        @{ Id='plain'; Table='<tr><td>one</td><td>two</td><td>three</td></tr><tr><td>four</td><td>five</td><td>six</td></tr><tr><td>seven</td><td>eight</td><td>nine</td></tr>' },
        @{ Id='colspan'; Table='<tr><td colspan="2">one</td><td>two</td></tr><tr><td>three</td><td>four</td><td>five</td></tr><tr><td>six</td><td>seven</td><td>eight</td></tr>' },
        @{ Id='rowspan'; Table='<tr><td rowspan="2">one</td><td>two</td><td>three</td></tr><tr><td>four</td><td>five</td></tr><tr><td>six</td><td>seven</td><td>eight</td></tr>' },
        @{ Id='combined'; Table='<tr><th id="h" colspan="2">head</th><td>one</td></tr><tr><td rowspan="2">two</td><td>three</td><td>four</td></tr><tr><td>five</td><td>six</td></tr>' },
        @{ Id='mixed'; Table='<tr><th>one</th><th>two</th><th>three</th></tr><tr><td>four</td><td>five</td><td>six</td></tr><tr><td>seven</td><td>eight</td><td>nine</td></tr>' },
        @{ Id='all-header'; Table='<tr><th>one</th><th>two</th></tr><tr><th>three</th><th>four</th></tr>' },
        @{ Id='edge-spans'; Table='<tr><td colspan="2">first</td><td>middle</td><td colspan="2">last</td></tr><tr><td rowspan="2">one</td><td>two</td><td colspan="2" rowspan="2">three</td><td>four</td></tr><tr><td>five</td><td>six</td></tr>' },
        @{ Id='bulk-10x10'; Table=$bulkTable.ToString() },
        @{ Id='bulk-header-10x10'; Table=$bulkHeaderTable },
        @{ Id='preserve'; Table=$preserveTable },
        @{ Id='preserve-header'; Table=$preserveHeaderTable },
        @{ Id='toggle-preserve'; Table=$preserveTable },
        @{ Id='toggle-preserve-header'; Table=$preserveHeaderTable }
    )
    if($FixtureId) { $fixtures=@($fixtures | Where-Object Id -eq $FixtureId); if($fixtures.Count -ne 1) { throw "Не найден structural fixture: $FixtureId" } }
    foreach($case in $fixtures) {
        $fixture=Join-Path $directory ($case.Id + '.fb2'); $report=Join-Path $directory ($case.Id + '.tsv'); $reopen=Join-Path $directory ($case.Id + '-reopen.tsv')
        ('<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>structural table</book-title><lang>en</lang></title-info><document-info><program-used>test</program-used><id>structural-' + $case.Id + '</id><version>1.0</version></document-info></description><body><section><table id="structural">' + $case.Table + '</table></section></body></FictionBook>') | Set-Content -LiteralPath $fixture -Encoding utf8
        $oldMode=$env:FBE_NEXT_TEST_MODE; $oldScenario=$env:FBE_NEXT_TEST_SCENARIO; $oldTarget=$env:FBE_NEXT_TEST_TABLE_TARGET; $oldOperation=$env:FBE_NEXT_TEST_TABLE_OPERATION; $oldSecondOperation=$env:FBE_NEXT_TEST_TABLE_SECOND_OPERATION; $oldRuntimeStyle=$env:FBE_NEXT_TEST_TABLE_RUNTIME_STYLE
        try { $env:FBE_NEXT_TEST_MODE='1'; $env:FBE_NEXT_TEST_SCENARIO='table-structural'; if($Target){$env:FBE_NEXT_TEST_TABLE_TARGET=$Target}; if($Operation){$env:FBE_NEXT_TEST_TABLE_OPERATION=$Operation}; if($SecondOperation){$env:FBE_NEXT_TEST_TABLE_SECOND_OPERATION=$SecondOperation}; if($RuntimeStyle){$env:FBE_NEXT_TEST_TABLE_RUNTIME_STYLE=$RuntimeStyle}; Invoke-Fbe @('-b',$report,$fixture) "table structural handlers ($($case.Id))" }
        finally { foreach($state in @(@('FBE_NEXT_TEST_MODE',$oldMode),@('FBE_NEXT_TEST_SCENARIO',$oldScenario),@('FBE_NEXT_TEST_TABLE_TARGET',$oldTarget),@('FBE_NEXT_TEST_TABLE_OPERATION',$oldOperation),@('FBE_NEXT_TEST_TABLE_SECOND_OPERATION',$oldSecondOperation),@('FBE_NEXT_TEST_TABLE_RUNTIME_STYLE',$oldRuntimeStyle))){if($null -eq $state[1]){Remove-Item ("Env:"+$state[0]) -ErrorAction SilentlyContinue}else{Set-Item ("Env:"+$state[0]) $state[1]}} }
        $rows=Import-Csv -LiteralPath $report -Delimiter "`t"
        foreach($operation in $(if($Operation){@($Operation)}else{@('toggle-header','insert-row-above','insert-row-below','delete-row','insert-column-left','insert-column-right','delete-column','make-header','make-normal')})) {
            $before=$rows|Where-Object phase -eq "$operation-before"; $after=$rows|Where-Object phase -eq "$operation-after"; $undo=$rows|Where-Object phase -eq "$operation-undo"; $redo=$rows|Where-Object phase -eq "$operation-redo"
            if(@($before,$after,$undo,$redo).Count -ne 4) { throw "Нет live DOM snapshots для $operation ($($case.Id))." }
            $secondBefore=@($rows|Where-Object phase -eq "$SecondOperation-second-before"); $secondAfter=@($rows|Where-Object phase -eq "$SecondOperation-second-after")
            if($SecondOperation -and ($secondBefore.Count -ne 1 -or $secondAfter.Count -ne 1)) { throw "Нет second-operation snapshots для $operation → $SecondOperation ($($case.Id))." }
            if($SecondOperation -and $secondAfter[0].grid_signature -eq $secondBefore[0].grid_signature) { throw "Вторая операция $SecondOperation не изменила semantic grid ($($case.Id))." }
            if($case.Id -eq 'colspan' -and $operation -eq 'delete-column' -and $SecondOperation) {
                if($after.grid_signature -match 'logical-colspan=2' -or $after.grid_signature -match 'colspan=2|fbcolspan=2') { throw 'Delete Column не нормализовал colspan=2 до 1.' }
				if($after.grid_signature -notmatch 'c0:id=,tag=TD,row=0,column=0,logical-colspan=1,logical-rowspan=1,html=[0-9A-F]+,style=,fbstyle=,colspan=,fbcolspan=,rowspan=,fbrowspan=') { throw 'Delete Column не очистил colspan/fbcolspan после нормализации span=1.' }
            }
            if($case.Id -eq 'rowspan' -and $operation -eq 'delete-row' -and $SecondOperation) {
                if($after.grid_signature -match 'logical-rowspan=2' -or $after.grid_signature -match 'rowspan=2|fbrowspan=2') { throw 'Delete Row не нормализовал rowspan=2 до 1.' }
				if($after.grid_signature -notmatch 'c0:id=,tag=TD,row=0,column=0,logical-colspan=1,logical-rowspan=1,html=[0-9A-F]+,style=,fbstyle=,colspan=,fbcolspan=,rowspan=,fbrowspan=') { throw 'Delete Row не очистил rowspan/fbrowspan после нормализации span=1.' }
            }
            $signature={ param($row) "$($row.table_count)/$($row.tr_count)/$($row.td_count)/$($row.th_count)" }
            if(-not $SecondOperation -and (&$signature $undo) -ne (&$signature $before)) { throw "Undo не восстановил live DOM для $operation ($($case.Id))." }
            if(-not $SecondOperation -and (&$signature $redo) -ne (&$signature $after)) { throw "Redo не восстановил live DOM для $operation ($($case.Id))." }
			$expectChange = if($requestedOperation) {
				-not (($operation -eq 'make-normal' -and $case.Id -notin @('mixed','all-header','bulk-header-10x10','preserve-header')) -or ($operation -eq 'make-header' -and $case.Id -eq 'all-header'))
			} else {
				# В полном сценарии команды выполняются одна за другой над тем же документом:
				# состояние перед каждой командой уже не равно исходной фикстуре.
				$null
			}
			if($null -ne $expectChange -and $expectChange -and (&$signature $after) -eq (&$signature $before) -and $after.grid_signature -eq $before.grid_signature) { throw "Handler $operation не изменил live DOM или semantic grid ($($case.Id))." }
			if($null -ne $expectChange -and -not $expectChange -and ((&$signature $after) -ne (&$signature $before) -or $after.grid_signature -ne $before.grid_signature)) { throw "Идемпотентная команда $operation изменила DOM ($($case.Id))." }
			if(-not $SecondOperation -and $undo.grid_signature -ne $before.grid_signature) { throw "Undo не восстановил logical grid для $operation ($($case.Id))." }
			if(-not $SecondOperation -and $redo.grid_signature -ne $after.grid_signature) { throw "Redo не восстановил logical grid для $operation ($($case.Id))." }
			if($expectChange -and $after.grid_signature -eq $before.grid_signature) { throw "Handler $operation не изменил logical grid ($($case.Id))." }
			if($requestedOperation -and $case.Id -eq 'bulk-10x10' -and $operation -eq 'make-header') {
				if([int]$before.td_count -ne 100 -or [int]$before.th_count -ne 0 -or [int]$after.td_count -ne 0 -or [int]$after.th_count -ne 100 -or [int]$undo.td_count -ne 100 -or [int]$undo.th_count -ne 0 -or [int]$redo.td_count -ne 0 -or [int]$redo.th_count -ne 100) { throw '10x10 Make Header не сохранил ожидаемую матрицу TD/TH через Undo/Redo.' }
			if([int]$after.grid_build_calls -gt 4) { throw "10x10 Make Header построил logical grid слишком много раз: $($after.grid_build_calls)." }
		}
			if($requestedOperation -and $case.Id -eq 'bulk-header-10x10' -and $operation -eq 'make-normal') {
				if([int]$before.td_count -ne 0 -or [int]$before.th_count -ne 100 -or [int]$after.td_count -ne 100 -or [int]$after.th_count -ne 0 -or [int]$undo.td_count -ne 0 -or [int]$undo.th_count -ne 100 -or [int]$redo.td_count -ne 100 -or [int]$redo.th_count -ne 0) { throw '10x10 Make Normal не сохранил ожидаемую матрицу TD/TH через Undo/Redo.' }
			if([int]$after.grid_build_calls -gt 4) { throw "10x10 Make Normal построил logical grid слишком много раз: $($after.grid_build_calls)." }
			}
			if($requestedOperation -and $case.Id -eq 'mixed' -and $operation -eq 'make-header' -and -not $Target) {
				if([int]$before.td_count -ne 6 -or [int]$before.th_count -ne 3 -or [int]$after.td_count -ne 0 -or [int]$after.th_count -ne 9 -or [int]$undo.td_count -ne 6 -or [int]$undo.th_count -ne 3 -or [int]$redo.td_count -ne 0 -or [int]$redo.th_count -ne 9) { throw 'Mixed rectangle Make Header не восстановил матрицу TD/TH через Undo/Redo.' }
			}
			if($requestedOperation -and $case.Id -eq 'mixed' -and $operation -eq 'make-normal' -and -not $Target) {
				if([int]$before.td_count -ne 6 -or [int]$before.th_count -ne 3 -or [int]$after.td_count -ne 9 -or [int]$after.th_count -ne 0 -or [int]$undo.td_count -ne 6 -or [int]$undo.th_count -ne 3 -or [int]$redo.td_count -ne 9 -or [int]$redo.th_count -ne 0) { throw 'Mixed rectangle Make Normal не восстановил матрицу TD/TH через Undo/Redo.' }
			}
			if($requestedOperation -and $case.Id -eq 'mixed' -and $operation -eq 'make-header' -and $Target -eq '1,0:1,2') {
				if([int]$after.td_count -ne 3 -or [int]$after.th_count -ne 6 -or [int]$undo.td_count -ne 6 -or [int]$undo.th_count -ne 3 -or [int]$redo.td_count -ne 3 -or [int]$redo.th_count -ne 6) { throw 'Make Header для целой логической строки изменил ячейки вне выделения.' }
			}
			if($requestedOperation -and $case.Id -eq 'mixed' -and $operation -eq 'make-normal' -and $Target -eq '0,0:2,0') {
				if([int]$after.td_count -ne 7 -or [int]$after.th_count -ne 2 -or [int]$undo.td_count -ne 6 -or [int]$undo.th_count -ne 3 -or [int]$redo.td_count -ne 7 -or [int]$redo.th_count -ne 2) { throw 'Make Normal для целого логического столбца изменил ячейки вне выделения.' }
			}
			if($requestedOperation -and (($case.Id -eq 'preserve' -and $operation -eq 'make-header') -or ($case.Id -eq 'preserve-header' -and $operation -eq 'make-normal'))) {
				$withoutTag = { param($snapshot) $snapshot -replace 'tag=(TD|TH)', 'tag=*' }
				if((&$withoutTag $before.grid_signature) -ne (&$withoutTag $after.grid_signature)) { throw "TD/TH conversion изменила attributes или content ($($case.Id))." }
			}
			if($requestedOperation -and $RuntimeStyle -and $operation -eq 'toggle-header' -and $case.Id -in @('toggle-preserve','toggle-preserve-header')) {
				$withoutTag = { param($snapshot) $snapshot -replace 'tag=(TD|TH)', 'tag=*' }
				if($before.grid_signature -notmatch 'style=[^,;]*37px') { throw "Runtime HTML style не был установлен в visual DOM ($($case.Id))." }
				if((&$withoutTag $before.grid_signature) -ne (&$withoutTag $after.grid_signature)) { throw "Single-cell Toggle Header изменила attributes или content ($($case.Id))." }
			}
        }
        if(-not ($rows|Where-Object phase -eq 'save-complete')) { throw "Production structural scenario не сохранил документ ($($case.Id))." }
        Assert-Schema $fixture
        $oldMode=$env:FBE_NEXT_TEST_MODE; $oldScenario=$env:FBE_NEXT_TEST_SCENARIO
        try { $env:FBE_NEXT_TEST_MODE='1'; $env:FBE_NEXT_TEST_SCENARIO='table-roundtrip'; Invoke-Fbe @('-b',$reopen,$fixture) "reopen and second Save structural table ($($case.Id))" }
        finally { if($null -eq $oldMode){Remove-Item Env:FBE_NEXT_TEST_MODE -ErrorAction SilentlyContinue}else{$env:FBE_NEXT_TEST_MODE=$oldMode}; if($null -eq $oldScenario){Remove-Item Env:FBE_NEXT_TEST_SCENARIO -ErrorAction SilentlyContinue}else{$env:FBE_NEXT_TEST_SCENARIO=$oldScenario} }
        Assert-Schema $fixture
    }
    Write-Host 'Production structural table handlers with Undo/Redo passed.'
}
finally { if($KeepArtifacts) { Write-Host "Артефакты structural test: $directory" } else { Remove-Item -LiteralPath $directory -Recurse -Force -ErrorAction SilentlyContinue } }
