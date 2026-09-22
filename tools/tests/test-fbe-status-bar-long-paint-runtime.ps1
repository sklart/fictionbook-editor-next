<# Exercises the dark status bar painter with boundary-length text in an isolated FBE profile. #>
[CmdletBinding()]
param([string]$FbeExe = (Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe'), [int]$TimeoutSeconds = 90)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$isolation = New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'status-paint'
$passed = $false
try {
    $fixture = Join-Path $isolation.Root 'status.fb2'
    $report = Join-Path $isolation.Root 'status.tsv'
    [IO.File]::WriteAllText($fixture, '<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>status</book-title><lang>en</lang></title-info><document-info><id>status-paint-test</id><version>1.0</version></document-info></description><body><section><p>status</p></section></body></FictionBook>', [Text.UTF8Encoding]::new($false))
    $savedMode, $savedScenario = $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO
    try {
        $env:FBE_NEXT_TEST_MODE = '1'
        $env:FBE_NEXT_TEST_SCENARIO = 'status-bar-long-paint'
        $process = Start-Process -FilePath $isolation.Exe -WorkingDirectory $isolation.Runtime -ArgumentList @('-b', $report, '--portable', $fixture) -PassThru
        $exitCode = Wait-IsolatedFbeProcess -Process $process -Isolation $isolation -Scenario 'status-bar-long-paint' -Report $report -TimeoutSeconds $TimeoutSeconds
        if ($exitCode -ne 0) { throw "FBE status-paint scenario exited with $exitCode." }
    } finally {
        $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO = $savedMode, $savedScenario
    }
    if (-not (Test-Path -LiteralPath $report)) { throw 'Status-paint scenario produced no report.' }
    $rows = @(Import-Csv -LiteralPath $report -Delimiter "`t")
    foreach ($length in @(0, 1023, 1024, 1500, 10000)) {
        $matching = @($rows | Where-Object { $_.length -eq [string]$length })
        if ($matching.Count -ne 1 -or $matching[0].set -ne '1' -or $matching[0].roundtrip -ne '1' -or $matching[0].painted -ne '1') {
            throw "Status pane failed at length $length`: $($matching | ConvertTo-Json -Compress)"
        }
    }
    $owner = @($rows | Where-Object length -eq 'owner')
    if ($owner.Count -ne 1 -or $owner[0].set -ne '1' -or $owner[0].painted -ne '1' -or $owner[0].owner_draw -ne '1') {
        throw "Owner-drawn status pane failed: $($owner | ConvertTo-Json -Compress)"
    }
    $passed = $true
    Write-Host 'Dark status bar boundary-length and owner-draw painting passed.'
} finally {
    Complete-IsolatedFbeRuntime -Isolation $isolation -Passed $passed
}
