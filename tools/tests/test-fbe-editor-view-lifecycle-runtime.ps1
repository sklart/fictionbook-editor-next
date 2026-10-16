<# Exercises the real BODY/DESC/SOURCE view lifecycle. #>
[CmdletBinding()]
param([string]$FbeExe = (Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe'), [int]$TimeoutSeconds = 90)

$ErrorActionPreference = 'Stop'
$FbeExe = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($FbeExe)
if(-not (Test-Path -LiteralPath $FbeExe -PathType Leaf)) { throw "FBE not found: $FbeExe" }
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$isolation = New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'editor-view-lifecycle'
$passed = $false
try {
    $fixture = Join-Path $isolation.Root 'view-lifecycle.fb2'; $report = Join-Path $isolation.Root 'view-lifecycle.txt'
    '<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><first-name>View</first-name><last-name>Test</last-name></author><book-title>view lifecycle</book-title><lang>en</lang></title-info><document-info><id>view-lifecycle-test</id><version>1.0</version></document-info></description><body><section><p>EDITOR_VIEW_LIFECYCLE</p></section></body></FictionBook>' | Set-Content -LiteralPath $fixture -Encoding utf8
    $oldMode, $oldScenario = $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO
    try {
        $env:FBE_NEXT_TEST_MODE = '1'; $env:FBE_NEXT_TEST_SCENARIO = 'editor-view-lifecycle-runtime'
        $process = Start-Process -FilePath $isolation.Exe -WorkingDirectory $isolation.Runtime -ArgumentList @('--portable', '-b', $report, $fixture) -PassThru
        if(-not $process.WaitForExit($TimeoutSeconds * 1000)) { Stop-Process -Id $process.Id -Force; throw "FBE did not finish editor view lifecycle scenario. $(Get-IsolatedRuntimeDiagnostics $isolation $report)" }
        if($process.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $report)) { throw "Editor view lifecycle scenario failed: exit $($process.ExitCode)." }
    } finally { $env:FBE_NEXT_TEST_MODE=$oldMode; $env:FBE_NEXT_TEST_SCENARIO=$oldScenario }
    $rows = @{}; foreach($line in Get-Content -LiteralPath $report) { $parts = $line -split '='; if($parts.Count -eq 2) { $rows[$parts[0]] = $parts[1] } }
    foreach($key in @('body_desc','desc_body','body_source','source_body','desc_source','source_desc','source_body_desc','cycles','document','tree_initial','tree_desc_hidden','tree_body_restored','tree_hidden_preference')) { if($rows[$key] -ne '1') { throw "Editor view lifecycle check failed: $key (value '$($rows[$key])')." } }
    $passed = $true
    Write-Host 'FBE editor view lifecycle runtime passed.'
} finally { Complete-IsolatedFbeRuntime -Isolation $isolation -Passed $passed }
