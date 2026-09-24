<# Exercises FBE-owned dark message boxes through a real isolated Win32 process. #>
[CmdletBinding()]
param([string]$FbeExe = (Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe'), [int]$TimeoutSeconds = 150)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$isolation = New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'themed-message'
$passed = $false
try {
    $fixture = Join-Path $isolation.Root 'dialog.fb2'
    $report = Join-Path $isolation.Root 'dialog.tsv'
    [IO.File]::WriteAllText($fixture, '<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>dialog</book-title><lang>en</lang></title-info><document-info><id>themed-dialog-test</id><version>1.0</version></document-info></description><body><section><p>dialog</p></section></body></FictionBook>', [Text.UTF8Encoding]::new($false))
    $savedMode, $savedScenario = $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO
    try {
        $env:FBE_NEXT_TEST_MODE = '1'
        $env:FBE_NEXT_TEST_SCENARIO = 'themed-message-behavior'
        $process = Start-Process -FilePath $isolation.Exe -WorkingDirectory $isolation.Runtime -ArgumentList @('-b', $report, '--portable', $fixture) -PassThru
        $exitCode = Wait-IsolatedFbeProcess -Process $process -Isolation $isolation -Scenario 'themed-message-behavior' -Report $report -TimeoutSeconds $TimeoutSeconds
        if ($exitCode -ne 0) { throw "FBE themed-message scenario exited with $exitCode." }
    } finally {
        $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO = $savedMode, $savedScenario
    }
    if (-not (Test-Path -LiteralPath $report)) { throw 'Themed-message scenario produced no report.' }
    $rows = @(Import-Csv -LiteralPath $report -Delimiter "`t")
    $expected = @('forged-command', 'yesno-escape', 'focused-enter', 'default-second', 'close-cancel', 'escape-cancel', 'long-warning', 'localized-mnemonic', 'arrow-space', 'long-keyboard-scroll')
    if ($rows.Count -ne $expected.Count) { throw "Expected $($expected.Count) dialog cases, got $($rows.Count)." }
    foreach ($name in $expected) {
        $matching = @($rows | Where-Object case -eq $name)
        if ($matching.Count -ne 1) { throw "Missing or duplicate dialog case: $name" }
        foreach ($field in @('result', 'window', 'geometry', 'content', 'keyboard', 'dpi_geometry', 'dpi_font')) {
            if ($matching[0].$field -ne '1') { throw "Themed dialog $name failed $field`: $($matching[0] | ConvertTo-Json -Compress)" }
        }
    }
    $parityReport = Join-Path $isolation.Root 'parity.tsv'
    $savedMode, $savedScenario = $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO
    try {
        $env:FBE_NEXT_TEST_MODE = '1'
        $env:FBE_NEXT_TEST_SCENARIO = 'themed-message-native-parity'
        $process = Start-Process -FilePath $isolation.Exe -WorkingDirectory $isolation.Runtime -ArgumentList @('-b', $parityReport, '--portable', $fixture) -PassThru
        $exitCode = Wait-IsolatedFbeProcess -Process $process -Isolation $isolation -Scenario 'themed-message-native-parity' -Report $parityReport -TimeoutSeconds $TimeoutSeconds
        if ($exitCode -ne 0) { throw "FBE native/themed parity scenario exited with $exitCode." }
    } finally {
        $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO = $savedMode, $savedScenario
    }
    $parityRows = @(Import-Csv -LiteralPath $parityReport -Delimiter "`t")
    if ($parityRows.Count -ne 13) { throw "Expected 13 native/themed button comparisons, got $($parityRows.Count)." }
    foreach ($row in $parityRows) {
        if ($row.buttons_found -ne '1' -or $row.parity -ne '1' -or $row.native -ne $row.themed -or $row.native -ne $row.button) {
            throw "Native/themed dialog result mismatch: $($row | ConvertTo-Json -Compress)"
        }
    }
    $quitReport = Join-Path $isolation.Root 'quit.tsv'
    $savedMode, $savedScenario = $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO
    try {
        $env:FBE_NEXT_TEST_MODE = '1'
        $env:FBE_NEXT_TEST_SCENARIO = 'themed-message-quit'
        $process = Start-Process -FilePath $isolation.Exe -WorkingDirectory $isolation.Runtime -ArgumentList @('-b', $quitReport, '--portable', $fixture) -PassThru
        $exitCode = Wait-IsolatedFbeProcess -Process $process -Isolation $isolation -Scenario 'themed-message-quit' -Report $quitReport -TimeoutSeconds $TimeoutSeconds
        if ($exitCode -ne 73) { throw "FBE did not preserve WM_QUIT code 73: exit $exitCode." }
    } finally {
        $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO = $savedMode, $savedScenario
    }
    $quit = @{}
    foreach ($line in Get-Content -LiteralPath $quitReport) {
        $parts = $line -split "`t"
        if ($parts.Count -eq 2) { $quit[$parts[0]] = $parts[1] }
    }
    foreach ($field in @('found', 'posted', 'no_answer', 'owner_enabled', 'destroyed')) {
        if ($quit[$field] -ne '1') { throw "Themed dialog WM_QUIT failed $field`: $($quit | ConvertTo-Json -Compress)" }
    }
    $failureReport = Join-Path $isolation.Root 'failure.tsv'
    $savedMode, $savedScenario = $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO
    try {
        $env:FBE_NEXT_TEST_MODE = '1'
        $env:FBE_NEXT_TEST_SCENARIO = 'themed-message-partial-failure'
        $process = Start-Process -FilePath $isolation.Exe -WorkingDirectory $isolation.Runtime -ArgumentList @('-b', $failureReport, '--portable', $fixture) -PassThru
        $exitCode = Wait-IsolatedFbeProcess -Process $process -Isolation $isolation -Scenario 'themed-message-partial-failure' -Report $failureReport -TimeoutSeconds $TimeoutSeconds
        if ($exitCode -ne 0) { throw "FBE partial-failure scenario exited with $exitCode." }
    } finally {
        $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO = $savedMode, $savedScenario
    }
    $failure = @{}
    foreach ($line in Get-Content -LiteralPath $failureReport) {
        $parts = $line -split "`t"
        if ($parts.Count -eq 2) { $failure[$parts[0]] = $parts[1] }
    }
    foreach ($field in @('hook', 'rejected', 'native', 'answer', 'owner_enabled', 'no_themed_hwnd')) {
        if ($failure[$field] -ne '1') { throw "Themed dialog partial creation failure failed $field`: $($failure | ConvertTo-Json -Compress)" }
    }
    $passed = $true
    Write-Host 'Themed message responses, native button parity, keyboard, command origin, DPI geometry, WM_QUIT, and partial-creation fallback passed.'
} finally {
    Complete-IsolatedFbeRuntime -Isolation $isolation -Passed $passed
}
