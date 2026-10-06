[CmdletBinding()]
param(
    [string]$FbeExe = (Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe'),
    [int]$TimeoutSeconds = 45
)

$ErrorActionPreference = 'Stop'
$FbeExe = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($FbeExe)
if (-not (Test-Path -LiteralPath $FbeExe -PathType Leaf)) { throw "FBE not found: $FbeExe" }
$root = Join-Path ([IO.Path]::GetTempPath()) ('fbe-fb2-binary-inspection-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $root | Out-Null
try {
    $fixture = Join-Path $root 'binary-inspection.fb2'
    $report = Join-Path $root 'report.txt'
@'
<?xml version="1.0" encoding="utf-8"?>
<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0">
  <description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>Binary inspection</book-title><lang>en</lang></title-info><document-info><id>fb2-binary-inspection</id><version>1.0</version></document-info></description>
  <body><section><p>Binary semantic validation fixture.</p></section></body>
  <binary id="jpeg-mismatch" content-type="image/png">/9j/</binary>
  <binary id="jpeg-alias" content-type="image/jpg">/9j/</binary>
  <binary id="png-missing">iVBORw0KGgo=</binary>
  <binary id="unknown" content-type="application/octet-stream">AQID</binary>
</FictionBook>
'@ | Set-Content -LiteralPath $fixture -Encoding utf8
    $oldMode, $oldScenario = $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO
    try {
        $env:FBE_NEXT_TEST_MODE = '1'; $env:FBE_NEXT_TEST_SCENARIO = 'fb2-binary-inspection-runtime'
        $process = Start-Process -FilePath $FbeExe -ArgumentList '-b',("`"$report`""),("`"$fixture`"") -PassThru
        if (-not $process.WaitForExit($TimeoutSeconds * 1000)) { Stop-Process -Id $process.Id -Force; throw 'FBE binary-inspection runtime smoke timed out.' }
        if ($process.ExitCode -ne 0) { $detail = if (Test-Path $report) { Get-Content $report -Raw } else { '<report missing>' }; throw "FBE binary-inspection runtime smoke failed: $($process.ExitCode); $detail" }
    } finally { $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO = $oldMode, $oldScenario }
    $rows = @{}
    Get-Content -LiteralPath $report | ForEach-Object { $pair = $_ -split '=', 2; if ($pair.Count -eq 2) { $rows[$pair[0]] = $pair[1] } }
    foreach ($key in 'semantic', 'warning', 'unchanged', 'validation') { if ($rows[$key] -ne '1') { throw "FB2 binary-inspection runtime failed: $key=$($rows[$key])" } }
    if ($rows['result'] -ne 'pass') { throw "FB2 binary-inspection runtime result=$($rows['result'])" }
    Write-Host 'FBE FB2 binary inspection runtime passed.'
} finally {
    Remove-Item -LiteralPath $root -Recurse -Force -ErrorAction SilentlyContinue
}