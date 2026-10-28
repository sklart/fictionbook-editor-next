[CmdletBinding()]
param(
    [string]$FbeExe = (Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe'),
    [ValidateRange(20, 120)][int]$TimeoutSeconds = 60,
    [string[]]$Cases = @('word', 'title', 'plain', 'plain-single', 'plain-middle', 'internal', 'internal-dirty', 'internal-history', 'internal-history-middle', 'internal-end', 'internal-missing-link', 'internal-missing-image', 'legacy-unmarked', 'queue-reload', 'queue-reload-idle', 'queue-moved-selection', 'queue-capture-after-post', 'queue-capture-notice', 'queue-close-retry', 'queue-post-idle', 'queue-post-failure', 'queue-post-clean-failure', 'queue-failure-retry', 'queue-two', 'queue-three', 'queue-reentrant', 'queue-reentrant-post-failure', 'queue-plain-two'),
    [switch]$KeepArtifacts,
    [switch]$RealPaste
)

$ErrorActionPreference = 'Stop'
if(-not (Test-Path -LiteralPath $FbeExe -PathType Leaf)) { throw "FBE.exe not found: $FbeExe" }
$directory = Join-Path ([IO.Path]::GetTempPath()) ('fbe-html-paste-' + [guid]::NewGuid().ToString('N'))
$fixture = Join-Path $directory 'html-paste.fb2'
$oldRoute = $env:FBE_NEXT_TEST_HTML_REAL_PASTE
$oldMode, $oldScenario, $oldCase = $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO, $env:FBE_NEXT_TEST_HTML_PASTE_CASE
try {
    New-Item -ItemType Directory -Path $directory | Out-Null
    $xml = '<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><first-name>Paste</first-name><last-name>Test</last-name></author><book-title>HTML paste</book-title><lang>en</lang></title-info><document-info><author><first-name>Paste</first-name><last-name>Test</last-name></author><date value="2026-10-15">2026-10-15</date><id>html-paste-test</id><version>1.0</version></document-info></description><body><section id="paste-target"><title><p>Heading</p></title><p>Before</p></section><section id="paste-neighbor"><p>Neighbor</p></section></body></FictionBook>'
    $env:FBE_NEXT_TEST_MODE = '1'
    $env:FBE_NEXT_TEST_SCENARIO = 'html-paste-runtime'
    $env:FBE_NEXT_TEST_HTML_REAL_PASTE = if ($RealPaste) { '1' } else { $null }
    foreach($case in $Cases) {
        [IO.File]::WriteAllText($fixture, $xml, [Text.UTF8Encoding]::new($false))
        $report = Join-Path $directory "$case.tsv"
        $env:FBE_NEXT_TEST_HTML_PASTE_CASE = $case
        $process = Start-Process -FilePath $FbeExe -ArgumentList @('--portable', '-b', $report, $fixture) -WindowStyle Hidden -PassThru
        if(-not $process.WaitForExit($TimeoutSeconds * 1000)) { Stop-Process -Id $process.Id -Force; throw "HTML paste runtime timed out: $case" }
        if($process.ExitCode -ne 0) { throw "HTML paste runtime exited $($process.ExitCode): $case; report: $(Get-Content $report -Raw)" }
        $row = @(Import-Csv -LiteralPath $report -Delimiter "`t")
        if($row.Count -ne 1 -or $row[0].result -ne 'pass') { throw "HTML paste runtime failed: $case; report: $(Get-Content $report -Raw)" }
        [xml]$saved = Get-Content -LiteralPath $fixture -Raw
        $savedXml = $saved.OuterXml
        if($case -like 'queue-*') {
            if($case -notin @('queue-reload', 'queue-reload-idle', 'queue-moved-selection', 'queue-capture-after-post', 'queue-capture-notice', 'queue-close-retry', 'queue-post-idle', 'queue-post-failure','queue-post-clean-failure') -and $savedXml -notmatch 'Queue1') { throw "Queued text missing after save: $case" }
            if($savedXml -match 'Queue9|Queue7') { throw "Rejected or newer clipboard inserted: $case" }
        }
        elseif($case -in @('internal-missing-link', 'internal-missing-image')) {
            if($savedXml -match 'Missing note|absent-image' -or $savedXml -notmatch 'Before') { throw "Unsafe internal fragment changed the saved document: $case" }
        }
        elseif($case -eq 'legacy-unmarked') {
            if($savedXml -match 'Legacy link' -or $savedXml -notmatch 'Before') { throw 'Unmarked FBE HTML changed the saved document.' }
        }
        elseif($case -in @('internal', 'internal-dirty', 'internal-history', 'internal-history-middle', 'internal-end')) {
            if($savedXml -notmatch 'Internal code' -or $savedXml -notmatch 'internal link') { throw "Internal fragment missing after save: $case" }
        }
        elseif($case -eq 'plain-single') {
            if($savedXml -notmatch 'Ж' -or $savedXml -notmatch 'Ω') { throw "Single-line Unicode missing after save: $case" }
        }
        elseif($case -in @('plain', 'plain-middle')) {
            if($savedXml -notmatch 'Plain one' -or $savedXml -notmatch 'Plain three') { throw "Plain text missing after save: $case" }
        }
        elseif($savedXml -notmatch 'Bold italic' -or $savedXml -notmatch 'Second') { throw "Formatted text missing after save: $case" }
    }
    Write-Host "HTML paste runtime passed: $($Cases -join ', '); FB2 parse."
}
finally {
    $env:FBE_NEXT_TEST_HTML_REAL_PASTE = $oldRoute
    $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO, $env:FBE_NEXT_TEST_HTML_PASTE_CASE = $oldMode, $oldScenario, $oldCase
    if($KeepArtifacts) { Write-Host "Artifacts: $directory" }
    else { Remove-Item -LiteralPath $directory -Recurse -Force -ErrorAction SilentlyContinue }
}
