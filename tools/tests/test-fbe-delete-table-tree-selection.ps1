[CmdletBinding()]
param([Parameter(Mandatory)][string]$FbeExe, [int]$TimeoutSeconds = 90)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$isolation = New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'delete-table-tree'
$passed = $false
$oldMode = $env:FBE_NEXT_TEST_MODE; $oldScenario = $env:FBE_NEXT_TEST_SCENARIO
try {
    $empty = Join-Path $isolation.Root 'empty-scripts'; New-Item -ItemType Directory -Force $empty | Out-Null
    $null = New-IsolatedFbeSettingsSeed -Isolation $isolation -StartupScriptsDirectory $empty
    $fixture = Join-Path $isolation.Root 'fixture.fb2'; $report = Join-Path $isolation.Root 'tree-result.txt'
    $env:FBE_NEXT_TEST_MODE = '1'; $env:FBE_NEXT_TEST_SCENARIO = 'delete-table-tree-selection'
    foreach($case in @('between-paragraphs','only-table','titled-only-table')) {
        $table = '<table id="target-table"><tr><td id="target-cell">Cell text</td></tr></table>'
        $body = switch($case) { 'between-paragraphs' { '<title><p>Target</p></title><p>Before.</p>' + $table + '<p>After.</p>' } 'titled-only-table' { '<title><p>Target</p></title>' + $table } default { $table } }
        $xml = '<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><nickname>T</nickname></author><book-title>Test</book-title><lang>en</lang></title-info><document-info><author><nickname>T</nickname></author><id>test</id><version>1</version></document-info></description><body><section id="previous"><title><p>Previous</p></title><p>Previous text.</p></section><section id="parent"><title><p>Parent</p></title><section id="target">' + $body + '</section><section id="next"><title><p>Next</p></title><p>Next text.</p></section></section></body></FictionBook>'
        [IO.File]::WriteAllText($fixture, $xml, [Text.UTF8Encoding]::new($false))
        Remove-Item -LiteralPath $report -Force -ErrorAction SilentlyContinue
        $process = Start-Process -FilePath $isolation.Exe -WorkingDirectory $isolation.Root -ArgumentList @('--portable', '-b', $report, $fixture) -PassThru
        $exitCode = Wait-IsolatedFbeProcess -Process $process -Isolation $isolation -Scenario $env:FBE_NEXT_TEST_SCENARIO -Report $report -TimeoutSeconds $TimeoutSeconds
        $content = if(Test-Path -LiteralPath $report) { Get-Content -LiteralPath $report -Raw -Encoding utf8 } else { '<no report>' }
        if($exitCode -ne 0 -or $content -notmatch '(?m)^result=pass\r?$') { throw "Tree selection test $case failed (exit $exitCode):`n$content" }
        foreach($phase in @('before','deleted','undo','redo')) { if($content -notmatch "(?m)^$phase=1;caret=target;tree=target\r?$") { throw "Unexpected tree selection for $case at $phase`:`n$content" } }
        Write-Host "${case}: delete, undo, redo: PASS"
    }
    $passed = $true
} finally {
    $env:FBE_NEXT_TEST_MODE = $oldMode; $env:FBE_NEXT_TEST_SCENARIO = $oldScenario
    Complete-IsolatedFbeRuntime -Isolation $isolation -Passed $passed
}
