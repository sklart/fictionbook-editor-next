<# Exercises the real BODY/DESC/SOURCE view lifecycle. #>
[CmdletBinding()]
param([string]$FbeExe = (Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe'), [ValidateRange(5,45)][int]$TimeoutSeconds = 25)

$ErrorActionPreference = 'Stop'
$FbeExe = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($FbeExe)
if(-not (Test-Path -LiteralPath $FbeExe -PathType Leaf)) { throw "FBE not found: $FbeExe" }
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$isolation = New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'editor-view-lifecycle'
$passed = $false
$evidence = Join-Path (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path ('build/editor-view-lifecycle-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $evidence | Out-Null
try {
    $null = New-IsolatedFbeSettingsSeed -Isolation $isolation
    $fixture = Join-Path $isolation.Root 'view-lifecycle.fb2'; $report = Join-Path $isolation.Root 'view-lifecycle.txt'
    '<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><first-name>View</first-name><last-name>Test</last-name></author><book-title>view lifecycle</book-title><lang>en</lang></title-info><document-info><id>view-lifecycle-test</id><version>1.0</version></document-info></description><body><section><p>EDITOR_VIEW_LIFECYCLE</p></section></body></FictionBook>' | Set-Content -LiteralPath $fixture -Encoding utf8
    $oldMode, $oldScenario, $oldBreadcrumb = $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO, $env:FBE_NEXT_TEST_STARTUP_BREADCRUMB
    try {
        $env:FBE_NEXT_TEST_MODE = '1'; $env:FBE_NEXT_TEST_SCENARIO = 'editor-view-lifecycle-runtime'
        $env:FBE_NEXT_TEST_STARTUP_BREADCRUMB = Join-Path $evidence 'startup.txt'
        $process = Start-Process -FilePath $isolation.Exe -WorkingDirectory $isolation.Runtime -ArgumentList ('--portable -b "{0}" "{1}"' -f $report, $fixture) -WindowStyle Hidden -PassThru
        $exitCode = Wait-IsolatedFbeProcess -Process $process -Isolation $isolation -Scenario 'editor-view-lifecycle-runtime' -Report $report -TimeoutSeconds $TimeoutSeconds
        if(Test-Path -LiteralPath $report) { Copy-Item -LiteralPath $report -Destination $evidence }
        Get-FileHash -LiteralPath $FbeExe -Algorithm SHA256 | Format-List | Out-File (Join-Path $evidence 'executable-sha256.txt')
        if($exitCode -ne 0 -or -not (Test-Path -LiteralPath $report)) { throw "Editor view lifecycle scenario failed: exit $exitCode." }
    } finally { $env:FBE_NEXT_TEST_MODE=$oldMode; $env:FBE_NEXT_TEST_SCENARIO=$oldScenario; $env:FBE_NEXT_TEST_STARTUP_BREADCRUMB=$oldBreadcrumb }
    $rows = @{}; foreach($line in Get-Content -LiteralPath $report) { $parts = $line -split '='; if($parts.Count -eq 2) { $rows[$parts[0]] = $parts[1] } }
    foreach($key in @('hidden_container','body_desc','desc_body','body_source','source_body','desc_source','source_desc','source_body_desc','source_desc_body','cycles','document','tree_initial','tree_desc_hidden','tree_body_restored','tree_hidden_preference')) { if($rows[$key] -ne '1') { throw "Editor view lifecycle check failed: $key (value '$($rows[$key])')." } }
    $passed = $true
    Write-Host "FBE editor view lifecycle runtime passed. Evidence: $evidence"
} finally { Complete-IsolatedFbeRuntime -Isolation $isolation -Passed $passed }
