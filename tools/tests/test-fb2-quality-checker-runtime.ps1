[CmdletBinding()]
param(
    [string]$FbeExe = (Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe'),
    [int]$TimeoutSeconds = 45
)

$ErrorActionPreference = 'Stop'
$FbeExe = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($FbeExe)
if (-not (Test-Path -LiteralPath $FbeExe -PathType Leaf)) { throw "FBE not found: $FbeExe" }
$root = Join-Path ([IO.Path]::GetTempPath()) ('fbe-quality-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $root | Out-Null
try {
    $fixture = Join-Path $root 'fixture.fb2'
    $report = Join-Path $root 'report.txt'
@'
<?xml version="1.0" encoding="utf-8"?>
<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0">
  <description><title-info><genre>prose</genre><author><first-name>Test</first-name><last-name>Author</last-name></author><book-title>Quality smoke</book-title><lang>en</lang></title-info><document-info><author><first-name>Test</first-name><last-name>Author</last-name></author><id>quality-smoke</id><version>1.0</version></document-info></description>
  <body><section><p id="quality-para">Unchanged editor document.</p><table id="quality-table"><tr><td>one</td><td>two</td></tr></table></section></body>
  <binary id="quality-image" content-type="image/png">AQID</binary>
</FictionBook>
'@ | Set-Content -LiteralPath $fixture -Encoding utf8
    $fixtureHash = (Get-FileHash -LiteralPath $fixture -Algorithm SHA256).Hash
    $oldMode, $oldScenario, $oldTrace = $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO, $env:FBE_NEXT_TRACE
    try {
        $env:FBE_NEXT_TEST_MODE = '1'; $env:FBE_NEXT_TEST_SCENARIO = 'fb2-quality-checker-runtime'
        $env:FBE_NEXT_TRACE = '1'
        $process = Start-Process -FilePath $FbeExe -ArgumentList '-b',("`"$report`""),("`"$fixture`"") -PassThru
        if (-not $process.WaitForExit($TimeoutSeconds * 1000)) { Stop-Process -Id $process.Id -Force; throw 'FB2 quality runtime timed out.' }
        if ($process.ExitCode -ne 0) { $detail = if (Test-Path $report) { Get-Content $report -Raw } else { '<report missing>' }; throw "FB2 quality runtime failed: $($process.ExitCode); $detail" }
    } finally { $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO, $env:FBE_NEXT_TRACE = $oldMode, $oldScenario, $oldTrace }
    $rows = @{}
    Get-Content -LiteralPath $report | ForEach-Object { $pair = $_ -split '=', 2; if ($pair.Count -eq 2) { $rows[$pair[0]] = $pair[1] } }
    if ((Get-FileHash -LiteralPath $fixture -Algorithm SHA256).Hash -ne $fixtureHash) { throw 'FB2 quality runtime changed the original fixture.' }
    foreach ($key in 'unchanged', 'undo_preserved', 'binary_table_preserved', 'snapshot_current', 'failed_snapshot_isolated', 'subsequent_save', 'links', 'binaries', 'metadata', 'empty', 'malformed', 'xlink_rules', 'note_graph', 'binary_rules', 'structure_rules', 'valid_metadata', 'large_document', 'exact_links', 'exact_id', 'missing_attribute', 'no_stale_jump', 'malformed_end', 'unicode_offset', 'body_to_source', 'unicode_selection', 'locations', 'report_formats') {
        if ($rows[$key] -ne '1') { throw "FB2 quality runtime: $key=$($rows[$key])" }
    }
    if ($rows['result'] -ne 'pass') { throw "FB2 quality runtime result=$($rows['result'])" }
    Write-Host 'FB2 quality checker runtime passed.'
} finally {
    $resolved = [IO.Path]::GetFullPath($root)
    $temp = [IO.Path]::GetFullPath([IO.Path]::GetTempPath())
    if ($resolved.StartsWith($temp, [StringComparison]::OrdinalIgnoreCase)) {
        Remove-Item -LiteralPath $resolved -Recurse -Force -ErrorAction SilentlyContinue
    }
}
