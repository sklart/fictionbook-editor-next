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
    $expected = @('missing-custom-ok', 'missing-builtin-ok', 'missing-builtin-select-none', 'custom-cancel',
        'explicit-colors-automatic-swatches', 'foreground-automatic-preview', 'background-automatic-preview',
        'automatic-settings-body', 'automatic-to-explicit', 'dark-white-background-auto-text',
        'dark-black-text-auto-background', 'light-black-background-auto-text', 'light-white-text-auto-background',
        'explicit-body-colors', 'builtin-background-automatic')
    if($rows.Count -ne $expected.Count) { throw "Expected $($expected.Count) Settings cases, got $($rows.Count)." }
    foreach($name in $expected) {
        $matches = @($rows | Where-Object case -eq $name)
        if($matches.Count -ne 1 -or $matches[0].passed -ne '1') { throw "Settings Editor page failed $name." }
    }
    $alphaReport = Join-Path $isolation.Root 'preview-alpha.tsv'
    $savedMode, $savedScenario, $savedDirectory = $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO, $env:FBE_NEXT_TEST_PREVIEW_DIRECTORY
    try {
        $env:FBE_NEXT_TEST_MODE = '1'
        $env:FBE_NEXT_TEST_SCENARIO = 'settings-editor-alpha-runtime'
        $env:FBE_NEXT_TEST_PREVIEW_DIRECTORY = $isolation.Root
        $process = Start-Process -FilePath $isolation.Exe -WorkingDirectory $isolation.Runtime -ArgumentList @('-b', $alphaReport, '--portable', $fixture) -PassThru
        $exitCode = Wait-IsolatedFbeProcess -Process $process -Isolation $isolation -Scenario 'settings-editor-alpha-runtime' -Report $alphaReport -TimeoutSeconds $TimeoutSeconds
        if($exitCode -ne 0) { throw "Settings Editor alpha preview exited with $exitCode." }
    } finally {
        $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO, $env:FBE_NEXT_TEST_PREVIEW_DIRECTORY = $savedMode, $savedScenario, $savedDirectory
    }
    $alphaRows = @(Import-Csv -LiteralPath $alphaReport -Delimiter "`t")
    if($alphaRows.Count -ne 25) { throw "Expected 24 PNG alpha preview cases plus same-path cache refresh, got $($alphaRows.Count)." }
    foreach($row in $alphaRows) {
        if($row.passed -ne '1') { throw "Settings preview alpha mismatch: $($row | ConvertTo-Json -Compress)" }
    }
    $passed = $true
    Write-Host 'Settings Editor Automatic swatches/BODY and 24 transparent PNG preview cases passed.'
} finally {
    Complete-IsolatedFbeRuntime -Isolation $isolation -Passed $passed
}
