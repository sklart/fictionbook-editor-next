<# Exercises Settings Editor page no-op OK and Cancel against an isolated FBE process. #>
[CmdletBinding()]
param([string]$FbeExe = (Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe'), [int]$TimeoutSeconds = 90)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$isolation = New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'settings-editor-page'
$passed = $false
try {
    $fixture = Join-Path $isolation.Root 'settings-page.fb2'
    $report = Join-Path $isolation.Root 'settings-page.tsv'
    [IO.File]::WriteAllText($fixture, '<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>Settings page</book-title><lang>en</lang></title-info><document-info><id>settings-page-test</id><version>1.0</version></document-info></description><body><section><p>Settings</p></section></body></FictionBook>', [Text.UTF8Encoding]::new($false))
    $savedMode, $savedScenario = $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO
    try {
        $env:FBE_NEXT_TEST_MODE = '1'
        $env:FBE_NEXT_TEST_SCENARIO = 'settings-editor-page-runtime'
        $process = Start-Process -FilePath $isolation.Exe -WorkingDirectory $isolation.Runtime -ArgumentList @('-b', $report, '--portable', $fixture) -PassThru
        $exitCode = Wait-IsolatedFbeProcess -Process $process -Isolation $isolation -Scenario 'settings-editor-page-runtime' -Report $report -TimeoutSeconds $TimeoutSeconds
        if($exitCode -ne 0) { throw "Settings Editor page scenario exited with $exitCode." }
    } finally {
        $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO = $savedMode, $savedScenario
    }
    $rows = @(Import-Csv -LiteralPath $report -Delimiter "`t")
    $expected = @('missing-custom-ok', 'missing-builtin-ok', 'missing-builtin-select-none', 'custom-cancel')
    if($rows.Count -ne $expected.Count) { throw "Expected $($expected.Count) Settings cases, got $($rows.Count)." }
    foreach($name in $expected) {
        $matches = @($rows | Where-Object case -eq $name)
        if($matches.Count -ne 1 -or $matches[0].passed -ne '1') { throw "Settings Editor page failed $name." }
    }
    $passed = $true
    Write-Host 'Settings Editor page preserves unavailable backgrounds and Automatic colors through no-op OK and Cancel.'
} finally {
    Complete-IsolatedFbeRuntime -Isolation $isolation -Passed $passed
}
