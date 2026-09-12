<# Exercises the production SplitContainer wrapper in the live MSHTML editor. #>
[CmdletBinding()]
param(
    [string]$FbeExe = (Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe'),
    [int]$TimeoutSeconds = 90,
    [Alias('Case')]
    [string]$CaseId,
    [switch]$KeepArtifacts
)

$ErrorActionPreference = 'Stop'
$FbeExe = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($FbeExe)
if(-not (Test-Path -LiteralPath $FbeExe -PathType Leaf)) { throw "FBE.exe was not found: $FbeExe" }
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$schemaPath = Join-Path $root 'runtime\FictionBook.xsd'
$directory = Join-Path ([IO.Path]::GetTempPath()) ('fbe-split-container-' + [guid]::NewGuid().ToString('N'))

function Assert-Fb2Schema([string]$Path) {
    $cache = New-Object -ComObject Msxml2.XMLSchemaCache.6.0
    $cache.add('http://www.gribuser.ru/xml/fictionbook/2.0', $schemaPath)
    $document = New-Object -ComObject Msxml2.DOMDocument.6.0
    $document.async = $false
    if(-not $document.load($Path)) { throw "MSXML could not read ${Path}: $($document.parseError.reason)" }
    $document.schemas = $cache
    if($document.validate().errorCode -ne 0) { throw "Saved FB2 is invalid: $($document.validate().reason)" }
}

try {
    New-Item -ItemType Directory -Path $directory -Force | Out-Null
    $cases = @(
        @{ id = 'split-section-middle'; body = '<section><p>First</p><p>Middle</p><p>Last</p></section>'; position = 'middle' },
        @{ id = 'split-section-start'; body = '<section><p>First</p><p>Last</p></section>'; position = 'start'; rejected = $true },
        @{ id = 'split-section-end'; body = '<section><p>First</p><p>Last</p></section>'; position = 'end'; rejected = $true },
        @{ id = 'split-stanza-middle'; body = '<section><p>Anchor</p><poem><stanza><v>First</v><v>Middle</v></stanza></poem></section>'; position = 'middle'; container = 'stanza' },
        @{ id = 'split-invalid-container'; body = '<epigraph><p>Quoted</p><p>Tail</p></epigraph>'; position = 'middle'; rejected = $true }
    )
    if($CaseId -and -not (@($cases | ForEach-Object { $_.id }) -contains $CaseId)) { throw "Unknown Split runtime case: $CaseId" }
    $selectedCases = @($cases | Where-Object { -not $CaseId -or $_.id -eq $CaseId })
    $selected = $selectedCases.Count; $started = 0; $completed = 0; $passed = 0; $failed = 0
    if($selected -eq 0) { throw 'No Split runtime cases were selected.' }
    foreach($testCase in $selectedCases) {
		$started++
		try {
		$case = $testCase
        $fixture = Join-Path $directory ($case.id + '.fb2')
        $report = Join-Path $directory ($case.id + '.tsv')
        $trace = Join-Path $directory ($case.id + '.trace.tsv')
        @("<?xml version=`"1.0`" encoding=`"utf-8`"?>", "<FictionBook xmlns=`"http://www.gribuser.ru/xml/fictionbook/2.0`"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>$($case.id)</book-title><lang>en</lang></title-info><document-info><program-used>test</program-used><id>$($case.id)</id><version>1.0</version></document-info></description><body>$($case.body)</body></FictionBook>") | Set-Content -LiteralPath $fixture -Encoding utf8
        $oldMode, $oldScenario, $oldPosition, $oldContainer, $oldReject, $oldTrace, $oldTraceCase = $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO, $env:FBE_NEXT_TEST_SPLIT_POSITION, $env:FBE_NEXT_TEST_SPLIT_CONTAINER_CLASS, $env:FBE_NEXT_TEST_SPLIT_EXPECT_REJECT, $env:FBE_NEXT_TEST_STRUCTURE_TRACE, $env:FBE_NEXT_TEST_STRUCTURE_CASE
        try {
            $env:FBE_NEXT_TEST_MODE = '1'; $env:FBE_NEXT_TEST_SCENARIO = 'split-container'; $env:FBE_NEXT_TEST_SPLIT_POSITION = $case.position
            $env:FBE_NEXT_TEST_SPLIT_CONTAINER_CLASS = if($case.ContainsKey('container')) { $case.container } else { $null }
            $env:FBE_NEXT_TEST_SPLIT_EXPECT_REJECT = if($case.ContainsKey('rejected')) { '1' } else { $null }
            $env:FBE_NEXT_TEST_STRUCTURE_TRACE = $trace; $env:FBE_NEXT_TEST_STRUCTURE_CASE = $case.id
            $process = Start-Process -FilePath $FbeExe -ArgumentList @('--portable', '-b', $report, $fixture) -WorkingDirectory (Split-Path $FbeExe) -PassThru
            if(-not $process.WaitForExit($TimeoutSeconds * 1000)) { Stop-Process -Id $process.Id -Force; throw "FBE timed out for $($case.id)." }
            if($process.ExitCode -ne 0) { throw "FBE failed for $($case.id): exit $($process.ExitCode)." }
        } finally {
            $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO, $env:FBE_NEXT_TEST_SPLIT_POSITION, $env:FBE_NEXT_TEST_SPLIT_CONTAINER_CLASS, $env:FBE_NEXT_TEST_SPLIT_EXPECT_REJECT, $env:FBE_NEXT_TEST_STRUCTURE_TRACE, $env:FBE_NEXT_TEST_STRUCTURE_CASE = $oldMode, $oldScenario, $oldPosition, $oldContainer, $oldReject, $oldTrace, $oldTraceCase
        }
        $row = Import-Csv -LiteralPath $report -Delimiter "`t"
        if(@($row).Count -ne 1 -or $row.result -ne 'pass') { throw "Split runtime contract failed for $($case.id): $($row | ConvertTo-Json -Compress)" }
        $requiredProperties = if($case.ContainsKey('rejected')) { @('check_dom_unchanged','check_selection_unchanged','check_dirty_unchanged') } else { @('check_allowed','check_dom_unchanged','check_selection_unchanged','check_dirty_unchanged','changed','before_equals_undo','after_equals_redo','selection_in_new','saved') }
        foreach($property in $requiredProperties) {
            if($row.$property -ne '1') { throw "Split runtime assertion $property failed for $($case.id)." }
        }
        if($case.ContainsKey('rejected') -and $row.check_allowed -ne '0') { throw "Rejected Split case was enabled: $($case.id)." }
        $traceRows = Import-Csv -LiteralPath $trace -Delimiter "`t"
        foreach($property in @('phase', 'event', 'hresult')) { if(-not ($traceRows[0].PSObject.Properties.Name -contains $property)) { throw "Malformed StructuralTrace TSV for $($case.id): missing $property" } }
        if($case.ContainsKey('rejected')) {
            if(@($traceRows | Where-Object { $_.phase -eq 'preflight-complete' -and $_.event -eq 'after' }).Count) { throw "Rejected Split case reached preflight completion: $($case.id)." }
        } else {
            foreach($phase in @('split-enter','preflight-complete','undo-begin','insert-container','undo-end','selection-update','split-success')) {
                if(-not @($traceRows | Where-Object { $_.phase -eq $phase -and $_.event -eq 'after' }).Count) { throw "Incomplete Split trace for $($case.id): $phase" }
            }
            if(@($traceRows | Where-Object { $_.event -in @('exception','failure') }).Count) { throw "Split trace has a failure for $($case.id)." }
            Assert-Fb2Schema $fixture
        }
        $completed++; $passed++
        } catch {
            $failed++
            Write-Host "Split scenario counters: selected=$selected started=$started completed=$completed passed=$passed failed=$failed"
            throw
        }
    }
    Write-Host "Split scenario counters: selected=$selected started=$started completed=$completed passed=$passed failed=$failed"
    if($selected -le 0 -or $started -ne $selected -or $completed -ne $selected -or $failed -ne 0) { throw 'Split scenario execution accounting failed.' }
    Write-Host 'Production SplitContainer MSHTML Undo/Redo passed.'
} finally {
    if($KeepArtifacts) { Write-Host "Artifacts: $directory" }
    else { Remove-Item -LiteralPath $directory -Recurse -Force -ErrorAction SilentlyContinue }
}
