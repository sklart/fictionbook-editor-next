<# .SYNOPSIS Verifies the live handlers cannot remove the final table row or logical column. #>
[CmdletBinding()]
param([string]$FbeExe = (Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe'), [int]$TimeoutSeconds = 30)
$ErrorActionPreference='Stop';$FbeExe=$ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($FbeExe)
if(-not(Test-Path $FbeExe)){throw "Не найден FBE: $FbeExe"}
function Invoke-Fbe([string[]]$Arguments,[string]$Name){$p=Start-Process -FilePath $FbeExe -ArgumentList $Arguments -PassThru;if(-not $p.WaitForExit($TimeoutSeconds*1000)){Stop-Process $p -Force;throw "FBE не завершил $Name"};if($p.ExitCode){throw "FBE вернул $($p.ExitCode): $Name"}}
$mainFrame = Get-Content -Raw -LiteralPath (Join-Path (Resolve-Path (Join-Path $PSScriptRoot '..\..')) 'src\fbe\mainfrm.cpp')
foreach ($contract in @('void CMainFrame::UpdateTableCommandState()', 'const bool canDeleteRow = validCell && grid.rows.size() > 1;', 'const bool canDeleteColumn = validCell && grid.columns > 1;', 'UIEnable(ID_TABLE_DELETE_ROW, canDeleteRow);', 'UIEnable(ID_TABLE_DELETE_COLUMN, canDeleteColumn);')) {
    if ($mainFrame.IndexOf($contract, [System.StringComparison]::Ordinal) -lt 0) { throw "Missing disabled-state contract: $contract" }
}$dir=Join-Path ([IO.Path]::GetTempPath()) ('fbe-table-guard-'+[guid]::NewGuid().ToString('N'));New-Item -ItemType Directory -Path $dir|Out-Null
try{foreach($case in @(@{id='one-row';table='<tr><td>A</td><td>B</td></tr>'},@{id='one-column';table='<tr><td>A</td></tr><tr><td>B</td></tr>'})){
 $fb2=Join-Path $dir ($case.id+'.fb2');$report=Join-Path $dir ($case.id+'.tsv');$text='<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>guard</book-title><lang>en</lang></title-info><document-info><program-used>test</program-used><id>guard-'+$case.id+'</id><version>1.0</version></document-info></description><body><section><table>'+$case.table+'</table></section></body></FictionBook>';Set-Content $fb2 $text -Encoding utf8
 $a=$env:FBE_NEXT_TEST_MODE;$b=$env:FBE_NEXT_TEST_SCENARIO;try{$env:FBE_NEXT_TEST_MODE='1';$env:FBE_NEXT_TEST_SCENARIO='table-delete-guard';Invoke-Fbe @('-b',$report,$fb2) $case.id}finally{if($null -eq $a){Remove-Item Env:FBE_NEXT_TEST_MODE -ErrorAction SilentlyContinue}else{$env:FBE_NEXT_TEST_MODE=$a};if($null -eq $b){Remove-Item Env:FBE_NEXT_TEST_SCENARIO -ErrorAction SilentlyContinue}else{$env:FBE_NEXT_TEST_SCENARIO=$b}}
 $rows = @(Import-Csv $report -Delimiter "`t")
$beforeRow = @($rows | Where-Object { $_.phase -eq 'before' })[0]
$rowAfterRow = @($rows | Where-Object { $_.phase -eq 'row-after' })[0]
$columnAfterRow = @($rows | Where-Object { $_.phase -eq 'column-after' })[0]
if (-not $beforeRow -or -not $rowAfterRow -or -not $columnAfterRow) { throw "Incomplete guard report: $($case.id)" }
$before = $beforeRow.snapshot; $rowAfter = $rowAfterRow.snapshot; $columnAfter = $columnAfterRow.snapshot
if ($case.id -eq 'one-row' -and $rowAfter -ne $before) { throw 'one-row: Delete Row удалил последнюю строку' }
if ($case.id -eq 'one-column' -and $columnAfter -ne $rowAfter) { throw 'one-column: Delete Column удалил последний столбец' }
};Write-Host 'Production final row/column deletion guards passed.'}finally{Remove-Item $dir -Recurse -Force -ErrorAction SilentlyContinue}