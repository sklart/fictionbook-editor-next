<# Runs the native byte-span and SHA-256 parity probe in the current FBE build. #>
[CmdletBinding()]
param([string]$FbeExe, [int]$TimeoutSeconds = 45)

$ErrorActionPreference = 'Stop'
if(-not $FbeExe) { $FbeExe = Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe' }
$FbeExe = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($FbeExe)
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$isolation = New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'script-card-block'
$passed = $false
try {
    $fixture = Join-Path $isolation.Root 'card-block.fb2'
    $report = Join-Path $isolation.Root 'card-block.txt'
    '<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>Card</book-title><lang>en</lang></title-info><document-info><id>card-block-smoke</id><version>1</version></document-info></description><body><section><p>Card</p></section></body></FictionBook>' |
        Set-Content -LiteralPath $fixture -Encoding utf8
    $oldMode, $oldScenario = $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO
    try {
        $env:FBE_NEXT_TEST_MODE = '1'
        $env:FBE_NEXT_TEST_SCENARIO = 'script-card-block-runtime'
        $process = Start-Process -FilePath $isolation.Exe -WorkingDirectory $isolation.Runtime -ArgumentList @('-b', $report, '--portable', $fixture) -PassThru
        $exitCode = Wait-IsolatedFbeProcess -Process $process -Isolation $isolation -Scenario 'script-card-block-runtime' -Report $report -TimeoutSeconds $TimeoutSeconds
    } finally {
        $env:FBE_NEXT_TEST_MODE = $oldMode
        $env:FBE_NEXT_TEST_SCENARIO = $oldScenario
    }
    $text = if(Test-Path -LiteralPath $report) { Get-Content -LiteralPath $report -Raw } else { '<missing>' }
    if($exitCode -ne 0) { throw "Native card block probe exited $exitCode`: $text" }
    foreach($field in 'ascii-span','ascii-hash','plain-hash','utf16-hash','malformed',
        'parsed-current','parsed-changed','parsed-unchecked','parsed-invalid','parsed-missing') {
        if($text -notmatch "(?m)^$field=1$") { throw "Native card block probe failed $field`: $text" }
    }
    if($text -notmatch '(?m)^result=pass$') { throw "Native card block probe failed: $text" }
    $passed = $true
    Write-Host 'Native script-card span and SHA-256 parity passed.'
} finally {
    Complete-IsolatedFbeRuntime -Isolation $isolation -Passed $passed
}
