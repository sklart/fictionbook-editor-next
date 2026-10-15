[CmdletBinding()]
param([string]$FbeExe = (Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe'), [int]$TimeoutSeconds = 60)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$isolation = New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'settings-errors-nbsp'
$passed = $false
$oldMode, $oldScenario = $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO
try {
    $fixture = Join-Path $isolation.Root 'settings.fb2'
    $report = Join-Path $isolation.Root 'settings-errors-nbsp.txt'
    [IO.File]::WriteAllText($fixture, '<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>settings</book-title><lang>en</lang></title-info><document-info><id>settings-errors-nbsp</id><version>1</version></document-info></description><body><section><p>test</p></section></body></FictionBook>', [Text.UTF8Encoding]::new($false))
    $env:FBE_NEXT_TEST_MODE = '1'; $env:FBE_NEXT_TEST_SCENARIO = 'settings-residual-runtime'
    $process = Start-Process -FilePath $isolation.Exe -WorkingDirectory $isolation.Runtime -ArgumentList @('--portable', '-b', $report, $fixture) -WindowStyle Hidden -PassThru
    $exitCode = Wait-IsolatedFbeProcess -Process $process -Isolation $isolation -Scenario 'settings-residual-runtime' -Report $report -TimeoutSeconds $TimeoutSeconds
    $text = if(Test-Path -LiteralPath $report) { Get-Content -LiteralPath $report -Raw } else { '<no report>' }
    if($exitCode -ne 0) { throw "Settings errors/NBSP failed: exit=$exitCode; $text" }
    foreach($line in 'single_error=1', 'nbsp_rollback=1', 'nbsp_retry=1') {
        if($text -notmatch [regex]::Escape($line)) { throw "Missing ${line}: $text" }
    }
    Write-Host 'Settings errors/NBSP: one preparation error with reason, complete NBSP rollback and successful retry passed.'
    $passed = $true
} finally {
    $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO = $oldMode, $oldScenario
    Complete-IsolatedFbeRuntime -Isolation $isolation -Passed $passed
}
