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
  <body><section><p>Unchanged editor document.</p></section></body>
</FictionBook>
'@ | Set-Content -LiteralPath $fixture -Encoding utf8
    $oldMode, $oldScenario = $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO
    try {
        $env:FBE_NEXT_TEST_MODE = '1'; $env:FBE_NEXT_TEST_SCENARIO = 'fb2-quality-checker-runtime'
        $process = Start-Process -FilePath $FbeExe -ArgumentList '-b',("`"$report`""),("`"$fixture`"") -PassThru
        if (-not $process.WaitForExit($TimeoutSeconds * 1000)) { Stop-Process -Id $process.Id -Force; throw 'FB2 quality runtime timed out.' }
        if ($process.ExitCode -ne 0) { $detail = if (Test-Path $report) { Get-Content $report -Raw } else { '<report missing>' }; throw "FB2 quality runtime failed: $($process.ExitCode); $detail" }
    } finally { $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO = $oldMode, $oldScenario }
    $rows = @{}
    Get-Content -LiteralPath $report | ForEach-Object { $pair = $_ -split '=', 2; if ($pair.Count -eq 2) { $rows[$pair[0]] = $pair[1] } }
    foreach ($key in 'unchanged', 'links', 'binaries', 'metadata', 'empty', 'malformed') {
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
