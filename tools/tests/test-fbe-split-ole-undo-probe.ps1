param(
    [string]$FbeExe = (Join-Path (Split-Path $PSScriptRoot -Parent -Parent) 'out\Release\FBE.exe'),
    [int]$TimeoutSeconds = 45,
    [switch]$KeepArtifacts
)

$ErrorActionPreference = 'Stop'
$FbeExe = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($FbeExe)
if(-not (Test-Path -LiteralPath $FbeExe -PathType Leaf)) { throw "FBE.exe was not found: $FbeExe" }
$directory = Join-Path ([IO.Path]::GetTempPath()) ('fbe-split-ole-undo-probe-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $directory -Force | Out-Null
$fixture = Join-Path $directory 'probe.fb2'
@('<?xml version="1.0" encoding="utf-8"?>', '<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>probe</book-title><lang>en</lang></title-info><document-info><program-used>test</program-used><id>probe</id><version>1.0</version></document-info></description><body><section><p>seed</p></section></body></FictionBook>') | Set-Content -LiteralPath $fixture -Encoding utf8
$report = Join-Path $directory 'ole-parent.tsv'
try {
    $oldMode, $oldScenario = $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO
    try {
        $env:FBE_NEXT_TEST_MODE = '1'; $env:FBE_NEXT_TEST_SCENARIO = 'split-ole-undo-probe'
        $process = Start-Process -FilePath $FbeExe -ArgumentList @('--portable', '-b', $report, $fixture) -WorkingDirectory (Split-Path $FbeExe) -PassThru
        if(-not $process.WaitForExit($TimeoutSeconds * 1000)) { Stop-Process -Id $process.Id -Force; throw 'FBE timed out for the OLE Split/Undo probe.' }
        if($process.ExitCode -ne 0) { throw "FBE failed for the OLE Split/Undo probe: exit $($process.ExitCode)." }
    } finally { $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO = $oldMode, $oldScenario }
    $row = @(Import-Csv -LiteralPath $report -Delimiter "`t")
    if($row.Count -ne 1 -or [string]::IsNullOrWhiteSpace($row[0].manager_hr)) { throw 'Malformed OLE Split/Undo probe report.' }
    Copy-Item -LiteralPath $report -Destination (Join-Path $directory 'summary.tsv') -Force
    Write-Host "OLE parent Split/Undo probe completed: $report"
    $row | Select-Object manager_hr,open_hr,close_hr,parent_units,state_before,state_after,state_undo,state_redo,undo_description_before,undo_description_after,undo_description_undo,undo_description_redo,result | Format-List
    if($row[0].result -ne 'pass') { Write-Warning 'OLE parent unit did not prove one-Undo/one-Redo Split composition; this is a diagnostic result, not a skipped assertion.' }
} finally {
    if($KeepArtifacts) { Write-Host "Artifacts: $directory" } else { Remove-Item -LiteralPath $directory -Recurse -Force -ErrorAction SilentlyContinue }
}
