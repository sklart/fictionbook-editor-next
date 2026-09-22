<# Verifies Phase B XSD and BODY/SOURCE cache cardinality in one FBE process. #>
[CmdletBinding()]
param(
    [string]$FbeExe = (Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe'),
    [int]$TimeoutSeconds = 90
)

$ErrorActionPreference = 'Stop'
$FbeExe = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($FbeExe)
if(-not (Test-Path -LiteralPath $FbeExe -PathType Leaf)) { throw "Не найден FBE: $FbeExe" }
$directory = Join-Path ([IO.Path]::GetTempPath()) ('fbe-phase-b-cache-' + [guid]::NewGuid().ToString('N'))
[void](New-Item -ItemType Directory -Path $directory)
try {
    $fixture = Join-Path $directory 'phase-b-cache.fb2'; $report = Join-Path $directory 'phase-b-cache.txt'
    @('<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>phase b cache</book-title><lang>en</lang></title-info><document-info><id>phase-b-cache</id><version>1.0</version></document-info></description><body><section><p>PHASE_B_CACHE_MARKER</p></section></body></FictionBook>') | Set-Content -LiteralPath $fixture -Encoding utf8
    $oldMode, $oldScenario, $oldTrace = $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO, $env:FBE_NEXT_TRACE
    try {
        $env:FBE_NEXT_TEST_MODE = '1'; $env:FBE_NEXT_TEST_SCENARIO = 'phase-b-cache-runtime'; $env:FBE_NEXT_TRACE = '1'
        $process = Start-Process -FilePath $FbeExe -ArgumentList @('-b', $report, $fixture) -PassThru
        if(-not $process.WaitForExit($TimeoutSeconds * 1000)) { Stop-Process -Id $process.Id -Force; throw 'FBE не завершил Phase B cache scenario.' }
        if($process.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $report)) { $diagnostics = if(Test-Path -LiteralPath $report) { Get-Content -LiteralPath $report -Raw } else { '<report missing>' }; throw "FBE Phase B cache scenario failed: exit $($process.ExitCode).`n$diagnostics" }
    } finally { $env:FBE_NEXT_TEST_MODE=$oldMode; $env:FBE_NEXT_TEST_SCENARIO=$oldScenario; $env:FBE_NEXT_TRACE=$oldTrace }
    $rows = @{}; foreach($line in Get-Content -LiteralPath $report) { $parts = $line -split '=', 2; if($parts.Count -eq 2) { $rows[$parts[0]] = $parts[1] } }
    foreach($key in @('first_validation','second_validation','xsd_second_equals_first','unchanged_second_source','edit_invalidates_source')) { if($rows[$key] -ne '1') { throw "Phase B cache check failed: $key (value '$($rows[$key])')." } }
    Write-Host "Phase B cache runtime passed: xsd_second_schema_loads=$([UInt64]$rows['xsd_after_second'] - [UInt64]$rows['xsd_after_first']), unchanged_second_source_serializations=$([UInt64]$rows['source_after_second'] - [UInt64]$rows['source_after_first'])."
} finally { Remove-Item -LiteralPath $directory -Recurse -Force -ErrorAction SilentlyContinue }
