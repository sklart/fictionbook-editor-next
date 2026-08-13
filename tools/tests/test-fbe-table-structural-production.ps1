<#
.SYNOPSIS
Exercises production CFBEView table handlers with live DOM snapshots and Undo/Redo.
#>
[CmdletBinding()]
param([string]$FbeExe = (Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe'), [int]$TimeoutSeconds = 180, [switch]$KeepArtifacts, [string]$Target, [string]$Operation, [string]$SecondOperation, [string]$FixtureId)

$ErrorActionPreference = 'Stop'
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
}
$directory=Join-Path ([IO.Path]::GetTempPath()) ('fbe-table-structural-'+[guid]::NewGuid().ToString('N'))
[void](New-Item -ItemType Directory -Path $directory)
try {
    $bulkTable = New-Object Text.StringBuilder
    for($row=0;$row -lt 10;$row++) { [void]$bulkTable.Append('<tr>'); for($column=0;$column -lt 10;$column++) { [void]$bulkTable.Append("<td id=`"bulk-$row-$column`">$row-$column</td>") }; [void]$bulkTable.Append('</tr>') }
    $fixtures = @(
        @{ Id='plain'; Table='<tr><td>one</td><td>two</td><td>three</td></tr><tr><td>four</td><td>five</td><td>six</td></tr><tr><td>seven</td><td>eight</td><td>nine</td></tr>' },
        @{ Id='colspan'; Table='<tr><td colspan="2">one</td><td>two</td></tr><tr><td>three</td><td>four</td><td>five</td></tr><tr><td>six</td><td>seven</td><td>eight</td></tr>' },
        @{ Id='rowspan'; Table='<tr><td rowspan="2">one</td><td>two</td><td>three</td></tr><tr><td>four</td><td>five</td></tr><tr><td>six</td><td>seven</td><td>eight</td></tr>' },
        @{ Id='combined'; Table='<tr><th id="h" colspan="2">head</th><td>one</td></tr><tr><td rowspan="2">two</td><td>three</td><td>four</td></tr><tr><td>five</td><td>six</td></tr>' },
        @{ Id='mixed'; Table='<tr><th>one</th><th>two</th><th>three</th></tr><tr><td>four</td><td>five</td><td>six</td></tr><tr><td>seven</td><td>eight</td><td>nine</td></tr>' },
        @{ Id='edge-spans'; Table='<tr><td colspan="2">first</td><td>middle</td><td colspan="2">last</td></tr><tr><td rowspan="2">one</td><td>two</td><td colspan="2" rowspan="2">three</td><td>four</td></tr><tr><td>five</td><td>six</td></tr>' },
        @{ Id='bulk-10x10'; Table=$bulkTable.ToString() }
    )
    if($FixtureId) { $fixtures=@($fixtures | Where-Object Id -eq $FixtureId); if($fixtures.Count -ne 1) { throw "Не найден structural fixture: $FixtureId" } }
    foreach($case in $fixtures) {
        $fixture=Join-Path $directory ($case.Id + '.fb2'); $report=Join-Path $directory ($case.Id + '.tsv'); $reopen=Join-Path $directory ($case.Id + '-reopen.tsv')
        ('<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>structural table</book-title><lang>en</lang></title-info><document-info><program-used>test</program-used><id>structural-' + $case.Id + '</id><version>1.0</version></document-info></description><body><section><table id="structural">' + $case.Table + '</table></section></body></FictionBook>') | Set-Content -LiteralPath $fixture -Encoding utf8
        $oldMode=$env:FBE_NEXT_TEST_MODE; $oldScenario=$env:FBE_NEXT_TEST_SCENARIO; $oldTarget=$env:FBE_NEXT_TEST_TABLE_TARGET; $oldOperation=$env:FBE_NEXT_TEST_TABLE_OPERATION; $oldSecondOperation=$env:FBE_NEXT_TEST_TABLE_SECOND_OPERATION
        try { $env:FBE_NEXT_TEST_MODE='1'; $env:FBE_NEXT_TEST_SCENARIO='table-structural'; if($Target){$env:FBE_NEXT_TEST_TABLE_TARGET=$Target}; if($Operation){$env:FBE_NEXT_TEST_TABLE_OPERATION=$Operation}; if($SecondOperation){$env:FBE_NEXT_TEST_TABLE_SECOND_OPERATION=$SecondOperation}; Invoke-Fbe @('-b',$report,$fixture) "table structural handlers ($($case.Id))" }
        finally { foreach($state in @(@('FBE_NEXT_TEST_MODE',$oldMode),@('FBE_NEXT_TEST_SCENARIO',$oldScenario),@('FBE_NEXT_TEST_TABLE_TARGET',$oldTarget),@('FBE_NEXT_TEST_TABLE_OPERATION',$oldOperation),@('FBE_NEXT_TEST_TABLE_SECOND_OPERATION',$oldSecondOperation))){if($null -eq $state[1]){Remove-Item ("Env:"+$state[0]) -ErrorAction SilentlyContinue}else{Set-Item ("Env:"+$state[0]) $state[1]}} }
        $rows=Import-Csv -LiteralPath $report -Delimiter "`t"
        foreach($operation in $(if($Operation){@($Operation)}else{@('toggle-header','insert-row-above','insert-row-below','delete-row','insert-column-left','insert-column-right','delete-column','make-header','make-normal')})) {
            $before=$rows|Where-Object phase -eq "$operation-before"; $after=$rows|Where-Object phase -eq "$operation-after"; $undo=$rows|Where-Object phase -eq "$operation-undo"; $redo=$rows|Where-Object phase -eq "$operation-redo"
            if(@($before,$after,$undo,$redo).Count -ne 4) { throw "Нет live DOM snapshots для $operation ($($case.Id))." }
            if($SecondOperation -and (@($rows|Where-Object phase -eq "$SecondOperation-second-before").Count -ne 1 -or @($rows|Where-Object phase -eq "$SecondOperation-second-after").Count -ne 1)) { throw "Нет second-operation snapshots для $operation → $SecondOperation ($($case.Id))." }
            $signature={ param($row) "$($row.table_count)/$($row.tr_count)/$($row.td_count)/$($row.th_count)" }
            if(-not $SecondOperation -and (&$signature $undo) -ne (&$signature $before)) { throw "Undo не восстановил live DOM для $operation ($($case.Id))." }
            if(-not $SecondOperation -and (&$signature $redo) -ne (&$signature $after)) { throw "Redo не восстановил live DOM для $operation ($($case.Id))." }
			if($operation -ne 'make-normal' -and (&$signature $after) -eq (&$signature $before) -and $after.grid_signature -eq $before.grid_signature) { throw "Handler $operation не изменил live DOM или semantic grid ($($case.Id))." }
			if(-not $SecondOperation -and $undo.grid_signature -ne $before.grid_signature) { throw "Undo не восстановил logical grid для $operation ($($case.Id))." }
			if(-not $SecondOperation -and $redo.grid_signature -ne $after.grid_signature) { throw "Redo не восстановил logical grid для $operation ($($case.Id))." }
			if($operation -ne 'make-normal' -and $after.grid_signature -eq $before.grid_signature) { throw "Handler $operation не изменил logical grid ($($case.Id))." }
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
