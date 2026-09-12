<#
.SYNOPSIS
Exercises InsertCite and InsertPoem in the live MSHTML editor and verifies
one-step Undo/Redo DOM snapshots plus a save after the final Undo.
#>
[CmdletBinding()]
param(
    [string]$FbeExe = (Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe'),
    [int]$TimeoutSeconds = 90,
    [string]$Case,
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
        @{ id = 'cite-many'; operation = 'cite'; paragraphs = @('First', 'Second') },
        @{ id = 'cite-sequential'; operation = 'cite'; repeat = $true; paragraphs = @('Text') },
        @{ id = 'cite-epigraph'; operation = 'cite'; target = 'epigraph'; body = '<epigraph><p>Epigraph text</p></epigraph><section><p>Anchor</p></section>' },
        @{ id = 'poem-one'; operation = 'poem'; paragraphs = @('Line') },
        @{ id = 'poem-sequential'; operation = 'poem'; repeat = $true; paragraphs = @('Line') },
        @{ id = 'poem-lines'; operation = 'poem'; paragraphs = @('First line', 'Second line') },
        @{ id = 'poem-stanzas'; operation = 'poem'; paragraphs = @('One', '', 'Two', 'Three') },
        @{ id = 'poem-in-cite'; operation = 'poem'; target = 'cite'; body = '<section><cite><p>Quoted line</p></cite><p>Anchor</p></section>' },
        @{ id = 'poem-caret-text'; operation = 'poem'; selection = 'caret'; expectedPoemText = '0054,0065,0078,0074'; paragraphs = @('Text') },
        @{ id = 'poem-caret-empty'; operation = 'poem'; selection = 'caret'; expectedPoemText = '0020'; paragraphs = @('') },
        @{ id = 'poem-caret-nbsp'; operation = 'poem'; selection = 'caret'; expectedPoemText = '0020'; paragraphs = @([string][char]160) },
        @{ id = 'poem-selected-empty'; operation = 'poem'; paragraphs = @('') },
        @{ id = 'poem-selected-spaces'; operation = 'poem'; paragraphs = @('   ') },
        @{ id = 'poem-selected-nbsp'; operation = 'poem'; paragraphs = @([string][char]160) },
        # MSHTML keeps this range non-collapsed, but does not anchor its start
        # inside the first whitespace-only P; the editor correctly rejects it.
        @{ id = 'poem-selected-whitespace-paragraphs'; operation = 'poem'; expectRejected = $true; paragraphs = @('  ', "`t", '  ') }
    )
    if($Case -and -not (@($cases | ForEach-Object { $_.id }) -contains $Case)) { throw "Unknown Cite/Poem runtime case: $Case" }
    foreach($case in $cases) {
		if($Case -and $case.id -ne $Case) { continue }
        $expectedTarget = if($case.ContainsKey('target')) { $case.target } else { 'section' }
        $expectedSelection = if($case.ContainsKey('selection')) { $case.selection } else { 'selected' }
        $paragraphs = if($case.ContainsKey('paragraphs')) { ($case.paragraphs | ForEach-Object { "<p>$([Security.SecurityElement]::Escape($_))</p>" }) -join '' } else { '' }
        $body = if($case.ContainsKey('body')) { $case.body } else { "<section>$paragraphs</section>" }
        $fixture = Join-Path $directory ($case.id + '.fb2')
        $report = Join-Path $directory ($case.id + '.tsv')
        @("<?xml version=`"1.0`" encoding=`"utf-8`"?>", "<FictionBook xmlns=`"http://www.gribuser.ru/xml/fictionbook/2.0`"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>$($case.id)</book-title><lang>en</lang></title-info><document-info><program-used>test</program-used><id>$($case.id)</id><version>1.0</version></document-info></description><body>$body</body></FictionBook>") | Set-Content -LiteralPath $fixture -Encoding utf8
        $oldMode, $oldScenario, $oldOperation, $oldTarget, $oldSelection, $oldRepeat = $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO, $env:FBE_NEXT_TEST_STRUCTURE_OPERATION, $env:FBE_NEXT_TEST_STRUCTURE_TARGET, $env:FBE_NEXT_TEST_STRUCTURE_SELECTION_MODE, $env:FBE_NEXT_TEST_STRUCTURE_REPEAT
        try {
            $env:FBE_NEXT_TEST_MODE = '1'; $env:FBE_NEXT_TEST_SCENARIO = 'cite-poem-undo'; $env:FBE_NEXT_TEST_STRUCTURE_OPERATION = $case.operation
            $env:FBE_NEXT_TEST_STRUCTURE_TARGET = $expectedTarget
            $env:FBE_NEXT_TEST_STRUCTURE_SELECTION_MODE = $expectedSelection
            $env:FBE_NEXT_TEST_STRUCTURE_REPEAT = if($case.ContainsKey('repeat')) { '1' } else { $null }
            $process = Start-Process -FilePath $FbeExe -ArgumentList @('-b', $report, $fixture) -PassThru
            if(-not $process.WaitForExit($TimeoutSeconds * 1000)) { Stop-Process -Id $process.Id -Force; throw "FBE timed out for $($case.id)." }
            if($process.ExitCode -ne 0 -and -not $case.ContainsKey('expectRejected')) { throw "FBE failed for $($case.id): exit $($process.ExitCode)." }
        }
        finally {
            $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO, $env:FBE_NEXT_TEST_STRUCTURE_OPERATION, $env:FBE_NEXT_TEST_STRUCTURE_TARGET, $env:FBE_NEXT_TEST_STRUCTURE_SELECTION_MODE, $env:FBE_NEXT_TEST_STRUCTURE_REPEAT = $oldMode, $oldScenario, $oldOperation, $oldTarget, $oldSelection, $oldRepeat
        }
        $row = Import-Csv -LiteralPath $report -Delimiter "`t"
        if(@($row).Count -ne 1) { throw "Missing live MSHTML report for $($case.id)." }
        $expectedCollapsed = if($expectedSelection -eq 'caret') { '1' } else { '0' }
        if($row.selection_collapsed -ne $expectedCollapsed) { throw "MSHTML collapsed-state mismatch for $($case.id): $($row.selection_collapsed)." }
        if($case.ContainsKey('expectRejected')) {
            if($process.ExitCode -eq 0 -or $row.check_allowed -ne '0' -or $row.result -ne 'operation-failed') { throw "Expected the unanchorable MSHTML whitespace range to be rejected for $($case.id)." }
            continue
        }
        if($case.ContainsKey('expectedPoemText') -and $row.poem_text_utf16 -ne $case.expectedPoemText) { throw "Poem text is wrong for $($case.id): $($row.poem_text_utf16)." }
        if($row.operation -ne $case.operation -or $row.target -ne $expectedTarget -or $row.selection_mode -ne $expectedSelection -or $row.check_allowed -ne '1' -or $row.before_equals_undo -ne '1' -or $row.after_equals_redo -ne '1' -or $row.sequential_cycle -ne '1' -or $row.empty_divs -ne '0' -or $row.empty_paragraphs -ne '0' -or $row.empty_stanzas -ne '0' -or $row.saved -ne '1' -or $row.result -ne 'pass') { throw "Undo/Redo contract failed for $($case.id): $($row | ConvertTo-Json -Compress)" }
        if($case.operation -eq 'cite' -and ([int]$row.after_cites -ne 1 -or [int]$row.after_poems -ne 0)) { throw "Cite structure is wrong for $($case.id)." }
        if($case.operation -eq 'poem' -and ([int]$row.after_poems -ne 1 -or [int]$row.after_stanzas -lt 1)) { throw "Poem structure is wrong for $($case.id)." }
        Assert-Fb2Schema $fixture
    }
    Write-Host 'Production Cite/Poem MSHTML Undo/Redo passed.'
}
finally {
    if($KeepArtifacts) { Write-Host "Artifacts: $directory" }
    else { Remove-Item -LiteralPath $directory -Recurse -Force -ErrorAction SilentlyContinue }
}
