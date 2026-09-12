<# Uses the production FBE MSHTML load/normalize/save route with loose inline
content, a BR and an empty paragraph.  It deliberately does not reuse a table
scenario: this test exercises CFBEView::Normalize and VisualDomNormalizer. #>
[CmdletBinding()]
param([string]$FbeExe = (Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe'), [int]$TimeoutSeconds = 180, [switch]$KeepArtifacts)
$ErrorActionPreference='Stop'
$directory = Join-Path ([IO.Path]::GetTempPath()) ('fbe-visual-dom-normalizer-' + [guid]::NewGuid().ToString('N'))
$fixture = Join-Path $directory 'normalizer.fb2'
$report = Join-Path $directory 'normalizer.tsv'
try {
    New-Item -ItemType Directory -Path $directory -Force | Out-Null
    @"
<?xml version="1.0" encoding="utf-8"?>
<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>Normalizer</book-title><lang>en</lang></title-info><document-info><program-used>test</program-used><id>normalizer-test</id><version>1.0</version></document-info></description><body><section><p>До</p><empty-line/><p>После</p></section></body></FictionBook>
"@ | Set-Content -LiteralPath $fixture -Encoding utf8
    $oldMode, $oldScenario = $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO
    $invokeNormalizer = {
        $process = Start-Process -FilePath $FbeExe -ArgumentList @('-b', $report, $fixture) -PassThru
        if(-not $process.WaitForExit($TimeoutSeconds * 1000)) { Stop-Process -Id $process.Id -Force; throw 'FBE timed out while normalizing the live DOM.' }
        if($process.ExitCode -ne 0) { throw "FBE normalizer scenario failed: exit $($process.ExitCode)." }
    }
    try {
        $env:FBE_NEXT_TEST_MODE = '1'; $env:FBE_NEXT_TEST_SCENARIO = 'visual-dom-normalizer'
        &$invokeNormalizer
    }
    finally { $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO = $oldMode, $oldScenario }
    $row = Import-Csv -LiteralPath $report -Delimiter "`t"
    if(@($row).Count -ne 5 -or @($row | Where-Object { $_.result -ne 'pass' }).Count -ne 0) { throw "Visual DOM normalizer runtime contract failed: $($row | ConvertTo-Json -Compress)" }
    $byCase = @{}; foreach($entry in $row) { $byCase[$entry.case] = $entry }
    foreach($name in 'single-br', 'double-br', 'empty-p', 'nbsp-p', 'formatted-br') { if(-not $byCase.ContainsKey($name)) { throw "Missing normalizer case: $name" } }
    if([int]$byCase['single-br'].paragraphs -ne 2 -or $byCase['single-br'].exact_paragraphs -ne '1') { throw 'A single BR did not become two ordered paragraphs.' }
    if([int]$byCase['double-br'].paragraphs -ne 3 -or $byCase['double-br'].empty_line -ne '1') { throw 'Two BRs did not preserve the intermediate empty line.' }
    if([int]$byCase['empty-p'].paragraphs -ne 3 -or $byCase['empty-p'].exact_paragraphs -ne '1') { throw 'An explicit empty paragraph was lost or reordered.' }
    if($byCase['nbsp-p'].nbsp -ne '1' -or $byCase['formatted-br'].formatting -ne '1') { throw 'NBSP or inline formatting around BR was not preserved.' }
    if(@($row | Where-Object { $_.empty_divs -ne '0' -or $_.brs -ne '0' }).Count -ne 0) { throw 'Normalizer left disposable DIVs or BRs in the resulting DOM.' }
    $saved = New-Object -ComObject Msxml2.DOMDocument.6.0
    if(-not $saved.load($fixture)) { throw "Saved normalization fixture is not XML: $($saved.parseError.reason)" }
    if($saved.selectNodes('//*[local-name()="table"]').length -ne 0) { throw 'Normalizer runtime fixture unexpectedly used a table.' }
    $section = $saved.selectSingleNode('/*[local-name()="FictionBook"]/*[local-name()="body"]/*[local-name()="section"]')
    if(-not $section -or $section.childNodes.length -ne 3 -or $section.childNodes.item(0).nodeName -ne 'p' -or $section.childNodes.item(1).nodeName -ne 'empty-line' -or $section.childNodes.item(2).nodeName -ne 'p' -or $section.childNodes.item(0).text -ne 'До' -or $section.childNodes.item(2).text -ne 'После') { throw 'Open → Save did not preserve the FB2 empty-line in its original position.' }
    try {
        $env:FBE_NEXT_TEST_MODE = '1'; $env:FBE_NEXT_TEST_SCENARIO = 'visual-dom-normalizer'
        &$invokeNormalizer
    }
    finally { $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO = $oldMode, $oldScenario }
    if(-not $saved.load($fixture)) { throw "Reopened normalization fixture is not XML: $($saved.parseError.reason)" }
    $section = $saved.selectSingleNode('/*[local-name()="FictionBook"]/*[local-name()="body"]/*[local-name()="section"]')
    if(-not $section -or $section.childNodes.length -ne 3 -or $section.childNodes.item(1).nodeName -ne 'empty-line') { throw 'Open → Save → Reopen lost or moved the FB2 empty-line.' }
}
finally { if($KeepArtifacts) { Write-Host "Artifacts: $directory" } else { Remove-Item -LiteralPath $directory -Recurse -Force -ErrorAction SilentlyContinue } }
Write-Host 'Visual DOM normalizer production runtime passed.'
