<# Exercises title-to-title copy/paste through the production CFBEView command
   path.  The runtime harness records the raw MSHTML insertion separately, then
   verifies OnPaste normalization, undo/redo and the normal Save/Validate path. #>
[CmdletBinding()]
param(
    [string]$FbeExe = (Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe'),
    [ValidateRange(30, 300)][int]$TimeoutSeconds = 180,
    [switch]$KeepArtifacts
)

$ErrorActionPreference = 'Stop'
if(-not (Test-Path -LiteralPath $FbeExe -PathType Leaf)) { throw "FBE.exe not found: $FbeExe" }
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$scratch = New-IsolatedFbeScratchDirectory -Name 'clipboard-title-paste'
$directory = $scratch.Root
$completed = $false
$fixture = Join-Path $directory 'clipboard-title-paste.fb2'
$cases = @('text-only', 'text-paragraph-end', 'full-paragraph', 'multiple-title-paragraphs', 'full-title-empty-target', 'full-title-nonempty', 'ctrl-v-command')
try {
    New-Item -ItemType Directory -Path $directory -Force | Out-Null
    [IO.File]::WriteAllText($fixture, @'
<?xml version="1.0" encoding="utf-8"?>
<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0" xmlns:l="http://www.w3.org/1999/xlink"><description><title-info><genre>prose</genre><author><first-name>Runtime</first-name><last-name>Test</last-name></author><book-title>Clipboard title paste</book-title><lang>en</lang></title-info><document-info><author><first-name>Runtime</first-name><last-name>Test</last-name></author><program-used>test</program-used><date value="2026-09-28">28 September 2026</date><id>clipboard-title-paste-test</id><version>1.0</version></document-info></description><body><section id="title-source"><title><p>Source <strong>strong</strong> <emphasis>em</emphasis> <a l:href="#title-source">link</a> Z</p><p>Source second</p></title><p>Source body</p></section><section id="title-target"><title><p>Target existing</p></title><p>Target body</p></section><section id="title-target-empty"><title/><p>Empty target body</p></section><section id="title-neighbor"><title><p>Neighbor title</p></title><p>Neighbor body</p></section></body></FictionBook>
'@, [Text.UTF8Encoding]::new($false))
    $fixtureTemplate = [IO.File]::ReadAllText($fixture)
    $oldMode, $oldScenario = $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO
    try {
        $env:FBE_NEXT_TEST_MODE = '1'
        $env:FBE_NEXT_TEST_SCENARIO = 'clipboard-title-paste'
        $oldTrace, $oldCase = $env:FBE_NEXT_TEST_PASTE_TRACE, $env:FBE_NEXT_TEST_TITLE_PASTE_CASE
        $rows = @()
        foreach($case in $cases) {
            # Each run starts from the same valid FB2 fixture; Save/Undo therefore
            # prove the scenario independently rather than carrying DOM state.
            [IO.File]::WriteAllText($fixture, $fixtureTemplate, [Text.UTF8Encoding]::new($false))
            $report = Join-Path $directory ("clipboard-title-paste-$case.tsv")
            $trace = Join-Path $directory ("clipboard-title-paste-$case-after-idm-paste.html")
            $env:FBE_NEXT_TEST_TITLE_PASTE_CASE = $case
            $env:FBE_NEXT_TEST_PASTE_TRACE = $trace
            $process = Start-Process -FilePath $FbeExe -ArgumentList @('--portable', '-b', $report, $fixture) -WindowStyle Hidden -PassThru
            if(-not $process.WaitForExit($TimeoutSeconds * 1000)) { Stop-Process -Id $process.Id -Force; throw "FBE title paste runtime timed out: $case" }
            if($process.ExitCode -ne 0) { throw "FBE title paste runtime exited $($process.ExitCode): $case." }
            $result = @(Import-Csv -LiteralPath $report -Delimiter "`t")
            if($result.Count -ne 1 -or $result[0].case -ne $case -or $result[0].result -ne 'pass') { throw "Production title paste regression failed for ${case}: $($result | ConvertTo-Json -Compress)" }
            foreach($field in 'normalized_nested_title', 'normalized_direct_titles', 'text_order', 'formatting', 'neighbor_unchanged', 'undo', 'redo', 'validate', 'save') {
                if($result[0].$field -ne '0' -and $field -like 'normalized_*') { throw "Title paste left an invalid structure for ${case}: $field=$($result[0].$field)" }
                if($field -notlike 'normalized_*' -and $result[0].$field -ne '1') { throw "Title paste did not preserve $field for $case." }
            }
            if($case -in 'full-title-nonempty', 'ctrl-v-command') {
                if(-not (Test-Path -LiteralPath $trace -PathType Leaf)) { throw "Missing DOM snapshot immediately after IDM_PASTE for $case." }
                $rawDom = Get-Content -LiteralPath $trace -Raw
                if($rawDom -notmatch '(?is)<DIV\s+class=(?:["'']title["'']|title)[^>]*>.*<DIV\s+class=(?:["'']title["'']|title)') { throw "MSHTML title paste did not reproduce nested-title DOM for ${case}: $rawDom" }
            }
            $rows += $result[0]
        }
    }
    finally {
        $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO = $oldMode, $oldScenario
        $env:FBE_NEXT_TEST_PASTE_TRACE, $env:FBE_NEXT_TEST_TITLE_PASTE_CASE = $oldTrace, $oldCase
    }
    if($rows.Count -ne $cases.Count) { throw "Expected $($cases.Count) title paste results, got $($rows.Count)." }
    $completed = $true
    Write-Host "Clipboard title paste matrix passed: $($cases -join ', ')"
}
finally {
    Complete-IsolatedFbeRuntime -Isolation $scratch -Passed $completed -KeepArtifacts:$KeepArtifacts -PreserveContents
}
Write-Host 'Clipboard title paste production runtime passed.'
