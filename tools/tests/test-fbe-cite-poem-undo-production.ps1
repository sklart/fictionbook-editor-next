<#
.SYNOPSIS
Exercises InsertCite and InsertPoem in the live MSHTML editor and verifies
one-step Undo/Redo DOM snapshots plus a save after the final Undo.
#>
[CmdletBinding()]
param(
    [string]$FbeExe = (Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe'),
    [int]$TimeoutSeconds = 90,
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
        @{ id = 'poem-one'; operation = 'poem'; paragraphs = @('Line') },
        @{ id = 'poem-lines'; operation = 'poem'; paragraphs = @('First line', 'Second line') },
        @{ id = 'poem-stanzas'; operation = 'poem'; paragraphs = @('One', '', 'Two', 'Three') }
    )
    foreach($case in $cases) {
        $paragraphs = ($case.paragraphs | ForEach-Object { "<p>$([Security.SecurityElement]::Escape($_))</p>" }) -join ''
        $fixture = Join-Path $directory ($case.id + '.fb2')
        $report = Join-Path $directory ($case.id + '.tsv')
        @("<?xml version=`"1.0`" encoding=`"utf-8`"?>", "<FictionBook xmlns=`"http://www.gribuser.ru/xml/fictionbook/2.0`"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>$($case.id)</book-title><lang>en</lang></title-info><document-info><program-used>test</program-used><id>$($case.id)</id><version>1.0</version></document-info></description><body><section>$paragraphs</section></body></FictionBook>") | Set-Content -LiteralPath $fixture -Encoding utf8
        $oldMode, $oldScenario, $oldOperation = $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO, $env:FBE_NEXT_TEST_STRUCTURE_OPERATION
        try {
            $env:FBE_NEXT_TEST_MODE = '1'; $env:FBE_NEXT_TEST_SCENARIO = 'cite-poem-undo'; $env:FBE_NEXT_TEST_STRUCTURE_OPERATION = $case.operation
            $process = Start-Process -FilePath $FbeExe -ArgumentList @('-b', $report, $fixture) -PassThru
            if(-not $process.WaitForExit($TimeoutSeconds * 1000)) { Stop-Process -Id $process.Id -Force; throw "FBE timed out for $($case.id)." }
            if($process.ExitCode -ne 0) { throw "FBE failed for $($case.id): exit $($process.ExitCode)." }
        }
        finally {
            $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO, $env:FBE_NEXT_TEST_STRUCTURE_OPERATION = $oldMode, $oldScenario, $oldOperation
        }
        $row = Import-Csv -LiteralPath $report -Delimiter "`t"
        if(@($row).Count -ne 1) { throw "Missing live MSHTML report for $($case.id)." }
        if($row.operation -ne $case.operation -or $row.check_allowed -ne '1' -or $row.before_equals_undo -ne '1' -or $row.after_equals_redo -ne '1' -or $row.undo_empty_divs -ne '0' -or $row.saved -ne '1' -or $row.result -ne 'pass') { throw "Undo/Redo contract failed for $($case.id): $($row | ConvertTo-Json -Compress)" }
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
