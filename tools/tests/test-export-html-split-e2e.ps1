<# Runs the production ExportHTML split path through FBE's headless host. #>
[CmdletBinding()]
param([string]$FbeExe = (Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe'), [int]$TimeoutSeconds = 90)
$ErrorActionPreference = 'Stop'
$FbeExe = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($FbeExe)
if (-not (Test-Path -LiteralPath $FbeExe)) { throw "Не найден FBE: $FbeExe" }
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$isolation = New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'export-html-split'
$FbeExe = $isolation.Exe
$directory = Join-Path $isolation.Root 'split-case'
$passed = $false
try {
    New-Item -ItemType Directory -Path $directory | Out-Null
    $png = 'iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVQIHWP4z8DwHwAFgAI/ScL1kQAAAABJRU5ErkJggg=='
    $fixture = Join-Path $directory 'split.fb2'
    @"
<?xml version="1.0" encoding="utf-8"?>
<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0" xmlns:l="http://www.w3.org/1999/xlink"><description><title-info><genre>prose</genre><book-title>Split</book-title><lang>en</lang></title-info></description><body><section id="first"><title><p>Глава / one &amp; # %</p></title><p><a l:href="#second">Next</a> <a type="note" l:href="#note">Note</a></p><image l:href="#picture.png"/></section><section id="second"><title><p>Глава / one &amp; # %</p></title><p><a l:href="#first">Back</a></p></section><section id="note"><p>Note text</p></section></body><binary id="picture.png" content-type="image/png">$png</binary></FictionBook>
"@ | Set-Content -LiteralPath $fixture -Encoding utf8
    $selected = Join-Path $directory 'chosen-name.html'
    $old = @($env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO, $env:FBE_NEXT_TEST_EXPORT_HTML_PATH, $env:FBE_NEXT_TEST_EXPORT_HTML_MODE, $env:FBE_NEXT_TEST_EXPORT_HTML_SPLIT)
    try {
        $env:FBE_NEXT_TEST_MODE='1'; $env:FBE_NEXT_TEST_SCENARIO='export-html'; $env:FBE_NEXT_TEST_EXPORT_HTML_PATH=$selected; $env:FBE_NEXT_TEST_EXPORT_HTML_MODE='2'; $env:FBE_NEXT_TEST_EXPORT_HTML_SPLIT='1'
        $report = Join-Path $directory 'split.tsv'; $process = Start-Process -FilePath $FbeExe -WorkingDirectory $isolation.Runtime -ArgumentList @('-b', ('"'+$report+'"'), '--portable', ('"'+$fixture+'"')) -PassThru
        $exitCode = Wait-IsolatedFbeProcess -Process $process -Isolation $isolation -Scenario 'export-html-split' -Report $report -TimeoutSeconds $TimeoutSeconds
        if ($exitCode -ne 0) { throw "FBE split export failed: $exitCode" }

    } finally { $env:FBE_NEXT_TEST_MODE,$env:FBE_NEXT_TEST_SCENARIO,$env:FBE_NEXT_TEST_EXPORT_HTML_PATH,$env:FBE_NEXT_TEST_EXPORT_HTML_MODE,$env:FBE_NEXT_TEST_EXPORT_HTML_SPLIT = $old }
    $index = Join-Path $directory 'index.html'; if (-not (Test-Path -LiteralPath $index)) { throw 'Split export did not create index.html.' }
    $pages = @((Get-ChildItem -LiteralPath $directory -Filter 'section-*.html').FullName); if ($pages.Count -ne 3) { throw "Expected three top-level section files, got $($pages.Count)." }
    if ((@($pages | Select-Object -Unique).Count -ne 3) -or $pages | Where-Object { $_ -match '\.\.' -or $_ -match '[\\/](?:\\|/)' }) { throw 'Split filenames are not unique and safe.' }
    $indexText = Get-Content -Raw -LiteralPath $index; if ($indexText -notmatch 'section-1-.*#_toc_' -or $indexText -match '<h2>Глава') { throw 'Index TOC or section removal regressed.' }
    $pageText = Get-Content -Raw -LiteralPath $pages[0]; if ($pageText -notmatch 'section-2-.*#second' -or $pageText -notmatch 'section-3-.*#note') { throw 'Cross-section or note links were not rewritten.' }
    $noteText = Get-Content -Raw -LiteralPath $pages[2]; if ($noteText -notmatch 'section-1-.*#_note_ref_') { throw 'Split note backlink was not rewritten.' }
    $images = Join-Path $directory 'index_files'; if (-not (Test-Path -LiteralPath (Join-Path $images 'picture.png'))) { throw 'Split export did not use one shared external image directory.' }
    if ($pages | Where-Object { [IO.Path]::GetFileName($_) -match '[#%&]' -or [IO.Path]::GetFileName($_) -match 'amp;' }) { throw 'Duplicate Unicode/special titles leaked into filenames.' }
    if ($pageText -notmatch 'index_files/picture.png') { throw 'Split page does not retain the shared image prefix.' }
    function Invoke-SplitExport([string]$Output, [int]$Mode, [int]$NotePlacement = 0, [string]$ConfirmOverwrite = "") {
        $saved = @($env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO, $env:FBE_NEXT_TEST_EXPORT_HTML_PATH, $env:FBE_NEXT_TEST_EXPORT_HTML_MODE, $env:FBE_NEXT_TEST_EXPORT_HTML_SPLIT, $env:FBE_NEXT_TEST_EXPORT_HTML_NOTE_PLACEMENT, $env:FBE_NEXT_TEST_EXPORT_HTML_CONFIRM_OVERWRITE)
        try {
            $env:FBE_NEXT_TEST_MODE='1'; $env:FBE_NEXT_TEST_SCENARIO='export-html'; $env:FBE_NEXT_TEST_EXPORT_HTML_PATH=$Output; $env:FBE_NEXT_TEST_EXPORT_HTML_MODE="$Mode"; $env:FBE_NEXT_TEST_EXPORT_HTML_SPLIT='1'; $env:FBE_NEXT_TEST_EXPORT_HTML_NOTE_PLACEMENT="$NotePlacement"; $env:FBE_NEXT_TEST_EXPORT_HTML_CONFIRM_OVERWRITE=$ConfirmOverwrite
            $report = Join-Path (Split-Path $Output) 'split.tsv'
            $process = Start-Process -FilePath $FbeExe -WorkingDirectory $isolation.Runtime -ArgumentList @('-b', ('"'+$report+'"'), '--portable', ('"'+$fixture+'"')) -PassThru
            $exitCode = Wait-IsolatedFbeProcess -Process $process -Isolation $isolation -Scenario 'export-html-split-mode' -Report $report -TimeoutSeconds $TimeoutSeconds
            if ($ConfirmOverwrite -ne "0" -and $exitCode -ne 0) { throw "Split export mode $Mode / notes $NotePlacement failed: $exitCode." }
        } finally { $env:FBE_NEXT_TEST_MODE,$env:FBE_NEXT_TEST_SCENARIO,$env:FBE_NEXT_TEST_EXPORT_HTML_PATH,$env:FBE_NEXT_TEST_EXPORT_HTML_MODE,$env:FBE_NEXT_TEST_EXPORT_HTML_SPLIT,$env:FBE_NEXT_TEST_EXPORT_HTML_NOTE_PLACEMENT,$env:FBE_NEXT_TEST_EXPORT_HTML_CONFIRM_OVERWRITE = $saved }
    }
    # A generated index is not silently overwritten when its confirmation is declined.
    [IO.File]::WriteAllText($index, 'original-index', [Text.UTF8Encoding]::new($false))
    Invoke-SplitExport $selected 2 0 '0'
    if ([IO.File]::ReadAllText($index, [Text.UTF8Encoding]::new($false)) -ne 'original-index') { throw 'Existing index.html was overwritten without confirmation.' }
    $htmlOnlyDirectory = Join-Path $directory 'html-only'; New-Item -ItemType Directory -Path $htmlOnlyDirectory | Out-Null
    Invoke-SplitExport (Join-Path $htmlOnlyDirectory 'chosen.html') 4
    $htmlOnlyIndex = Join-Path $htmlOnlyDirectory 'index.html'
    if (-not (Test-Path -LiteralPath $htmlOnlyIndex) -or (Get-Content -Raw -LiteralPath $htmlOnlyIndex) -match '<img\b') { throw 'Split HTML-without-images mode regressed.' }
    $bookEndDirectory = Join-Path $directory 'book-end'; New-Item -ItemType Directory -Path $bookEndDirectory | Out-Null
    Invoke-SplitExport (Join-Path $bookEndDirectory 'chosen.html') 2 1
    $bookEndIndex = Get-Content -Raw -LiteralPath (Join-Path $bookEndDirectory 'index.html')
    $bookEndFirst = Get-Content -Raw -LiteralPath ((Get-ChildItem -LiteralPath $bookEndDirectory -Filter 'section-1-*.html').FullName)
    if ($bookEndIndex -notmatch 'class="notes"' -or $bookEndFirst -notmatch 'index.html#note') { throw 'End-of-book split notes or links regressed.' }
    # End-of-section notes are descendants of their owning top-level section.
    $nestedFixture = Join-Path $directory 'split-nested-notes.fb2'
    @"
<?xml version="1.0" encoding="utf-8"?>
<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0" xmlns:l="http://www.w3.org/1999/xlink"><description><title-info><genre>prose</genre><book-title>Split notes</book-title><lang>en</lang></title-info></description><body><section id="first"><title><p>First</p></title><p><a type="note" l:href="#note">Note</a></p><section id="note"><p>Nested note</p></section></section><section id="second"><title><p>Second</p></title><p>Text</p></section></body></FictionBook>
"@ | Set-Content -LiteralPath $nestedFixture -Encoding utf8
    $fixture = $nestedFixture
    $sectionEndDirectory = Join-Path $directory 'section-end'; New-Item -ItemType Directory -Path $sectionEndDirectory | Out-Null
    Invoke-SplitExport (Join-Path $sectionEndDirectory 'chosen.html') 2 2
    $sectionEndFirst = Get-Content -Raw -LiteralPath ((Get-ChildItem -LiteralPath $sectionEndDirectory -Filter 'section-1-*.html').FullName)
    if ($sectionEndFirst -notmatch 'class="notes"') { throw 'End-of-section split notes regressed.' }
    $passed = $true
    Write-Host 'ExportHTML split production E2E passed.'
} finally {
    try {
        if (-not $passed -and (Test-Path -LiteralPath $directory)) {
            Get-ChildItem -LiteralPath $directory -Filter '*.tsv' -File -Recurse | ForEach-Object { Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $isolation.Root ('failure-' + $_.Directory.Name + '-' + $_.Name)) -Force }
        }
    } finally {
        Complete-IsolatedFbeRuntime -Isolation $isolation -Passed $passed
    }
}
