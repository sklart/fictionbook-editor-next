param(
    [string]$FbeExe = (Join-Path (Split-Path $PSScriptRoot -Parent -Parent) 'out\Release\FBE.exe'),
    [int]$TimeoutSeconds = 45,
    [switch]$KeepArtifacts
)

$ErrorActionPreference = 'Stop'
$FbeExe = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($FbeExe)
if(-not (Test-Path -LiteralPath $FbeExe -PathType Leaf)) { throw "FBE.exe was not found: $FbeExe" }
$directory = Join-Path ([IO.Path]::GetTempPath()) ('fbe-split-undo-probe-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $directory -Force | Out-Null
$fixture = Join-Path $directory 'probe.fb2'
@('<?xml version="1.0" encoding="utf-8"?>', '<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>probe</book-title><lang>en</lang></title-info><document-info><program-used>test</program-used><id>probe</id><version>1.0</version></document-info></description><body><section><p>seed</p></section></body></FictionBook>') | Set-Content -LiteralPath $fixture -Encoding utf8
$variants = 'insert-adjacent','insert-before','append-child','pastehtml-content','pastehtml-root','markup-after-end','markup-before-begin','markup-parse-copy','detached-subtree','whole-innerhtml'
$rows = @()
try {
    foreach($variant in $variants) {
        $report = Join-Path $directory ($variant + '.tsv')
        $oldMode, $oldScenario, $oldVariant = $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO, $env:FBE_NEXT_TEST_SPLIT_UNDO_PROBE_VARIANT
        try {
            $env:FBE_NEXT_TEST_MODE = '1'; $env:FBE_NEXT_TEST_SCENARIO = 'split-undo-probe'; $env:FBE_NEXT_TEST_SPLIT_UNDO_PROBE_VARIANT = $variant
            $process = Start-Process -FilePath $FbeExe -ArgumentList @('--portable', '-b', $report, $fixture) -WorkingDirectory (Split-Path $FbeExe) -PassThru
            if(-not $process.WaitForExit($TimeoutSeconds * 1000)) { Stop-Process -Id $process.Id -Force; throw "FBE timed out for probe $variant." }
            if($process.ExitCode -ne 0) { throw "FBE failed for probe ${variant}: exit $($process.ExitCode)." }
        } finally { $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO, $env:FBE_NEXT_TEST_SPLIT_UNDO_PROBE_VARIANT = $oldMode, $oldScenario, $oldVariant }
        $row = @(Import-Csv -LiteralPath $report -Delimiter "`t")
        if($row.Count -ne 1 -or $row[0].variant -ne $variant) { throw "Malformed split probe report for $variant." }
        $rows += $row[0]
    }
    $summary = Join-Path $directory 'summary.tsv'; $rows | Export-Csv -LiteralPath $summary -Delimiter "`t" -NoTypeInformation
    if($rows.Count -ne $variants.Count) { throw 'Split probe did not report every requested MSHTML variant.' }
    Write-Host "Split/Undo MSHTML probe completed: $summary"
    $rows | Select-Object variant,hresult,undos_required,result | Format-Table -AutoSize
} finally {
    if($KeepArtifacts) { Write-Host "Artifacts: $directory" } else { Remove-Item -LiteralPath $directory -Recurse -Force -ErrorAction SilentlyContinue }
}
