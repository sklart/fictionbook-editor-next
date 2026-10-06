<#
.SYNOPSIS
Runs the production ARGB scaler on a synthetic magenta-key bitmap at all command-toolbar DPIs.
#>
[CmdletBinding()]
param([string]$FbeExe = (Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe'), [int]$TimeoutSeconds = 90, [switch]$KeepArtifacts)
$ErrorActionPreference = 'Stop'
$FbeExe = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($FbeExe)
if (-not (Test-Path -LiteralPath $FbeExe -PathType Leaf)) { throw "Не найден FBE: $FbeExe" }
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$isolation = New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'toolbar-alpha-scale'
$passed = $false
try {
    $fixture = Join-Path $isolation.Root 'alpha-scale.fb2'
    $report = Join-Path $isolation.Root 'alpha-scale.tsv'
    $xml = '<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>alpha</book-title><lang>en</lang></title-info><document-info><program-used>test</program-used><id>alpha-scale-test</id><version>1.0</version></document-info></description><body><section><p>alpha scale</p></section></body></FictionBook>'
    Set-Content -LiteralPath $fixture -Value $xml -Encoding utf8
    $oldMode, $oldScenario = $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO
    try {
        $env:FBE_NEXT_TEST_MODE = '1'; $env:FBE_NEXT_TEST_SCENARIO = 'toolbar-alpha-scale'
        $process = Start-Process -FilePath $isolation.Exe -WorkingDirectory $isolation.Runtime -ArgumentList @('-b', $report, '--portable', $fixture) -PassThru
        $exitCode = Wait-IsolatedFbeProcess -Process $process -Isolation $isolation -Scenario 'toolbar-alpha-scale' -Report $report -TimeoutSeconds $TimeoutSeconds
        if ($exitCode -ne 0 -or -not (Test-Path -LiteralPath $report)) { throw "FBE alpha-scale scenario failed: exit $exitCode." }
    } finally { $env:FBE_NEXT_TEST_MODE=$oldMode; $env:FBE_NEXT_TEST_SCENARIO=$oldScenario }
    $rows = @(Import-Csv -LiteralPath $report -Delimiter "`t")
    $expected = @{ 96 = 24; 120 = 30; 144 = 36; 168 = 42; 192 = 48 }
    if($rows.Count -ne $expected.Count) { throw "Expected $($expected.Count) alpha-scale rows, got $($rows.Count)." }
    foreach($dpi in $expected.Keys) {
        $row = @($rows | Where-Object dpi -eq $dpi)
        if($row.Count -ne 1 -or [int]$row[0].size -ne $expected[$dpi] -or [int]$row[0].transparent_corners -ne 1 -or [int]$row[0].visible_pixels -le 0 -or [int]$row[0].opaque_black_pixels -ne 0 -or [int]$row[0].magenta_pixels -ne 0 -or [int]$row[0].passed -ne 1) { throw "ARGB alpha-scale contract failed at $dpi DPI." }
    }
    $passed = $true
    Write-Host 'FBE toolbar ARGB scaling contract passed.'
} finally {
    Complete-IsolatedFbeRuntime -Isolation $isolation -Passed ($passed -and -not $KeepArtifacts)
}