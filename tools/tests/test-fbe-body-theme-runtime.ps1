<# Checks actual MSHTML BODY/table colors and document invariants across theme switches. #>
[CmdletBinding()]
param([string]$FbeExe = (Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe'), [int]$TimeoutSeconds = 90)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$isolation = New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'body-theme'
$passed = $false
try {
    $fixture = Join-Path $isolation.Root 'body-theme.fb2'
    $report = Join-Path $isolation.Root 'body-theme.tsv'
    $paragraphs = (1..100 | ForEach-Object { "<p>Theme target paragraph $_.</p>" }) -join ''
    $fb2 = '<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>Body theme</book-title><lang>en</lang></title-info><document-info><id>body-theme-test</id><version>1.0</version></document-info></description><body><section>' + $paragraphs + '<table><tr><th>Heading</th><td>Cell</td></tr></table></section></body></FictionBook>'
    [IO.File]::WriteAllText($fixture, $fb2, [Text.UTF8Encoding]::new($false))
    $before = (Get-FileHash -LiteralPath $fixture -Algorithm SHA256).Hash
    $savedMode, $savedScenario = $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO
    try {
        $env:FBE_NEXT_TEST_MODE = '1'
        $env:FBE_NEXT_TEST_SCENARIO = 'body-theme-runtime'
        $process = Start-Process -FilePath $isolation.Exe -WorkingDirectory $isolation.Runtime -ArgumentList @('-b', $report, '--portable', $fixture) -PassThru
        $exitCode = Wait-IsolatedFbeProcess -Process $process -Isolation $isolation -Scenario 'body-theme-runtime' -Report $report -TimeoutSeconds $TimeoutSeconds
        if($exitCode -ne 0) { throw "BODY theme scenario exited with $exitCode." }
    } finally {
        $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO = $savedMode, $savedScenario
    }
    $rows = @(Import-Csv -LiteralPath $report -Delimiter "`t")
    $expected = @(
        @('light-auto', '0', '4278190080', '4278190080'),
        @('dark-auto', '1', '4278190080', '4278190080'),
        @('dark-ui-light-body', '0', '0', '16777215'),
        @('light-ui-dark-body', '1', '16777215', '1315860'),
        @('light-auto-again', '0', '4278190080', '4278190080'),
        @('dark-explicit-again', '0', '0', '16777215')
    )
    if($rows.Count -ne $expected.Count) { throw "Expected $($expected.Count) BODY phases, got $($rows.Count)." }
    foreach($phase in $expected) {
        $matching = @($rows | Where-Object phase -eq $phase[0])
        if($matching.Count -ne 1) { throw "Missing BODY phase $($phase[0])." }
        $row = $matching[0]
        if($row.table_dark -ne $phase[1] -or $row.configured_fg -ne $phase[2] -or $row.configured_bg -ne $phase[3] -or
           $row.no_important -ne '1' -or $row.state_ok -ne '1') {
            throw "BODY theme mismatch in $($phase[0]): $($row | ConvertTo-Json -Compress)"
        }
        if(-not $row.foreground -or -not $row.background) { throw "BODY colors missing in $($phase[0])." }
    }
    $after = (Get-FileHash -LiteralPath $fixture -Algorithm SHA256).Hash
    if($after -ne $before) { throw 'Theme-only changes modified the FB2 file.' }
    $passed = $true
    Write-Host 'BODY/table palette, Automatic/explicit transitions, document state, selection and scroll invariants passed.'
} finally {
    Complete-IsolatedFbeRuntime -Isolation $isolation -Passed $passed
}
