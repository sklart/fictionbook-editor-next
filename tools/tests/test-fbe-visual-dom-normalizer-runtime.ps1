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
<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>Normalizer</book-title><lang>en</lang></title-info><document-info><program-used>test</program-used><id>normalizer-test</id><version>1.0</version></document-info></description><body><section><p>Seed</p></section></body></FictionBook>
"@ | Set-Content -LiteralPath $fixture -Encoding utf8
    $oldMode, $oldScenario = $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO
    try {
        $env:FBE_NEXT_TEST_MODE = '1'; $env:FBE_NEXT_TEST_SCENARIO = 'visual-dom-normalizer'
        $process = Start-Process -FilePath $FbeExe -ArgumentList @('-b', $report, $fixture) -PassThru
        if(-not $process.WaitForExit($TimeoutSeconds * 1000)) { Stop-Process -Id $process.Id -Force; throw 'FBE timed out while normalizing the live DOM.' }
        if($process.ExitCode -ne 0) { throw "FBE normalizer scenario failed: exit $($process.ExitCode)." }
    }
    finally { $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO = $oldMode, $oldScenario }
    $row = Import-Csv -LiteralPath $report -Delimiter "`t"
    if(@($row).Count -ne 1 -or [int]$row.paragraphs -lt 1 -or $row.empty_divs -ne '0' -or $row.brs -ne '0' -or $row.text -ne '1' -or $row.result -ne 'pass') { throw "Visual DOM normalizer runtime contract failed: $($row | ConvertTo-Json -Compress)" }
    $saved = New-Object -ComObject Msxml2.DOMDocument.6.0
    if(-not $saved.load($fixture)) { throw "Saved normalization fixture is not XML: $($saved.parseError.reason)" }
    if($saved.selectNodes('//*[local-name()="table"]').length -ne 0) { throw 'Normalizer runtime fixture unexpectedly used a table.' }
}
finally { if($KeepArtifacts) { Write-Host "Artifacts: $directory" } else { Remove-Item -LiteralPath $directory -Recurse -Force -ErrorAction SilentlyContinue } }
Write-Host 'Visual DOM normalizer production runtime passed.'
