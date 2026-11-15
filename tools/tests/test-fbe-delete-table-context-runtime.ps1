[CmdletBinding()]
param([Parameter(Mandatory)][string]$FbeExe, [int]$TimeoutSeconds = 90)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$iso = New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'delete-table-context'
$passed = $false; $oldMode = $env:FBE_NEXT_TEST_MODE; $oldScenario = $env:FBE_NEXT_TEST_SCENARIO
try {
    $empty = Join-Path $iso.Root 'empty-scripts'; New-Item -ItemType Directory -Force $empty | Out-Null
    $null = New-IsolatedFbeSettingsSeed -Isolation $iso -StartupScriptsDirectory $empty
    $fixture = Join-Path $iso.Root 'fixture.fb2'; $report = Join-Path $iso.Root 'context-result.txt'
    $xml = '<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><nickname>T</nickname></author><book-title>Test</book-title><lang>en</lang></title-info><document-info><author><nickname>T</nickname></author><id>test</id><version>1</version></document-info></description><body><section id="section"><p id="outside">Before</p><table id="table-a"><tr><td id="a0"><strong>First</strong></td><td id="a1">Second</td></tr></table><p>Between</p><table id="table-b"><tr><td id="b0">Third</td></tr></table><p>After</p></section></body></FictionBook>'
    [IO.File]::WriteAllText($fixture, $xml, [Text.UTF8Encoding]::new($false))
    $env:FBE_NEXT_TEST_MODE = '1'; $env:FBE_NEXT_TEST_SCENARIO = 'delete-table-context-target'
    $process = Start-Process -FilePath $iso.Exe -WorkingDirectory $iso.Root -ArgumentList @('--portable', '-b', $report, $fixture) -PassThru
    $exitCode = Wait-IsolatedFbeProcess -Process $process -Isolation $iso -Scenario $env:FBE_NEXT_TEST_SCENARIO -Report $report -TimeoutSeconds $TimeoutSeconds
    $content = if(Test-Path -LiteralPath $report) { Get-Content -LiteralPath $report -Raw -Encoding utf8 } else { '<no report>' }
    if($exitCode -ne 0 -or $content -notmatch '(?m)^result=pass\r?$' -or $content -notmatch '(?m)^selected-cells=2\r?$') { throw "Context delete failed (exit $exitCode):`n$content" }
    $passed = $true; Write-Host "Context menu target and Undo/Redo passed.`n$content"
} finally {
    $env:FBE_NEXT_TEST_MODE = $oldMode; $env:FBE_NEXT_TEST_SCENARIO = $oldScenario
    Complete-IsolatedFbeRuntime -Isolation $iso -Passed $passed
}
