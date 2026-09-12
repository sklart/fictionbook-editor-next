<#
.SYNOPSIS
Exercises InsertCite and InsertPoem in the live MSHTML editor and verifies
one-step Undo/Redo DOM snapshots plus a save after the final Undo.
#>
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
$directory = Join-Path ([IO.Path]::GetTempPath()) ('fbe-cite-poem-undo-' + [guid]::NewGuid().ToString('N'))

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
        @{ id = 'cite-one'; operation = 'cite'; paragraphs = @('Text') },
		@{ id = 'cite-wrapper-success'; operation = 'cite'; route = 'wrapper'; paragraphs = @('Text') },
        @{ id = 'cite-many'; operation = 'cite'; paragraphs = @('First', 'Second') },
        @{ id = 'cite-sequential'; operation = 'cite'; repeat = $true; paragraphs = @('Text') },
        @{ id = 'cite-epigraph'; operation = 'cite'; target = 'epigraph'; body = '<epigraph><p>Epigraph text</p></epigraph><section><p>Anchor</p></section>' },
		@{ id = 'poem-one'; operation = 'poem'; paragraphs = @('Line') },
		@{ id = 'poem-wrapper-success'; operation = 'poem'; route = 'wrapper'; paragraphs = @('Line') },
        @{ id = 'poem-sequential'; operation = 'poem'; repeat = $true; paragraphs = @('Line') },
        @{ id = 'poem-lines'; operation = 'poem'; paragraphs = @('First line', 'Second line') },
        @{ id = 'poem-stanzas'; operation = 'poem'; paragraphs = @('One', '', 'Two', 'Three') },
        @{ id = 'poem-in-cite'; operation = 'poem'; target = 'cite'; body = '<section><cite><p>Quoted line</p></cite><p>Anchor</p></section>' },
        @{ id = 'poem-caret-text'; operation = 'poem'; selection = 'caret'; expectedPoemText = '0054,0065,0078,0074'; paragraphs = @('Text') },
        # Keep an unselected sibling so both the pre-operation fixture and
        # the restored document are valid FictionBook sections at Save time.
        @{ id = 'poem-caret-empty'; operation = 'poem'; selection = 'caret'; expectedPoemText = '0020'; paragraphs = @('', 'Anchor') },
        @{ id = 'poem-caret-nbsp'; operation = 'poem'; selection = 'caret'; expectedPoemText = '0020'; paragraphs = @([string][char]160, 'Anchor') },
        @{ id = 'poem-selected-empty'; operation = 'poem'; expectRejected = $true; body = '<section><p></p><empty-line/></section>' },
        @{ id = 'poem-selected-spaces'; operation = 'poem'; expectRejected = $true; body = '<section><p>   </p><empty-line/></section>' },
        # MSHTML collapses a selection containing only NBSP to a caret. Keep a
        # real sibling paragraph so the document remains valid when saved.
        @{ id = 'poem-selected-nbsp'; operation = 'poem'; selection = 'caret'; expectedPoemText = '0020'; paragraphs = @([string][char]160, 'Anchor') },
        # MSHTML keeps this range non-collapsed, but does not anchor its start
        # inside the first whitespace-only P; the editor correctly rejects it.
		@{ id = 'poem-selected-whitespace-paragraphs'; operation = 'poem'; expectRejected = $true; paragraphs = @('  ', "`t", '  ') },
		@{ id = 'poem-wrapper-rejected'; operation = 'poem'; route = 'wrapper'; expectRejected = $true; paragraphs = @('  ', "`t", '  ') }
		,@{ id = 'cite-fault-before'; operation = 'cite'; route = 'wrapper'; paragraphs = @('Text'); fault = 'before-mutation'; documentChanged = $false }
		,@{ id = 'cite-fault-after-insert'; operation = 'cite'; route = 'wrapper'; paragraphs = @('Text'); fault = 'after-insert'; documentChanged = $true }
		,@{ id = 'cite-fault-before-selection'; operation = 'cite'; route = 'wrapper'; paragraphs = @('Text'); fault = 'before-selection'; documentChanged = $true }
		,@{ id = 'poem-fault-before'; operation = 'poem'; route = 'wrapper'; paragraphs = @('Line'); fault = 'before-mutation'; documentChanged = $false }
		,@{ id = 'poem-fault-after-insert'; operation = 'poem'; route = 'wrapper'; paragraphs = @('Line'); fault = 'after-insert'; documentChanged = $true }
		,@{ id = 'poem-fault-before-selection'; operation = 'poem'; route = 'wrapper'; paragraphs = @('Line'); fault = 'before-selection'; documentChanged = $true }
    )
    if($CaseId -and -not (@($cases | ForEach-Object { $_.id }) -contains $CaseId)) { throw "Unknown Cite/Poem runtime case: $CaseId" }
    $selectedCases = @($cases | Where-Object { -not $CaseId -or $_.id -eq $CaseId })
    $selected = $selectedCases.Count; $started = 0; $completed = 0; $passed = 0; $failed = 0
    if($selected -eq 0) { throw 'No Cite/Poem runtime cases were selected.' }
    foreach($testCase in $selectedCases) {
		$started++
		try {
		$case = $testCase
        $expectedTarget = if($case.ContainsKey('target')) { $case.target } else { 'section' }
        $expectedSelection = if($case.ContainsKey('selection')) { $case.selection } else { 'selected' }
        $paragraphs = if($case.ContainsKey('paragraphs')) { ($case.paragraphs | ForEach-Object { "<p>$([Security.SecurityElement]::Escape($_))</p>" }) -join '' } else { '' }
        $body = if($case.ContainsKey('body')) { $case.body } else { "<section>$paragraphs</section>" }
        $fixture = Join-Path $directory ($case.id + '.fb2')
        $report = Join-Path $directory ($case.id + '.tsv')
        $trace = Join-Path $directory ($case.id + '.trace.tsv')
        @("<?xml version=`"1.0`" encoding=`"utf-8`"?>", "<FictionBook xmlns=`"http://www.gribuser.ru/xml/fictionbook/2.0`"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>$($case.id)</book-title><lang>en</lang></title-info><document-info><program-used>test</program-used><id>$($case.id)</id><version>1.0</version></document-info></description><body>$body</body></FictionBook>") | Set-Content -LiteralPath $fixture -Encoding utf8
        $beforeFixture = Get-Content -LiteralPath $fixture -Raw
        $oldMode, $oldScenario, $oldOperation, $oldTarget, $oldSelection, $oldRepeat, $oldTrace, $oldTraceCase, $oldRoute, $oldFault = $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO, $env:FBE_NEXT_TEST_STRUCTURE_OPERATION, $env:FBE_NEXT_TEST_STRUCTURE_TARGET, $env:FBE_NEXT_TEST_STRUCTURE_SELECTION_MODE, $env:FBE_NEXT_TEST_STRUCTURE_REPEAT, $env:FBE_NEXT_TEST_STRUCTURE_TRACE, $env:FBE_NEXT_TEST_STRUCTURE_CASE, $env:FBE_NEXT_TEST_STRUCTURE_ROUTE, $env:FBE_NEXT_TEST_CITE_POEM_FAULT
        try {
            $env:FBE_NEXT_TEST_MODE = '1'; $env:FBE_NEXT_TEST_SCENARIO = 'cite-poem-undo'; $env:FBE_NEXT_TEST_STRUCTURE_OPERATION = $case.operation
            $env:FBE_NEXT_TEST_STRUCTURE_TARGET = $expectedTarget
            $env:FBE_NEXT_TEST_STRUCTURE_SELECTION_MODE = $expectedSelection
            $env:FBE_NEXT_TEST_STRUCTURE_REPEAT = if($case.ContainsKey('repeat')) { '1' } else { $null }
            $env:FBE_NEXT_TEST_STRUCTURE_ROUTE = if($case.ContainsKey('route')) { $case.route } else { $null }
			$env:FBE_NEXT_TEST_CITE_POEM_FAULT = if($case.ContainsKey('fault')) { $case.fault } else { $null }
            $env:FBE_NEXT_TEST_STRUCTURE_TRACE = if($case.ContainsKey('route')) { $null } else { $trace }
            $env:FBE_NEXT_TEST_STRUCTURE_CASE = $case.id
            $process = Start-Process -FilePath $FbeExe -ArgumentList @('--portable', '-b', $report, $fixture) -WorkingDirectory (Split-Path $FbeExe) -PassThru
            if(-not $process.WaitForExit($TimeoutSeconds * 1000)) {
                $tail = if(Test-Path -LiteralPath $trace) { (Get-Content -LiteralPath $trace | Select-Object -Last 20) -join [Environment]::NewLine } else { '<trace unavailable>' }
                Stop-Process -Id $process.Id -Force
                throw "FBE timed out for case=$($case.id), trace=$trace. Last phases:`n$tail"
            }
            if($process.ExitCode -ne 0 -and -not $case.ContainsKey('expectRejected')) { throw "FBE failed for $($case.id): exit $($process.ExitCode)." }
        }
        finally {
            $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO, $env:FBE_NEXT_TEST_STRUCTURE_OPERATION, $env:FBE_NEXT_TEST_STRUCTURE_TARGET, $env:FBE_NEXT_TEST_STRUCTURE_SELECTION_MODE, $env:FBE_NEXT_TEST_STRUCTURE_REPEAT, $env:FBE_NEXT_TEST_STRUCTURE_TRACE, $env:FBE_NEXT_TEST_STRUCTURE_CASE, $env:FBE_NEXT_TEST_STRUCTURE_ROUTE, $env:FBE_NEXT_TEST_CITE_POEM_FAULT = $oldMode, $oldScenario, $oldOperation, $oldTarget, $oldSelection, $oldRepeat, $oldTrace, $oldTraceCase, $oldRoute, $oldFault
        }
        $row = Import-Csv -LiteralPath $report -Delimiter "`t"
        if(@($row).Count -ne 1) { throw "Missing live MSHTML report for $($case.id)." }
        $expectedCollapsed = if($expectedSelection -eq 'caret') { '1' } else { '0' }
        if($row.selection_collapsed -ne $expectedCollapsed) { throw "MSHTML collapsed-state mismatch for $($case.id): $($row.selection_collapsed)." }
		if($case.ContainsKey('fault')) {
			if($process.ExitCode -ne 0 -or $row.result -ne 'pass' -or $row.check_status -ne 'applied' -or $row.apply_status -ne 'failed' -or $row.hresult -ne '0x80004005' -or $row.document_changed -ne $(if($case.documentChanged){'1'}else{'0'})) { throw "Cite/Poem fault result contract failed for $($case.id): $($row | ConvertTo-Json -Compress)" }
			$completed++; $passed++; continue
		}
        if($case.ContainsKey('expectRejected')) {
            if($process.ExitCode -eq 0 -or $row.check_allowed -ne '0' -or $row.result -ne 'not-applicable' -or $row.check_status -ne 'not-applicable' -or $row.apply_status -ne 'not-applicable' -or $row.hresult -ne '0x00000000' -or $row.document_changed -ne '0' -or (Get-Content -LiteralPath $fixture -Raw) -ne $beforeFixture) { throw "Expected an unchanged, explicitly not-applicable MSHTML whitespace range for $($case.id)." }
            if($case.ContainsKey('route')) { $completed++; $passed++; continue }
            if(-not (Test-Path -LiteralPath $trace)) { throw "Missing structural trace for rejected $($case.id): $trace" }
            $traceRows = Import-Csv -LiteralPath $trace -Delimiter "`t"
            if(-not (@($traceRows | Where-Object { $_.phase -eq 'preflight-rejected' -and $_.event -eq 'after' }).Count)) { throw "Rejected structural trace is not explained for $($case.id): $trace" }
            $completed++; $passed++; continue
        }
        if(-not $case.ContainsKey('route')) {
            if(-not (Test-Path -LiteralPath $trace)) { throw "Missing structural trace for $($case.id): $trace" }
            $traceRows = Import-Csv -LiteralPath $trace -Delimiter "`t"
            foreach($property in @('phase', 'event', 'hresult')) {
                if(-not ($traceRows[0].PSObject.Properties.Name -contains $property)) { throw "Malformed StructuralTrace TSV for $($case.id): missing $property" }
            }
            $required = @("$($case.operation)-enter", 'preflight-complete', 'undo-begin', 'insert-before', 'undo-end', "$($case.operation)-success")
            foreach($phase in $required) {
                if(-not (@($traceRows | Where-Object { $_.phase -eq $phase -and $_.event -eq 'after' }).Count)) { throw "Incomplete structural trace for $($case.id): missing $phase after ($trace)" }
            }
            if(@($traceRows | Where-Object { $_.event -in @('exception', 'failure') }).Count) { throw "Structural trace recorded a COM failure for $($case.id): $trace" }
        }
        if($case.ContainsKey('expectedPoemText') -and $row.poem_text_utf16 -ne $case.expectedPoemText) { throw "Poem text is wrong for $($case.id): $($row.poem_text_utf16)." }
        if($row.operation -ne $case.operation -or $row.target -ne $expectedTarget -or $row.selection_mode -ne $expectedSelection -or $row.check_allowed -ne '1' -or $row.before_equals_undo -ne '1' -or $row.after_equals_redo -ne '1' -or $row.sequential_cycle -ne '1' -or $row.empty_divs -ne '0' -or $row.empty_paragraphs -ne '0' -or $row.empty_stanzas -ne '0' -or $row.saved -ne '1' -or $row.result -ne 'pass' -or $row.check_status -ne 'applied' -or $row.apply_status -ne 'applied' -or $row.hresult -ne '0x00000000' -or $row.document_changed -ne '1') { throw "Undo/Redo contract failed for $($case.id): $($row | ConvertTo-Json -Compress)" }
        if($case.operation -eq 'cite' -and ([int]$row.after_cites -ne 1 -or [int]$row.after_poems -ne 0)) { throw "Cite structure is wrong for $($case.id)." }
        if($case.operation -eq 'poem' -and ([int]$row.after_poems -ne 1 -or [int]$row.after_stanzas -lt 1)) { throw "Poem structure is wrong for $($case.id)." }
        Assert-Fb2Schema $fixture
        $completed++; $passed++
        } catch {
            $failed++
            Write-Host "Cite/Poem scenario counters: selected=$selected started=$started completed=$completed passed=$passed failed=$failed"
            throw
        }
    }
    Write-Host "Cite/Poem scenario counters: selected=$selected started=$started completed=$completed passed=$passed failed=$failed"
    if($selected -le 0 -or $started -ne $selected -or $completed -ne $selected -or $failed -ne 0) { throw 'Cite/Poem scenario execution accounting failed.' }
    Write-Host 'Production Cite/Poem MSHTML Undo/Redo passed.'
}
finally {
    if($KeepArtifacts) { Write-Host "Artifacts: $directory" }
    else { Remove-Item -LiteralPath $directory -Recurse -Force -ErrorAction SilentlyContinue }
}
