<# Exercises the contextual attribute UI through an unattended FBE process. #>
[CmdletBinding()]
param([string]$FbeExe = (Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe'), [int]$TimeoutSeconds = 90)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$isolation = New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'context-bars'
$passed = $false
try {
    $fixture = Join-Path $isolation.Root 'context.fb2'; $report = Join-Path $isolation.Root 'context.tsv'
    '<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>context</book-title><lang>en</lang></title-info><document-info><id>context-test</id><version>1.0</version></document-info></description><body><section><p>context</p></section></body></FictionBook>' | Set-Content -LiteralPath $fixture -Encoding utf8
    $oldMode, $oldScenario = $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO
    try {
        $env:FBE_NEXT_TEST_MODE = '1'; $env:FBE_NEXT_TEST_SCENARIO = 'context-attribute-bars-runtime'
        $process = Start-Process -FilePath $isolation.Exe -WorkingDirectory $isolation.Runtime -ArgumentList @('--portable', '-b', $report, $fixture) -PassThru
        $exitCode = Wait-IsolatedFbeProcess -Process $process -Isolation $isolation -Scenario 'context-attribute-bars-runtime' -Report $report -TimeoutSeconds $TimeoutSeconds
        if($exitCode -ne 0 -or -not (Test-Path -LiteralPath $report)) { throw "FBE context attribute scenario failed: exit $exitCode." }
    } finally { $env:FBE_NEXT_TEST_MODE=$oldMode; $env:FBE_NEXT_TEST_SCENARIO=$oldScenario }
    $rows = @{}; foreach($line in Get-Content -LiteralPath $report) { $parts = $line -split "`t"; if($parts.Count -eq 2) { $rows[$parts[0]] = $parts[1] } }
    foreach($key in @('controls', 'ids', 'catalogs', 'state', 'availability', 'layout', 'href_caret', 'href_catalog_selection')) { if($rows[$key] -ne '1') { throw "Context attribute runtime check failed: $key (value '$($rows[$key])')." } }
    $passed = $true
    Write-Host 'FBE context attribute bars runtime passed.'
} finally { Complete-IsolatedFbeRuntime -Isolation $isolation -Passed $passed }
